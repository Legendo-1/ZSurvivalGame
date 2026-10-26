// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ZRecipeOutcomes.generated.h"

class FDataValidationContext;
class UZCraftingComponent;
class UZCraftingRecipe;
class UZInventoryComponent;
class UZItemDefinition;
class UZItemInstance;

/** What a recipe outcome works with */
struct FZRecipeOutcomeContext
{
	/** The crafting player's crafting component (e.g. for their tradeskills) */
	const UZCraftingComponent* Crafting = nullptr;

	UZInventoryComponent* Inventory = nullptr;

	/** The recipe's Target input: the item the outcome changes */
	UZItemInstance* Target = nullptr;

	/** Kinds of items the craft uses up, e.g. the mod being installed */
	TArray<UZItemDefinition*> ConsumedItems;

	/** Server, while applying: lines for the message to the player */
	TArray<FText>* Notes = nullptr;
};

/**
 *  Something a recipe does to its Target input (an item that's kept), instead of or besides making new items:
 *  upgrade a weapon, install a mod, repair a tool, ... A new kind of change is a new child class; existing recipes don't change
 *  Added to a recipe's Outcomes list in its Data Asset
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class UZRecipeOutcome : public UObject
{
	GENERATED_BODY()

public:

	/** Can it happen to this target right now? If not, OutReason tells the player why (checked before anything is used up) */
	virtual bool CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const { return true; }

	/** Server: makes it happen (the ingredients are already used up) */
	virtual void Apply(const FZRecipeOutcomeContext& Context) const {}

	/** "Upgrade Target", for logs and developer commands */
	virtual FString GetDebugString() const;

#if WITH_EDITOR
	/** Checks the outcome fits its recipe (called when the recipe is validated) */
	virtual void ValidateForRecipe(const UZCraftingRecipe* Recipe, FDataValidationContext& Context) const {}
#endif

protected:

	/** The target's name for messages, e.g. "Rifle" */
	static FText GetTargetName(const FZRecipeOutcomeContext& Context);
};

/**
 *  Raises a stat of the target, e.g. a weapon's upgrade level (more damage per level, see its weapon data)
 *  Each level can need a higher tradeskill, so upgrading further takes grinding
 */
UCLASS(meta = (DisplayName = "Upgrade Target"))
class UZRecipeOutcome_UpgradeTarget : public UZRecipeOutcome
{
	GENERATED_BODY()

public:

	UZRecipeOutcome_UpgradeTarget();

	/** The stat that goes up */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Item.Stat"))
	FGameplayTag Stat;

	/** How much it goes up per craft */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "1"))
	int32 Amount = 1;

	/** The highest it can go; 0 = the weapon data's Max Upgrade Level */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 MaxLevel = 0;

	/** A tradeskill each level needs more of (empty: none) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (Categories = "Tradeskill"))
	FGameplayTag Skill;

	/** Tradeskill needed per level the target already has: with 10, +0 → +1 needs 0, +2 → +3 needs 20 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade", meta = (ClampMin = "0"))
	int32 SkillPerLevel = 10;

	virtual bool CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const override;
	virtual void Apply(const FZRecipeOutcomeContext& Context) const override;
	virtual FString GetDebugString() const override;

#if WITH_EDITOR
	virtual void ValidateForRecipe(const UZCraftingRecipe* Recipe, FDataValidationContext& Context) const override;
#endif

protected:

	/** The highest level for this target (0 if it can't be upgraded) */
	int32 GetMaxLevel(const UZItemInstance* Target) const;
};

/**
 *  Installs the used-up weapon mod on the target weapon (a mod already in that slot goes back to the inventory)
 */
UCLASS(meta = (DisplayName = "Install Mod"))
class UZRecipeOutcome_InstallMod : public UZRecipeOutcome
{
	GENERATED_BODY()

public:

	virtual bool CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const override;
	virtual void Apply(const FZRecipeOutcomeContext& Context) const override;

#if WITH_EDITOR
	virtual void ValidateForRecipe(const UZCraftingRecipe* Recipe, FDataValidationContext& Context) const override;
#endif

protected:

	/** The used-up item that's a weapon mod, or null */
	static UZItemDefinition* FindMod(const FZRecipeOutcomeContext& Context);
};

/**
 *  Takes every mod off the target weapon and puts them back in the inventory
 */
UCLASS(meta = (DisplayName = "Remove Mods"))
class UZRecipeOutcome_RemoveMods : public UZRecipeOutcome
{
	GENERATED_BODY()

public:

	virtual bool CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const override;
	virtual void Apply(const FZRecipeOutcomeContext& Context) const override;
};

/**
 *  Gives the target back durability, e.g. repairing a worn tool
 */
UCLASS(meta = (DisplayName = "Repair Target"))
class UZRecipeOutcome_RepairTarget : public UZRecipeOutcome
{
	GENERATED_BODY()

public:

	/** Durability given back; 0 = all of it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Repair", meta = (ClampMin = "0"))
	int32 Amount = 0;

	virtual bool CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const override;
	virtual void Apply(const FZRecipeOutcomeContext& Context) const override;
};
