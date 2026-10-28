// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Fire.generated.h"

class AZWeapon;

/**
 *  Fires one shot of the equipped gun (automatic guns are fired again by the character while the button is held)
 *  The shooter's machine traces the bullets from the camera and sends the hits to the server,
 *  which checks them and applies the damage. One round per shot; fire rate from the weapon data
 *  An action (see COMBAT.md): after each shot the shooter is committed for the gun's Shot Recovery Time (no dodging yet); a shot may also start in an attack's combo window
 */
UCLASS()
class UZGameplayAbility_Fire : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_Fire();

	/** Needs a weapon that's ready for the next shot */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** A round in the magazine? */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Uses the round, on the server */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** Shooter's machine: traces the bullets, kicks the camera and sends the hits to the server */
	void FireLocalShot();

	/** The shot's hits are known: from FireLocalShot, or received from the client on the server */
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& InTargetData, FGameplayTag ApplicationTag);

	/** Server: is this hit plausible for the shooter (in range)? */
	bool IsHitValid(const FHitResult& Hit, float Range) const;

	/** The shot's recovery is over: the shooter may dodge again */
	UFUNCTION()
	void OnRecoveryFinished();

	/** The weapon in the character's hands */
	static AZWeapon* GetWeapon(const FGameplayAbilityActorInfo* ActorInfo);

	FDelegateHandle TargetDataDelegateHandle;
};
