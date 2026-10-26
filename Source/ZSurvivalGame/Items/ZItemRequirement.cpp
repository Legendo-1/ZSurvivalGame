// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemRequirement.h"
#include "Items/ZItemDefinition.h"

bool FZItemRequirement::IsValid() const
{
	return Item != nullptr || !ItemTags.IsEmpty();
}

bool FZItemRequirement::Matches(const UZItemDefinition* Candidate, bool bCheckToolQuality) const
{
	if (!Candidate)
	{
		return false;
	}

	if (bCheckToolQuality && MinToolQuality > 0 && Candidate->GetToolQuality() < MinToolQuality)
	{
		return false;
	}

	if (Item)
	{
		return Candidate == Item;
	}

	// HasAll also accepts child tags, so asking for Item.Tool accepts every tool
	return !ItemTags.IsEmpty() && Candidate->ItemTags.HasAll(ItemTags);
}

FText FZItemRequirement::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	if (Item)
	{
		return Item->GetItemName();
	}

	if (ItemTags.IsEmpty())
	{
		return FText::GetEmpty();
	}

	// "Any Screwdriver" from Item.Tool.Screwdriver
	FString TagName = ItemTags.First().ToString();
	int32 LastDot = INDEX_NONE;
	if (TagName.FindLastChar(TEXT('.'), LastDot))
	{
		TagName.RightChopInline(LastDot + 1);
	}

	return FText::Format(NSLOCTEXT("ZItems", "AnyTaggedItem", "Any {0}"), FText::FromString(FName::NameToDisplayString(TagName, false)));
}
