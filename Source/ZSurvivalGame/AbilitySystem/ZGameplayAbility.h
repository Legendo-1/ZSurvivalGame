// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ZSurvivalGameTypes.h"
#include "ZGameplayAbility.generated.h"

class AZSurvivalGameCharacter;
class UZCharacterMovementComponent;

/**
 *  Base class for the player's abilities
 *  Predicted: the owning client starts it right away, the server runs it too and confirms it (or cancels it)
 */
UCLASS(Abstract)
class UZGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility();

	/** Which input activates this ability; None for abilities started by code or events */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EZAbilityInputID InputID = EZAbilityInputID::None;

protected:

	/** The character using this ability, or null if it isn't ours */
	static AZSurvivalGameCharacter* GetZCharacter(const FGameplayAbilityActorInfo* ActorInfo);

	/** The character's movement component, which syncs sprint and aim with the server */
	static UZCharacterMovementComponent* GetZMovement(const FGameplayAbilityActorInfo* ActorInfo);
};
