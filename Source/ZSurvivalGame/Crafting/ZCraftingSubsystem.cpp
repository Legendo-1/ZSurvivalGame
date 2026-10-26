// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/ZCraftingSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ZSurvivalGame.h"
#include "Crafting/ZCraftingRecipe.h"

UZCraftingSubsystem* UZCraftingSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UZCraftingSubsystem>() : nullptr;
}

TArray<UZCraftingRecipe*> UZCraftingSubsystem::GetAllRecipes() const
{
	TArray<UZCraftingRecipe*> Result;
	for (UZCraftingRecipe* Recipe : GetRecipes())
	{
		Result.Add(Recipe);
	}

	return Result;
}

TConstArrayView<TObjectPtr<UZCraftingRecipe>> UZCraftingSubsystem::GetRecipes() const
{
	EnsureRecipesLoaded();
	return Recipes;
}

TArray<UZCraftingRecipe*> UZCraftingSubsystem::GetRecipesUsingItem(const UZItemDefinition* Item) const
{
	TArray<UZCraftingRecipe*> Result;
	for (UZCraftingRecipe* Recipe : GetRecipes())
	{
		if (Recipe->UsesItem(Item))
		{
			Result.Add(Recipe);
		}
	}

	return Result;
}

TArray<UZCraftingRecipe*> UZCraftingSubsystem::GetRecipesProducingItem(const UZItemDefinition* Item) const
{
	TArray<UZCraftingRecipe*> Result;
	for (UZCraftingRecipe* Recipe : GetRecipes())
	{
		if (Recipe->ProducesItem(Item))
		{
			Result.Add(Recipe);
		}
	}

	return Result;
}

UZCraftingRecipe* UZCraftingSubsystem::FindRecipeByName(FName RecipeName) const
{
	for (UZCraftingRecipe* Recipe : GetRecipes())
	{
		if (Recipe->GetFName() == RecipeName)
		{
			return Recipe;
		}
	}

	return nullptr;
}

TArray<UZCraftingRecipe*> UZCraftingSubsystem::FindRecipesForCombination(TConstArrayView<const UZItemDefinition*> Items) const
{
	TArray<UZCraftingRecipe*> Result;
	for (UZCraftingRecipe* Recipe : GetRecipes())
	{
		if (Recipe->MatchesCombination(Items))
		{
			Result.Add(Recipe);
		}
	}

	// the most specific recipe first; the rest stays in name order
	Result.StableSort([](const UZCraftingRecipe& A, const UZCraftingRecipe& B) { return A.GetNumInputs() > B.GetNumInputs(); });
	return Result;
}

void UZCraftingSubsystem::LoadRecipes()
{
	bRecipesLoaded = true;
	Recipes.Reset();

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> RecipeIds;
	AssetManager.GetPrimaryAssetIdList(UZCraftingRecipe::GetRecipeAssetType(), RecipeIds);

	// small data assets, loaded together once; their items load with them
	TArray<UZCraftingRecipe*> Loaded;
	for (const FPrimaryAssetId& RecipeId : RecipeIds)
	{
		if (UZCraftingRecipe* Recipe = Cast<UZCraftingRecipe>(AssetManager.GetPrimaryAssetPath(RecipeId).TryLoad()))
		{
			Loaded.Add(Recipe);
		}
		else
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("Couldn't load the recipe '%s'"), *RecipeId.ToString());
		}
	}

	Loaded.Sort([](const UZCraftingRecipe& A, const UZCraftingRecipe& B) { return A.GetName() < B.GetName(); });
	for (UZCraftingRecipe* Recipe : Loaded)
	{
		Recipes.Add(Recipe);
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Crafting: %d recipes"), Recipes.Num());
	WarnAboutSameInputs();
}

void UZCraftingSubsystem::EnsureRecipesLoaded() const
{
	if (!bRecipesLoaded)
	{
		// loading only fills the cache, so it's fine from const getters
		const_cast<UZCraftingSubsystem*>(this)->LoadRecipes();
	}
}

void UZCraftingSubsystem::WarnAboutSameInputs() const
{
	// what an input asks for, independent of the order of the inputs
	auto GetInputsKey = [](const UZCraftingRecipe& Recipe)
	{
		TArray<FString> Keys;
		for (const FZRecipeInput& Input : Recipe.Inputs)
		{
			if (Input.IsValid())
			{
				Keys.AddUnique(Input.Item ? Input.Item->GetPathName() : Input.ItemTags.ToStringSimple());
			}
		}

		Keys.Sort();
		return FString::Join(Keys, TEXT(" + "));
	};

	TMap<FString, const UZCraftingRecipe*> RecipesByInputs;
	for (const UZCraftingRecipe* Recipe : Recipes)
	{
		const FString Key = GetInputsKey(*Recipe);
		if (const UZCraftingRecipe* const* Existing = RecipesByInputs.Find(Key))
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("Recipes '%s' and '%s' take the same kinds of items (%s): players can only ever make the one they can craft first. Give one of them a different input."),
				*GetNameSafe(*Existing), *GetNameSafe(Recipe), *Key);
		}
		else
		{
			RecipesByInputs.Add(Key, Recipe);
		}
	}
}
