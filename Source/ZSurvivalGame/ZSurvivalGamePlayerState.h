// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "ZSurvivalGamePlayerState.generated.h"

class UAbilitySystemComponent;
class UZAttributeSet;

/**
 *  PlayerState that owns the player's ability system and attributes (health, stamina)
 *  They live here instead of on the character so they survive respawns
 */
UCLASS()
class AZSurvivalGamePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

	/** Runs the player's abilities and holds their attributes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Health and stamina */
	UPROPERTY()
	TObjectPtr<UZAttributeSet> AttributeSet;

public:

	AZSurvivalGamePlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UZAttributeSet* GetAttributeSet() const { return AttributeSet; }
};
