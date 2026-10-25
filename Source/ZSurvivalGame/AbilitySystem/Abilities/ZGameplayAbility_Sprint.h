// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Sprint.generated.h"

/**
 *  Sprint while the sprint key is held
 *  Blocked while aiming or while a speed limit is active; the server ends it when stamina runs out
 *  The speed itself comes from the movement component (see UZCharacterMovementComponent)
 */
UCLASS()
class UZGameplayAbility_Sprint : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_Sprint();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** Stamina needed to start sprinting */
	UPROPERTY(EditDefaultsOnly, Category = "Sprint")
	float MinStaminaToStart = 10.f;

	/** Called when the sprint key is released */
	UFUNCTION()
	void OnInputReleased(float TimeHeld);
};
