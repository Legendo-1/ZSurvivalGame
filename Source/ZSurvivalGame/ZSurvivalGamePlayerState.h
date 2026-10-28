// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "ZSurvivalGamePlayerState.generated.h"

class UAbilitySystemComponent;
class UZAttributeSet;
class UZCraftingComponent;
class UZEquipmentComponent;
class UZInventoryComponent;
class UZItemInstance;
class UZProgressionComponent;

/**
 *  PlayerState that owns the player's ability system, attributes (health, stamina, stats), inventory, equipment, crafting and progression (level, traits)
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

	/** Which items are equipped: weapons, armor and jewelry */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZEquipmentComponent> Equipment;

	/** Crafting by combining items, tradeskills and discoveries */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZCraftingComponent> Crafting;

	/** Experience, level, trait points and traits */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZProgressionComponent> Progression;

public:

	AZSurvivalGamePlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UZAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UZInventoryComponent* GetInventory() const { return Inventory; }

	UZEquipmentComponent* GetEquipment() const { return Equipment; }

	UZCraftingComponent* GetCrafting() const { return Crafting; }

	UZProgressionComponent* GetProgression() const { return Progression; }

	/** True once the player got their starting items (so a respawn doesn't give them again). Saved with the character; the player's own machine gets it too, for saving there */
	UPROPERTY(Replicated)
	bool bReceivedStartingItems = false;

	/** Server: the weapon item to take out when the character next spawns, e.g. the one in the hands when the game was saved */
	TWeakObjectPtr<UZItemInstance> PendingEquipItem;
};
