// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "ZSurvivalGamePlayerState.generated.h"

class UAbilitySystemComponent;
class UZAttributeSet;
class UZInventoryComponent;

/**
 *  PlayerState that owns the player's ability system, attributes (health, stamina) and inventory
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

	/** The player's items; only the player sees what's in it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZInventoryComponent> Inventory;

public:

	AZSurvivalGamePlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UZAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UZInventoryComponent* GetInventory() const { return Inventory; }

	/** Server: true once the player got their starting items (so a respawn doesn't give them again) */
	bool bReceivedStartingItems = false;
};
