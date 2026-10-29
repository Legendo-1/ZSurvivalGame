// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAICharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AI/ZAIController.h"
#include "AbilitySystem/ZAbilitySystemComponent.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_MeleeAttack.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Stagger.h"
#include "Audio/ZFootstepComponent.h"
#include "Combat/ZHitFlashComponent.h"
#include "Combat/ZLockOnTargetComponent.h"
#include "Items/ZLootTable.h"
#include "Items/ZPickup.h"
#include "Progression/ZProgressionComponent.h"

AZAICharacter::AZAICharacter()
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

	// AI characters don't need as many updates as players
	SetNetUpdateFrequency(30.f);

	AIControllerClass = AZAIController::StaticClass();
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

void AZAICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZAICharacter, bIsDead);
}

UAbilitySystemComponent* AZAICharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGenericTeamId AZAICharacter::GetGenericTeamId() const
{
	return FGenericTeamId(static_cast<uint8>(Team));
}

void AZAICharacter::HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	// the hit flash comes with its Hit Cue, which the server sends to everyone with every hit
	if (EventType == EGameplayCueEvent::Executed && CharacterCues.HitCue.IsValid() && GameplayCueTag == CharacterCues.HitCue)
	{
		HitFlashComponent->Flash();
	}

	// the default: Blueprint functions named after the tag
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void AZAICharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!HasAuthority())
	{
		return;
	}

	// health and poise from this enemy type (max first, so the value isn't capped at the old max)
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetMaxPoiseAttribute(), MaxPoise);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetPoiseAttribute(), MaxPoise);
	AttributeSet->OnOutOfHealth.AddUObject(this, &AZAICharacter::HandleOutOfHealth);
	AttributeSet->OnPoiseBroken.AddUObject(this, &AZAICharacter::HandlePoiseBroken);

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

void AZAICharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// refresh, so the ability system knows the controller
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

float AZAICharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
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

bool AZAICharacter::TryAttack()
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

void AZAICharacter::HandleOutOfHealth(AActor* DamageInstigator)
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

void AZAICharacter::HandlePoiseBroken(AActor* StaggerInstigator)
{
	if (!bIsDead)
	{
		UZCombatLibrary::Stagger(this, StaggerInstigator);
	}
}

void AZAICharacter::PlayDeath()
{
	// stop moving and fall over as a ragdoll
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	// the Ragdoll profile blocks every channel it doesn't name: let shots pass through the body so it can't shield the living
	GetMesh()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Ignore);
}

void AZAICharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		PlayDeath();
	}
}
