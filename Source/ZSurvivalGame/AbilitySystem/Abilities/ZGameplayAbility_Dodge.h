// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Dodge.generated.h"

/**
 *  Quick dash in the movement direction (forward when standing still), costs stamina
 *  Moves the character with root motion, which the client and the server run in sync
 */
UCLASS()
class UZGameplayAbility_Dodge : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_Dodge();

	/** Enough stamina for the dodge? */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Pays the stamina, on the server */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:

	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float StaminaCost = 25.f;

	/** Speed during the dodge, in cm/s */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DodgeSpeed = 1200.f;

	/** How long the dodge lasts, in seconds */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float DodgeDuration = 0.3f;

	/** Speed the character keeps when the dodge ends, in cm/s */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	float ExitSpeed = 350.f;

	/** Called when the dodge movement is done */
	UFUNCTION()
	void OnDodgeFinished();
};
