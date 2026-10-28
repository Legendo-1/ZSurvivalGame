// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_MeleeAttack.generated.h"

/**
 *  Melee attack for AI characters (server only): plays the character's attack animation,
 *  then after its hit delay damages everyone hostile in front of it (and wears down their poise)
 *  Damage, poise damage, reach, radius and timing come from the AZAICharacter. A stagger cuts it short
 *  (Players' melee attacks are UZGameplayAbility_LightAttack and UZGameplayAbility_HeavyAttack)
 */
UCLASS()
class UZGameplayAbility_MeleeAttack : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_MeleeAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:

	/** Called when the attack reaches its hit time */
	UFUNCTION()
	void OnHitTime();
};
