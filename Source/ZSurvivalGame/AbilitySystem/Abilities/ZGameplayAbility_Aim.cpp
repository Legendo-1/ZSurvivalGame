// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Aim.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZCharacterMovementComponent.h"

UZGameplayAbility_Aim::UZGameplayAbility_Aim()
{
	InputID = EZAbilityInputID::Aim;
	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Aim.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Aiming);

	// Aiming cancels sprint
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability_Sprint);
}

void UZGameplayAbility_Aim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// runs on the owning client and on the server; the client also sends it with every move
	if (UZCharacterMovementComponent* Movement = GetZMovement(ActorInfo))
	{
		Movement->SetWantsToAim(true);
	}

	// aim until the key is released
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitRelease->OnRelease.AddDynamic(this, &UZGameplayAbility_Aim::OnInputReleased);
	WaitRelease->ReadyForActivation();
}

void UZGameplayAbility_Aim::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UZCharacterMovementComponent* Movement = GetZMovement(ActorInfo))
	{
		Movement->SetWantsToAim(false);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UZGameplayAbility_Aim::OnInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
