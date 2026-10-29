// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAIBaseCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AI/ZAIBaseController.h"
#include "AbilitySystem/ZAbilitySystemComponent.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_MeleeAttack.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Stagger.h"
#include "Audio/ZFootstepComponent.h"
#include "Combat/ZCharacterSpacingComponent.h"
#include "Combat/ZHitFlashComponent.h"
#include "Combat/ZLockOnTargetComponent.h"
#include "Items/ZLootTable.h"
#include "Items/ZPickup.h"
#include "Progression/ZProgressionComponent.h"

AZAIBaseCharacter::AZAIBaseCharacter()
{
	// the ability system lives on the character itself (AI has no PlayerState); Minimal: only tags and cues replicate
	AbilitySystemComponent = CreateDefaultSubobject<UZAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UZAttributeSet>(TEXT("AttributeSet"));

	FootstepComponent = CreateDefaultSubobject<UZFootstepComponent>(TEXT("Footsteps"));

	// players lock on to its chest: on the capsule rather than a bone, so the camera follows it smoothly
	LockOnTarget = CreateDefaultSubobject<UZLockOnTargetComponent>(TEXT("LockOnTarget"));
	LockOnTarget->SetupAttachment(GetCapsuleComponent());
	LockOnTarget->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	HitFlashComponent = CreateDefaultSubobject<UZHitFlashComponent>(TEXT("HitFlash"));

	SpacingComponent = CreateDefaultSubobject<UZCharacterSpacingComponent>(TEXT("Spacing"));
	SpacingComponent->SetupAttachment(GetCapsuleComponent());

	// AI characters don't need as many updates as players
	SetNetUpdateFrequency(30.f);

	AIControllerClass = AZAIBaseController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Weapon shots pass the capsule and hit the mesh's physics bodies, so a hit knows the bone (e.g. for headshots)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Block);

	// turn toward the target (the AI controller's focus), also while standing still to attack
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 360.f, 0.f);
	GetCharacterMovement()->MaxWalkSpeed = 300.f;

	// path following moves it through acceleration, like a player's input: anim blueprints only play walking while accelerating,
	// and without it the character would glide in its idle pose (and make no footsteps)
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;

	AttackAbility = UZGameplayAbility_MeleeAttack::StaticClass();
	StaggerAbility = UZGameplayAbility_Stagger::StaticClass();
}

void AZAIBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZAIBaseCharacter, bIsDead);

	// for animations and effects on every machine
	DOREPLIFETIME(AZAIBaseCharacter, CombatState);
	DOREPLIFETIME(AZAIBaseCharacter, CombatTarget);
}

UAbilitySystemComponent* AZAIBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGenericTeamId AZAIBaseCharacter::GetGenericTeamId() const
{
	return FGenericTeamId(static_cast<uint8>(Team));
}

void AZAIBaseCharacter::HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	// the hit flash comes with its Hit Cue, which the server sends to everyone with every hit
	if (EventType == EGameplayCueEvent::Executed && CharacterCues.HitCue.IsValid() && GameplayCueTag == CharacterCues.HitCue)
	{
		HitFlashComponent->Flash();
	}

	// the default: Blueprint functions named after the tag
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void AZAIBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// the AI chases at this speed and slows down for other things (strafing)
	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	if (!HasAuthority())
	{
		return;
	}

	// health and poise from this enemy type (max first, so the value isn't capped at the old max)
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetMaxPoiseAttribute(), MaxPoise);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetPoiseAttribute(), MaxPoise);
	AttributeSet->OnOutOfHealth.AddUObject(this, &AZAIBaseCharacter::HandleOutOfHealth);
	AttributeSet->OnPoiseBroken.AddUObject(this, &AZAIBaseCharacter::HandlePoiseBroken);

	if (AttackAbility)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AttackAbility, 1, INDEX_NONE, this));
	}

	if (StaggerAbility)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(StaggerAbility, 1, INDEX_NONE, this));
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (AbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
		}
	}
}

void AZAIBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// refresh, so the ability system knows the controller
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

float AZAIBaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// health only changes on the server (UZAttributeSet keeps it in range); armor lowers it, as for every other damage
	if (HasAuthority() && ActualDamage > 0.f && !bIsDead)
	{
		const FGameplayAttribute HealthAttribute = UZAttributeSet::GetHealthAttribute();
		const float DamageTaken = UZAttributeSet::GetDamageAfterArmor(ActualDamage, AbilitySystemComponent->GetNumericAttribute(UZAttributeSet::GetArmorAttribute()));
		AbilitySystemComponent->SetNumericAttributeBase(HealthAttribute, AbilitySystemComponent->GetNumericAttributeBase(HealthAttribute) - DamageTaken);
	}

	return ActualDamage;
}

bool AZAIBaseCharacter::IsAttackReady() const
{
	return !bIsDead && AttackAbility && GetWorld() && GetWorld()->GetTimeSeconds() - LastAttackTime >= AttackCooldown;
}

bool AZAIBaseCharacter::IsPlayingAttackAnimation() const
{
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return AttackMontage && AnimInstance && AnimInstance->Montage_IsPlaying(AttackMontage);
}

void AZAIBaseCharacter::SetCombatState(EZAICombatState NewState)
{
	if (!HasAuthority() || NewState == CombatState)
	{
		return;
	}

	const EZAICombatState OldState = CombatState;
	CombatState = NewState;

	// the server doesn't get the OnRep
	HandleCombatStateChanged(OldState);
}

void AZAIBaseCharacter::SetCombatSettings(const FZAICombatSettings& NewSettings)
{
	if (!HasAuthority())
	{
		return;
	}

	CombatSettings = NewSettings;
	if (AZAIBaseController* AIController = Cast<AZAIBaseController>(GetController()))
	{
		AIController->RefreshCombatSettings();
	}
}

void AZAIBaseCharacter::SetCombatTarget(AActor* NewTarget)
{
	if (HasAuthority())
	{
		CombatTarget = NewTarget;
	}
}

void AZAIBaseCharacter::OnRep_CombatState(EZAICombatState OldState)
{
	HandleCombatStateChanged(OldState);
}

void AZAIBaseCharacter::HandleCombatStateChanged(EZAICombatState OldState)
{
	if (!HasAuthority())
	{
		UE_LOG(LogZSurvivalGame, VeryVerbose, TEXT("'%s' combat state: %s -> %s (replicated)"), *GetNameSafe(this), LexToString(OldState), LexToString(CombatState));
	}

	OnCombatStateChanged.Broadcast(CombatState, OldState);
	BP_OnCombatStateChanged(CombatState, OldState);
}

bool AZAIBaseCharacter::TryAttack()
{
	if (bIsDead || !AttackAbility || !HasAuthority())
	{
		return false;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return false;
	}

	if (AbilitySystemComponent->TryActivateAbilityByClass(AttackAbility))
	{
		LastAttackTime = Now;
		return true;
	}

	return false;
}

void AZAIBaseCharacter::HandleOutOfHealth(AActor* DamageInstigator)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	UE_LOG(LogZSurvivalGame, Log, TEXT("AI character '%s' died (killed by '%s')"), *GetNameSafe(this), *GetNameSafe(DamageInstigator));

	// death sound and effects for everyone
	FGameplayCueParameters DeathCue;
	DeathCue.Instigator = DamageInstigator;
	DeathCue.EffectCauser = DamageInstigator;
	UZGameplayCueLibrary::ExecuteCue(this, CharacterCues.DeathCue, DeathCue);

	// State.Dead blocks every ability; it replicates so clients know too
	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->AddLooseGameplayTag(ZGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);

	PlayDeath();

	if (LootTable)
	{
		LootTable->SpawnLoot(GetWorld(), GetActorLocation(), LootPickupClass, LootLifeSpan);
	}

	// every player gets the experience and trait points, not only the one who killed it
	if (const AGameStateBase* GameState = GetWorld()->GetGameState())
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(Player))
			{
				Progression->AddExperience(ExperienceReward);
				Progression->AddTraitPoints(TraitPointReward);
			}
		}
	}

	BP_OnDied(DamageInstigator);

	// stop the AI and remove the body after a while
	DetachFromControllerPendingDestroy();
	SetLifeSpan(DeadBodyLifeSpan);
}

void AZAIBaseCharacter::HandlePoiseBroken(AActor* StaggerInstigator)
{
	if (!bIsDead)
	{
		UZCombatLibrary::Stagger(this, StaggerInstigator);
	}
}

void AZAIBaseCharacter::PlayDeath()
{
	// stop moving and fall over as a ragdoll
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpacingComponent->SetSpacingEnabled(false);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	// the Ragdoll profile blocks every channel it doesn't name: let shots pass through the body so it can't shield the living
	GetMesh()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Ignore);
}

void AZAIBaseCharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		PlayDeath();
	}
}
