// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZCraftingSubsystem.generated.h"

class UZCraftingRecipe;
class UZItemDefinition;
class UZItemInstance;

/**
 *  Knows every crafting recipe in the project: the Asset Manager finds them (primary asset type "Recipe"), and they're loaded the first time they're needed
 *  So a new recipe only needs its Data Asset; nothing else has to list it
 */
UCLASS()
class UZCraftingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** The subsystem of the world's game instance, or null */
	static UZCraftingSubsystem* Get(const UObject* WorldContextObject);

	/** Every recipe, sorted by name */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UZCraftingRecipe*> GetAllRecipes() const;

	/** Every recipe (native, no copy) */
	TConstArrayView<TObjectPtr<UZCraftingRecipe>> GetRecipes() const;

	/** Recipes that take this item */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UZCraftingRecipe*> GetRecipesUsingItem(const UZItemDefinition* Item) const;

	/** Recipes that make this item */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<UZCraftingRecipe*> GetRecipesProducingItem(const UZItemDefinition* Item) const;

	/** A recipe by its asset name, e.g. RC_Bandage */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	UZCraftingRecipe* FindRecipeByName(FName RecipeName) const;

	/** Recipes whose inputs are exactly these kinds of items (counts and tool quality aside), the ones with the most inputs first */
	TArray<UZCraftingRecipe*> FindRecipesForCombination(TConstArrayView<const UZItemDefinition*> Items) const;

	/** The highest any upgrade recipe can raise this stat on this item, e.g. a weapon's upgrade level (0 if none can) */
	int32 GetHighestUpgradeLevel(const UZItemInstance* Target, FGameplayTag Stat) const;

private:

	/** Loads every recipe the Asset Manager knows */
	void LoadRecipes();

	/** Loads the recipes on first use */
	void EnsureRecipesLoaded() const;

	/** Warns about recipes that take the same kinds of items, since only one of them could ever be made */
	void WarnAboutSameInputs() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZCraftingRecipe>> Recipes;

	bool bRecipesLoaded = false;
};
