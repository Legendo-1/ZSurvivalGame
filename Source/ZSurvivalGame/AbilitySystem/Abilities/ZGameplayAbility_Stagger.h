// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_Stagger.generated.h"

/**
 *  Staggered: the character's poise broke (UZAttributeSet::OnPoiseBroken), so it reels for a moment and can't do anything (State.Staggered)
 *  The server starts it with Event.Stagger (UZCombatLibrary::Stagger); for a player it then runs on their machine too. A new stagger starts it over
 *  Cuts short whatever the character was doing. Its animation is the character's Stagger Montage (IZCombatantInterface); without one, it stands still
 *  for the Default Stagger Duration (Project Settings → Game → Combat). Everyone gets the character's Stagger Cue
 */
UCLASS()
class UZGameplayAbility_Stagger : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_Stagger();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** The stagger is over */
	UFUNCTION()
	void OnStaggerFinished();
};
