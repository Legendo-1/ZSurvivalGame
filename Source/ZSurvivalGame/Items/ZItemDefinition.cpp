// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemDefinition.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZEquipmentSettings.h"
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

FText UZItemDefinition::GetTypeName() const
{
	const FGameplayTag TypeTag = GetItemTypeTag();
	if (!TypeTag.IsValid())
	{
		return FText::GetEmpty();
	}

	// the last part of the tag: WeaponMod → "Weapon Mod"
	FString TypeName = TypeTag.ToString();
	int32 LastDot = INDEX_NONE;
	if (TypeName.FindLastChar(TEXT('.'), LastDot))
	{
		TypeName.RightChopInline(LastDot + 1);
	}

	return FText::FromString(FName::NameToDisplayString(TypeName, false));
}

TArray<FText> UZItemDefinition::GetDescriptionLines(const UZItemInstance* Instance) const
{
	TArray<FText> Lines;
	for (const UZItemFragment* Fragment : Fragments)
	{
		if (Fragment)
		{
			Fragment->GetDescriptionLines(Instance, Lines);
		}
	}

	return Lines;
}

int32 UZItemDefinition::GetToolQuality() const
{
	const UZItemFragment_Tool* Tool = FindFragment<UZItemFragment_Tool>();
	return Tool ? Tool->Quality : 0;
}

FGameplayTag UZItemDefinition::GetEquipmentSlotType() const
{
	// the first fragment that makes it equippable says where it goes
	for (const UZItemFragment* Fragment : Fragments)
	{
		const FGameplayTag SlotType = Fragment ? Fragment->GetEquipmentSlotType() : FGameplayTag();
		if (SlotType.IsValid())
		{
			return SlotType;
		}
	}

	return FGameplayTag();
}

TArray<FZStatModifier> UZItemDefinition::GetEquippedStatModifiers() const
{
	TArray<FZStatModifier> Modifiers;
	for (const UZItemFragment* Fragment : Fragments)
	{
		if (Fragment)
		{
			Fragment->GetEquippedStatModifiers(Modifiers);
		}
	}

	return Modifiers;
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

	// equippable: players need a slot of its kind, and only one fragment says which
	TSet<FGameplayTag> SlotTypes;
	for (const UZItemFragment* Fragment : Fragments)
	{
		const FGameplayTag SlotType = Fragment ? Fragment->GetEquipmentSlotType() : FGameplayTag();
		if (SlotType.IsValid())
		{
			SlotTypes.Add(SlotType);
		}
	}

	if (SlotTypes.Num() > 1)
	{
		Context.AddWarning(LOCTEXT("SeveralSlotTypes", "More than one fragment says which equipment slot the item goes in; the first one counts."));
	}

	const FGameplayTag SlotType = GetEquipmentSlotType();
	if (SlotType.IsValid() && UZEquipmentSettings::Get()->GetSlotCount(SlotType) <= 0)
	{
		Context.AddWarning(FText::Format(LOCTEXT("NoSuchSlot", "Players have no '{0}' slot (Project Settings → Game → Equipment), so the item can't be equipped."), FText::FromString(SlotType.ToString())));
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
