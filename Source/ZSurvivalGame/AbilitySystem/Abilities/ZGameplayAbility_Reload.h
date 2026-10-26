// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Reload.generated.h"

/**
 *  Reloads the equipped weapon: takes the weapon's reload time, then loads the magazine from the weapon's ammo item in the inventory
 *  Limits the speed to jogging meanwhile; firing is blocked, dodging or switching weapons cancels it
 */
UCLASS()
class UZGameplayAbility_Reload : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_Reload();

	/** Needs a weapon that isn't full */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** Called when the reload time is over */
	UFUNCTION()
	void OnReloadFinished();
};
