// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ZAbilitySystemComponent.generated.h"

/**
 *  The ability system of players (on their PlayerState) and AI characters
 *  Keeps lasting cues (UZGameplayCueLibrary::AddCue, e.g. the crafting loop) on the character's body, which the engine doesn't do
 *  for an ability system on a PlayerState: they start on the character when it arrives (e.g. for a player who joins during a craft),
 *  and move to the new body after a respawn
 */
UCLASS()
class UZAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

private:

	/** Sends every lasting cue to Target, e.g. Removed to the old body and WhileActive to the new one */
	void InvokeActiveCues(AActor* Target, EGameplayCueEvent::Type EventType);
};
