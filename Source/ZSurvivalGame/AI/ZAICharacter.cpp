// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAICharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AI/ZAIController.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_MeleeAttack.h"

AZAICharacter::AZAICharacter()
{
	// the ability system lives on the character itself (AI has no PlayerState); Minimal: only tags and cues replicate
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UZAttributeSet>(TEXT("AttributeSet"));

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

	AttackAbility = UZGameplayAbility_MeleeAttack::StaticClass();
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

void AZAICharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (!HasAuthority())
	{
		return;
	}

	// health from this enemy type (max first, so the health isn't capped at the old max)
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	AbilitySystemComponent->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), MaxHealth);
	AttributeSet->OnOutOfHealth.AddUObject(this, &AZAICharacter::HandleOutOfHealth);

	if (AttackAbility)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AttackAbility, 1, INDEX_NONE, this));
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

	// health only changes on the server (UZAttributeSet keeps it in range)
	if (HasAuthority() && ActualDamage > 0.f && !bIsDead)
	{
		const FGameplayAttribute HealthAttribute = UZAttributeSet::GetHealthAttribute();
		AbilitySystemComponent->SetNumericAttributeBase(HealthAttribute, AbilitySystemComponent->GetNumericAttributeBase(HealthAttribute) - ActualDamage);
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

	// State.Dead blocks every ability; it replicates so clients know too
	AbilitySystemComponent->CancelAllAbilities();
	AbilitySystemComponent->AddLooseGameplayTag(ZGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);

	PlayDeath();
	BP_OnDied(DamageInstigator);

	// stop the AI and remove the body after a while
	DetachFromControllerPendingDestroy();
	SetLifeSpan(DeadBodyLifeSpan);
}

void AZAICharacter::PlayDeath()
{
	// stop moving and fall over as a ragdoll
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
}

void AZAICharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		PlayDeath();
	}
}
