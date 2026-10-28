// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Dodge.generated.h"

/**
 *  Quick dash in the movement direction (forward when standing still), costs stamina
 *  Moves the character with root motion, which the client and the server run in sync
 *  An action (see COMBAT.md): no dodging mid-swing or mid-shot (it waits in the input buffer), except in an action's Dodge Window.
 *  Its first part is invulnerable: hits pass through (State.Invulnerable)
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

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

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

	/** Seconds from the start in which hits pass through the character (souls-like invulnerable frames); 0 = none */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0", Units = "s"))
	float InvulnerableTime = 0.2f;

	/** Animation played with the dodge, e.g. a roll (optional). It must stay in place: the dodge moves the character itself, so one with root motion isn't played */
	UPROPERTY(EditDefaultsOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;

	/** Called when the dodge movement is done */
	UFUNCTION()
	void OnDodgeFinished();

	/** The invulnerable frames are over */
	UFUNCTION()
	void OnInvulnerabilityFinished();

	/** Takes State.Invulnerable off again, if this dodge put it on */
	void EndInvulnerability();

	bool bInvulnerable = false;
};
