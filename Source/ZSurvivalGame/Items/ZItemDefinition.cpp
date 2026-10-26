// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemDefinition.h"

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
