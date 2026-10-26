// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemRequirement.h"
#include "ZCraftingRecipe.generated.h"

class UZRecipeOutcome;

/** What crafting does with an input */
UENUM(BlueprintType)
enum class EZRecipeInputUsage : uint8
{
	/** Used up (an ingredient) */
	Consumed,
	/** Needed, but kept (a tool); it may lose durability */
	Tool,
	/** Kept, and changed by the recipe's Outcomes (e.g. the weapon being upgraded) */
	Target
};

/** One of the items a recipe combines */
USTRUCT(BlueprintType)
struct FZRecipeInput : public FZItemRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	EZRecipeInputUsage Usage = EZRecipeInputUsage::Consumed;

	/** Durability a tool loses each craft (tools with a Durability fragment break at 0) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0", EditCondition = "Usage == EZRecipeInputUsage::Tool"))
	int32 DurabilityCost = 0;

	bool IsTool() const { return Usage == EZRecipeInputUsage::Tool; }

	bool IsTarget() const { return Usage == EZRecipeInputUsage::Target; }

	bool IsConsumed() const { return Usage == EZRecipeInputUsage::Consumed; }
};

/** A tradeskill level a recipe needs */
USTRUCT(BlueprintType)
struct FZSkillRequirement
{
	GENERATED_BODY()

	/** e.g. Tradeskill.Electronics */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (Categories = "Tradeskill"))
	FGameplayTag Skill;

	/** Players below this level can't craft it at all (0 = anyone, but it still trains the skill) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill", meta = (ClampMin = "0"))
	int32 MinLevel = 0;
};

/**
 *  A hidden combination: the right items (some used up, some tools that are kept), maybe a station nearby and a high enough tradeskill, make something
 *  Players never see recipes; they find them by combining items. Every successful craft trains the recipe's tradeskills
 *  One Data Asset per recipe in Content/Crafting/Recipes: right-click → Miscellaneous → Data Asset → ZCraftingRecipe.
 *  The game finds every recipe by itself (Asset Manager), so adding or deleting the asset is all it takes
 */
UCLASS(BlueprintType)
class UZCraftingRecipe : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Primary asset type of every recipe (the Asset Manager finds them by it) */
	static FPrimaryAssetType GetRecipeAssetType() { return FPrimaryAssetType(TEXT("Recipe")); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Name in messages, e.g. "Crafted Medkit" (empty: the first result's name) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	/** --- WHAT IT TAKES --- */

	/** The items to combine, each used up or kept as a tool. A combination only works with exactly these kinds of items, so players have to find it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Inputs")
	TArray<FZRecipeInput> Inputs;

	/** A crafting station that has to be nearby, e.g. Crafting.Station.Workbench (empty: anywhere) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Inputs", meta = (Categories = "Crafting.Station"))
	FGameplayTag RequiredStation;

	/** Tradeskill levels needed. A hard limit: below it, it can't be made. Crafting it trains these skills */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Skill")
	TArray<FZSkillRequirement> SkillRequirements;

	/** Tradeskill experience per craft, for each skill above (it fades out once the player is far above the needed level, see Project Settings → Progression) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Skill", meta = (ClampMin = "0"))
	int32 SkillExperience = 10;

	/** Extra tradeskill experience the first time a player finds this combination */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Skill", meta = (ClampMin = "0"))
	int32 DiscoveryExperience = 25;

	/** --- WHAT IT GIVES --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Results")
	TArray<FZItemStack> Results;

	/** What it does to its Target input, e.g. upgrade a weapon or install a mod (a recipe needs Results, Outcomes or both). A recipe with Outcomes is made once per combine */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = "Recipe|Results")
	TArray<TObjectPtr<UZRecipeOutcome>> Outcomes;

	/** Seconds one craft takes (0 = right away) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Results", meta = (ClampMin = "0", Units = "s"))
	float CraftTime = 0.f;

	/** --- EFFECTS: what plays is a GameplayCue Notify asset with the tag (see AUDIO.md); a child tag gives this recipe its own. Empty = nothing --- */

	/** Lasts while it's being crafted, e.g. hammering (at the station if the recipe needs one, else at the player). Needs a Craft Time */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Effects", meta = (Categories = "GameplayCue.Crafting.Working"))
	FGameplayTag WorkingCue = ZGameplayTags::GameplayCue_Crafting_Working;

	/** A combine made something (Raw Magnitude = how many crafts) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe|Effects", meta = (Categories = "GameplayCue.Crafting.Done"))
	FGameplayTag DoneCue = ZGameplayTags::GameplayCue_Crafting_Done;

#if WITH_EDITORONLY_DATA
	/** Notes for the team, e.g. where the ingredients are found (not in the game) */
	UPROPERTY(EditDefaultsOnly, Category = "Recipe", meta = (MultiLine = "true"))
	FString DeveloperNotes;
#endif

	/** The input marked Target, or null */
	const FZRecipeInput* GetTargetInput() const;

	/** Name for messages: Display Name, or the first result's name */
	UFUNCTION(BlueprintPure, Category = "Recipe")
	FText GetRecipeName() const;

	/** The first result's item, or null */
	UFUNCTION(BlueprintPure, Category = "Recipe")
	UZItemDefinition* GetMainResult() const;

	/** True if the item is one of the inputs */
	UFUNCTION(BlueprintPure, Category = "Recipe")
	bool UsesItem(const UZItemDefinition* Item) const;

	/** True if the item is one of the results */
	UFUNCTION(BlueprintPure, Category = "Recipe")
	bool ProducesItem(const UZItemDefinition* Item) const;

	/**
	 *  True if these are exactly the kinds of items the recipe combines: each input is among them, and each of them is an input
	 *  (counts and tool quality aside, so a near miss can tell the player what's wrong)
	 */
	bool MatchesCombination(TConstArrayView<const UZItemDefinition*> Items) const;

	/** Inputs that name an item or tags */
	int32 GetNumInputs() const;

	/** The level needed in this skill (0 if the recipe doesn't train it) */
	int32 GetRequiredLevel(FGameplayTag Skill) const;

	/** "Cloth x3 + Any Screwdriver (tool) → Bandage x1", for logs and developer commands */
	FString GetDebugString() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
