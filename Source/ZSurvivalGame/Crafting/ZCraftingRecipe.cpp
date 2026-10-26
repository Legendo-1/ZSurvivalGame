// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZRecipeOutcomes.h"
#include "Items/ZItemFragments.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZCrafting"

FPrimaryAssetId UZCraftingRecipe::GetPrimaryAssetId() const
{
	// every recipe (also of child classes) is the same asset type
	return FPrimaryAssetId(GetRecipeAssetType(), GetFName());
}

FText UZCraftingRecipe::GetRecipeName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	const UZItemDefinition* MainResult = GetMainResult();
	return MainResult ? MainResult->GetItemName() : FText::FromName(GetFName());
}

const FZRecipeInput* UZCraftingRecipe::GetTargetInput() const
{
	return Inputs.FindByPredicate([](const FZRecipeInput& Input) { return Input.IsValid() && Input.IsTarget(); });
}

UZItemDefinition* UZCraftingRecipe::GetMainResult() const
{
	for (const FZItemStack& Result : Results)
	{
		if (Result.Item)
		{
			return Result.Item;
		}
	}

	return nullptr;
}

bool UZCraftingRecipe::UsesItem(const UZItemDefinition* Item) const
{
	return Item && Inputs.ContainsByPredicate([Item](const FZRecipeInput& Input) { return Input.Matches(Item, false); });
}

bool UZCraftingRecipe::ProducesItem(const UZItemDefinition* Item) const
{
	return Item && Results.ContainsByPredicate([Item](const FZItemStack& Result) { return Result.Item == Item; });
}

bool UZCraftingRecipe::MatchesCombination(TConstArrayView<const UZItemDefinition*> Items) const
{
	if (Items.IsEmpty() || GetNumInputs() == 0)
	{
		return false;
	}

	// every input is among the items
	for (const FZRecipeInput& Input : Inputs)
	{
		if (Input.IsValid() && !Items.ContainsByPredicate([&Input](const UZItemDefinition* Item) { return Input.Matches(Item, false); }))
		{
			return false;
		}
	}

	// and nothing else was added
	for (const UZItemDefinition* Item : Items)
	{
		if (!UsesItem(Item))
		{
			return false;
		}
	}

	return true;
}

int32 UZCraftingRecipe::GetNumInputs() const
{
	int32 Num = 0;
	for (const FZRecipeInput& Input : Inputs)
	{
		Num += Input.IsValid() ? 1 : 0;
	}

	return Num;
}

int32 UZCraftingRecipe::GetRequiredLevel(FGameplayTag Skill) const
{
	const FZSkillRequirement* Requirement = SkillRequirements.FindByPredicate([Skill](const FZSkillRequirement& Entry) { return Entry.Skill == Skill; });
	return Requirement ? Requirement->MinLevel : 0;
}

FString UZCraftingRecipe::GetDebugString() const
{
	TArray<FString> InputTexts;
	for (const FZRecipeInput& Input : Inputs)
	{
		if (!Input.IsValid())
		{
			continue;
		}

		FString Text = FString::Printf(TEXT("%s x%d"), *Input.GetDisplayName().ToString(), Input.Count);
		if (Input.IsTool())
		{
			Text += Input.MinToolQuality > 0 ? FString::Printf(TEXT(" (tool, quality %d+)"), Input.MinToolQuality) : FString(TEXT(" (tool)"));
		}
		else if (Input.IsTarget())
		{
			Text += TEXT(" (target)");
		}

		InputTexts.Add(Text);
	}

	TArray<FString> ResultTexts;
	for (const FZItemStack& Result : Results)
	{
		if (Result.Item)
		{
			ResultTexts.Add(FString::Printf(TEXT("%s x%d"), *Result.Item->GetItemName().ToString(), Result.Count));
		}
	}

	for (const UZRecipeOutcome* Outcome : Outcomes)
	{
		if (Outcome)
		{
			ResultTexts.Add(Outcome->GetDebugString());
		}
	}

	TArray<FString> Extras;
	if (RequiredStation.IsValid())
	{
		Extras.Add(FString::Printf(TEXT("at %s"), *RequiredStation.ToString()));
	}

	for (const FZSkillRequirement& Requirement : SkillRequirements)
	{
		Extras.Add(FString::Printf(TEXT("%s %d"), *Requirement.Skill.ToString(), Requirement.MinLevel));
	}

	return FString::Printf(TEXT("%s: %s -> %s%s"), *GetName(), *FString::Join(InputTexts, TEXT(" + ")), *FString::Join(ResultTexts, TEXT(", ")),
		Extras.Num() > 0 ? *FString::Printf(TEXT(" [%s]"), *FString::Join(Extras, TEXT(", "))) : TEXT(""));
}

#if WITH_EDITOR
EDataValidationResult UZCraftingRecipe::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!GetMainResult() && Outcomes.IsEmpty())
	{
		Context.AddError(LOCTEXT("NoResults", "The recipe does nothing: add an item to Results, or an Outcome."));
	}

	// outcomes change the Target input, so there has to be exactly one
	int32 NumTargets = 0;
	for (const FZRecipeInput& Input : Inputs)
	{
		NumTargets += Input.IsValid() && Input.IsTarget() ? 1 : 0;
	}

	if (!Outcomes.IsEmpty() && NumTargets != 1)
	{
		Context.AddError(LOCTEXT("OutcomesNeedTarget", "A recipe with Outcomes needs exactly one input with Usage Target (the item it changes)."));
	}
	else if (Outcomes.IsEmpty() && NumTargets > 0)
	{
		Context.AddWarning(LOCTEXT("TargetWithoutOutcomes", "An input is a Target, but the recipe has no Outcomes to change it, so it's just kept."));
	}

	for (const UZRecipeOutcome* Outcome : Outcomes)
	{
		if (!Outcome)
		{
			Context.AddWarning(LOCTEXT("EmptyOutcome", "The recipe has an empty entry in Outcomes."));
			continue;
		}

		Outcome->ValidateForRecipe(this, Context);
	}

	if (GetNumInputs() == 0)
	{
		Context.AddError(LOCTEXT("NoInputs", "The recipe takes nothing: add Inputs."));
	}

	for (const FZRecipeInput& Input : Inputs)
	{
		if (!Input.IsValid())
		{
			Context.AddWarning(LOCTEXT("EmptyInput", "An input has neither an Item nor Item Tags, so it's ignored."));
		}
		else if (Input.IsTool() && Input.DurabilityCost > 0 && Input.Item && !Input.Item->FindFragment<UZItemFragment_Durability>())
		{
			Context.AddWarning(FText::Format(LOCTEXT("ToolNoDurability", "The tool {0} has a Durability Cost but no Durability fragment, so it never wears out."), Input.Item->GetItemName()));
		}
		else if (Input.MinToolQuality > 0 && Input.Item && Input.Item->GetToolQuality() < Input.MinToolQuality)
		{
			Context.AddError(FText::Format(LOCTEXT("ToolQualityTooHigh", "The input {0} asks for tool quality {1}, but that item's quality is lower, so the recipe can never be made."), Input.Item->GetItemName(), Input.MinToolQuality));
		}
		else if (Input.IsTarget() && Input.Count != 1)
		{
			Context.AddError(LOCTEXT("TargetCount", "A Target input's Count has to be 1: the outcomes change one item."));
		}
	}

	for (const FZSkillRequirement& Requirement : SkillRequirements)
	{
		if (!Requirement.Skill.IsValid())
		{
			Context.AddWarning(LOCTEXT("EmptySkill", "A skill requirement has no skill, so it's ignored."));
		}
	}

	for (const FZItemStack& Stack : Results)
	{
		if (!Stack.Item)
		{
			Context.AddWarning(LOCTEXT("EmptyResult", "A result has no item, so it's ignored."));
		}
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
