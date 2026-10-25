// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Aim.generated.h"

/**
 *  Aim while the aim key is held: slower walk, faces the camera, camera zooms in
 *  Cancels sprint
 */
UCLASS()
class UZGameplayAbility_Aim : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_Aim();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** Called when the aim key is released */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);
};
