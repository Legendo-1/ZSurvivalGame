// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Dodge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZSurvivalGame.h"

UZGameplayAbility_Dodge::UZGameplayAbility_Dodge()
{
	InputID = EZAbilityInputID::Dodge;
	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Dodge.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Dodging);

	// a dodge interrupts a reload
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability_Reload);
}

bool UZGameplayAbility_Dodge::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->GetNumericAttribute(UZAttributeSet::GetStaminaAttribute()) < StaminaCost)
	{
		return false;
	}

	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UZGameplayAbility_Dodge::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	// stamina only changes on the server; the new value replicates to the player
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (ASC && HasAuthority(&ActivationInfo))
	{
		const FGameplayAttribute StaminaAttribute = UZAttributeSet::GetStaminaAttribute();
		ASC->SetNumericAttributeBase(StaminaAttribute, ASC->GetNumericAttributeBase(StaminaAttribute) - StaminaCost);
	}
}

void UZGameplayAbility_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const ACharacter* Character = ActorInfo ? Cast<ACharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// dodge in the movement input direction (it reaches the server with every move), or forward when standing still
	FVector Direction = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector().GetSafeNormal2D();
	}

	// the player's dodge distance (traits) makes it faster over the same time; both machines have the same value
	const float Speed = DodgeSpeed * UZAttributeSet::GetAttributeValue(Character, UZAttributeSet::GetDodgeDistanceMultiplierAttribute(), 1.f);
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' dodging at %.0f cm/s for %.2f s"), *GetNameSafe(Character), Speed, DodgeDuration);

	// root motion runs on the client and the server in sync, so the server doesn't pull the player back
	UAbilityTask_ApplyRootMotionConstantForce* DodgeTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, NAME_None, Direction, Speed, DodgeDuration, false, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, ExitSpeed, true);
	DodgeTask->OnFinish.AddDynamic(this, &UZGameplayAbility_Dodge::OnDodgeFinished);
	DodgeTask->ReadyForActivation();

	// TODO Phase 1 Step 2: Play Dodge Roll Animation Montage here when ready
}

void UZGameplayAbility_Dodge::OnDodgeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
