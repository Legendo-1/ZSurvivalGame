// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAIBaseCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
#include "Combat/ZMeleeTraceComponent.h"
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

	MeleeTraceComponent = CreateDefaultSubobject<UZMeleeTraceComponent>(TEXT("MeleeTrace"));

	// one plain attack until an enemy type sets its own (a hit 1.2 m in front, 0.4 s in)
	Attacks.AddDefaulted();

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
	CreateAttachedMeshes();

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

void AZAIBaseCharacter::CreateAttachedMeshes()
{
	// looks only: every machine makes the same ones from the class defaults, so there's nothing to replicate
	for (const FZAttachedMesh& Attached : AttachedMeshes)
	{
		if (!Attached.Mesh)
		{
			continue;
		}

		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetStaticMesh(Attached.Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetupAttachment(GetMesh(), Attached.Socket);
		Component->SetRelativeTransform(Attached.Offset);
		Component->RegisterComponent();
		AttachedMeshComponents.Add(Component);
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

TArray<FZAttackOption> AZAIBaseCharacter::MakeAttackOptions() const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	TArray<FZAttackOption> Options;
	Options.Reserve(Attacks.Num());
	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		FZAttackOption& Option = Options.AddDefaulted_GetRef();
		Option.MinRange = Attacks[Index].MinRange;
		Option.MaxRange = Attacks[Index].MaxRange;
		Option.Weight = Attacks[Index].Weight;
		Option.bReady = !AttackReadyTimes.IsValidIndex(Index) || Now >= AttackReadyTimes[Index];
	}

	return Options;
}

bool AZAIBaseCharacter::IsAttackReady() const
{
	if (bIsDead || !AttackAbility || !GetWorld() || GetWorld()->GetTimeSeconds() - LastAttackTime < AttackCooldown)
	{
		return false;
	}

	return MakeAttackOptions().ContainsByPredicate([](const FZAttackOption& Option) { return Option.bReady && Option.Weight > 0.f; });
}

namespace
{
	/** The attack can still be used from this distance: ready, and the target isn't too close for it */
	bool IsPlanStillGood(const FZAttackOption& Option, float DistanceToTarget)
	{
		return Option.bReady && Option.Weight > 0.f && DistanceToTarget >= Option.MinRange;
	}
}

void AZAIBaseCharacter::UpdateAttackPlan(float DistanceToTarget)
{
	if (!HasAuthority())
	{
		return;
	}

	// it sticks to its plan (so it doesn't keep changing its mind while it walks in), until that attack can't be used
	const TArray<FZAttackOption> Options = MakeAttackOptions();
	if (Options.IsValidIndex(PlannedAttackIndex) && IsPlanStillGood(Options[PlannedAttackIndex], DistanceToTarget))
	{
		return;
	}

	PlannedAttackIndex = ZAICombatRules::PlanAttack(Options, DistanceToTarget, FMath::FRand());
}

float AZAIBaseCharacter::GetApproachRange(float DistanceToTarget) const
{
	const TArray<FZAttackOption> Options = MakeAttackOptions();
	return Options.IsValidIndex(PlannedAttackIndex) ? ZAICombatRules::GetApproachRangeFor(Options[PlannedAttackIndex], DistanceToTarget)
		: ZAICombatRules::GetApproachRange(Options, DistanceToTarget);
}

bool AZAIBaseCharacter::IsPlayingAttackAnimation() const
{
	const FZEnemyAttack* Attack = GetCurrentAttack();
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	return Attack && Attack->Montage && AnimInstance && AnimInstance->Montage_IsPlaying(Attack->Montage);
}

bool AZAIBaseCharacter::GetMeleeBlade(FZMeleeBlade& OutBlade) const
{
	for (int32 Index = 0; Index < AttachedMeshes.Num(); ++Index)
	{
		const UStaticMeshComponent* Component = AttachedMeshComponents.IsValidIndex(Index) ? AttachedMeshComponents[Index].Get() : nullptr;
		const UStaticMesh* StaticMesh = Component ? Component->GetStaticMesh() : nullptr;
		if (!AttachedMeshes[Index].bIsWeapon || !StaticMesh)
		{
			continue;
		}

		const float Scale = Component->GetComponentScale().GetAbsMax();
		OutBlade = FZMeleeBlade();
		OutBlade.Mesh = const_cast<UStaticMeshComponent*>(Component);

		// its own sockets mark the part that hits
		static const FName BladeStart(TEXT("BladeStart"));
		static const FName BladeEnd(TEXT("BladeEnd"));
		if (Component->DoesSocketExist(BladeStart) && Component->DoesSocketExist(BladeEnd))
		{
			OutBlade.StartSocket = BladeStart;
			OutBlade.EndSocket = BladeEnd;
			OutBlade.Radius = (AttachedMeshes[Index].BladeRadius > 0.f ? AttachedMeshes[Index].BladeRadius : 10.f) * Scale;
			return true;
		}

		// otherwise the whole mesh: a line through the middle of its box along its longest side, as thick as the box's next-largest side
		const FBox Bounds = StaticMesh->GetBoundingBox();
		const FVector Extent = Bounds.GetExtent();
		const int32 LongAxis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? 0 : (Extent.Y >= Extent.Z ? 1 : 2);
		const float Thickness = AttachedMeshes[Index].BladeRadius > 0.f ? AttachedMeshes[Index].BladeRadius
			: FMath::Max(LongAxis == 0 ? 0.f : Extent.X, FMath::Max(LongAxis == 1 ? 0.f : Extent.Y, LongAxis == 2 ? 0.f : Extent.Z));

		// the balls reach Thickness past the line's ends: stop the line that much short of the box's ends
		FVector HalfLine = FVector::ZeroVector;
		HalfLine[LongAxis] = FMath::Max(Extent[LongAxis] - Thickness, 0.f);
		OutBlade.StartOffset = Bounds.GetCenter() - HalfLine;
		OutBlade.EndOffset = Bounds.GetCenter() + HalfLine;
		OutBlade.Radius = Thickness * Scale;
		return true;
	}

	return false;
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

bool AZAIBaseCharacter::TryAttack(float DistanceToTarget)
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

	// the attack it planned if it fits now, otherwise one of the ready attacks that fit, by weight
	const TArray<FZAttackOption> Options = MakeAttackOptions();
	const bool bPlanFits = Options.IsValidIndex(PlannedAttackIndex) && IsPlanStillGood(Options[PlannedAttackIndex], DistanceToTarget)
		&& DistanceToTarget <= Options[PlannedAttackIndex].MaxRange;
	const int32 AttackIndex = bPlanFits ? PlannedAttackIndex : ZAICombatRules::ChooseAttack(Options, DistanceToTarget, FMath::FRand());
	if (AttackIndex == INDEX_NONE)
	{
		return false;
	}

	// the attack ability plays the attack it finds here
	const int32 PreviousAttackIndex = CurrentAttackIndex;
	CurrentAttackIndex = AttackIndex;
	if (!AbilitySystemComponent->TryActivateAbilityByClass(AttackAbility))
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' couldn't start attack %d: its attack ability didn't activate"), *GetNameSafe(this), AttackIndex + 1);
		CurrentAttackIndex = PreviousAttackIndex;
		return false;
	}

	// the next decision plans its next attack
	LastAttackTime = Now;
	PlannedAttackIndex = INDEX_NONE;
	AttackReadyTimes.SetNumZeroed(Attacks.Num());
	AttackReadyTimes[AttackIndex] = Now + Attacks[AttackIndex].Cooldown;
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' attacks: attack %d '%s' from %.0f cm"), *GetNameSafe(this), AttackIndex + 1, *GetNameSafe(Attacks[AttackIndex].Montage), DistanceToTarget);
	return true;
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
	// stop moving and fall over as a ragdoll; a swing it was making hits nothing more
	MeleeTraceComponent->EndAllHitboxes();
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
