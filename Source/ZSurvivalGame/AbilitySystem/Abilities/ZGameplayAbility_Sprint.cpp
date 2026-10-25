// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Sprint.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZCharacterMovementComponent.h"

UZGameplayAbility_Sprint::UZGameplayAbility_Sprint()
{
	InputID = EZAbilityInputID::Sprint;
	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Sprint.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Sprinting);

	// can't sprint while aiming or firing
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Aiming);
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Firing);
}

bool UZGameplayAbility_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// not while a speed limit is active (weapon swap, reload, ...)
	const UZCharacterMovementComponent* Movement = GetZMovement(ActorInfo);
	if (!Movement || Movement->GetSpeedLimit() != EZSpeedLimit::None)
	{
		return false;
	}

	// Minimum stamina needed to initiate sprint
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return ASC && ASC->GetNumericAttribute(UZAttributeSet::GetStaminaAttribute()) > MinStaminaToStart;
}

void UZGameplayAbility_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// runs on the owning client and on the server; the client also sends it with every move
	if (UZCharacterMovementComponent* Movement = GetZMovement(ActorInfo))
	{
		Movement->SetWantsToSprint(true);
	}

	// sprint until the key is released
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitRelease->OnRelease.AddDynamic(this, &UZGameplayAbility_Sprint::OnInputReleased);
	WaitRelease->ReadyForActivation();
}

void UZGameplayAbility_Sprint::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UZCharacterMovementComponent* Movement = GetZMovement(ActorInfo))
	{
		Movement->SetWantsToSprint(false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZGameplayAbility_Sprint::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
