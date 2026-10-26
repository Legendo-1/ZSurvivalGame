// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemDefinition.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZItemFragments.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZItems"

FPrimaryAssetId UZItemDefinition::GetPrimaryAssetId() const
{
	// every item (also of child classes) is the same asset type
	return FPrimaryAssetId(GetItemAssetType(), GetFName());
}

const UZItemFragment* UZItemDefinition::FindFragmentByClass(TSubclassOf<UZItemFragment> FragmentClass) const
{
	if (!FragmentClass)
	{
		return nullptr;
	}

	for (const UZItemFragment* Fragment : Fragments)
	{
		if (Fragment && Fragment->IsA(FragmentClass))
		{
			return Fragment;
		}
	}

	return nullptr;
}

FText UZItemDefinition::GetItemName() const
{
	return DisplayName.IsEmpty() ? FText::FromName(GetFName()) : DisplayName;
}

FText UZItemDefinition::GetUseActionName() const
{
	// the first fragment that makes it usable names the action
	for (const UZItemFragment* Fragment : Fragments)
	{
		if (Fragment)
		{
			FText ActionName = Fragment->GetUseActionName();
			if (!ActionName.IsEmpty())
			{
				return ActionName;
			}
		}
	}

	return FText::GetEmpty();
}

FGameplayTag UZItemDefinition::GetItemTypeTag() const
{
	const FGameplayTag TypeParent = FGameplayTag::RequestGameplayTag(TEXT("Item.Type"));
	for (const FGameplayTag& Tag : ItemTags)
	{
		if (Tag.MatchesTag(TypeParent) && Tag != TypeParent)
		{
			return Tag;
		}
	}

	return FGameplayTag();
}

int32 UZItemDefinition::GetToolQuality() const
{
	const UZItemFragment_Tool* Tool = FindFragment<UZItemFragment_Tool>();
	return Tool ? Tool->Quality : 0;
}

#if WITH_EDITOR
EDataValidationResult UZItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoDisplayName", "The item has no Display Name; the UI shows the asset name instead."));
	}

	if (!GetItemTypeTag().IsValid())
	{
		Context.AddWarning(LOCTEXT("NoTypeTag", "The item has no Item.Type tag (e.g. Item.Type.Material), so the inventory can't sort or filter it."));
	}

	for (const UZItemFragment* Fragment : Fragments)
	{
		if (!Fragment)
		{
			Context.AddWarning(LOCTEXT("EmptyFragment", "The item has an empty entry in Fragments."));
			continue;
		}

		Fragment->ValidateForItem(this, Context);
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
