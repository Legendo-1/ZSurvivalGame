// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZEquipmentSettings.h"
#include "AbilitySystem/ZGameplayTags.h"

#define LOCTEXT_NAMESPACE "ZItems"

UZEquipmentSettings::UZEquipmentSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Equipment");

	const FText Weapons = LOCTEXT("WeaponsGroup", "Weapons");
	const FText Armor = LOCTEXT("ArmorGroup", "Armor");
	const FText Jewelry = LOCTEXT("JewelryGroup", "Jewelry");

	const auto AddSlotType = [this](const FGameplayTag& Type, int32 Count, const FText& Name, const FText& Group)
	{
		FZEquipmentSlotType& SlotType = SlotTypes.AddDefaulted_GetRef();
		SlotType.Type = Type;
		SlotType.Count = Count;
		SlotType.DisplayName = Name;
		SlotType.Group = Group;
	};

	// one weapon of each kind (keys 1, 2 and 3), armor, a necklace and four rings, like Remnant 2
	AddSlotType(ZGameplayTags::Equipment_Slot_ShortGun, 1, LOCTEXT("ShortGunSlot", "Short gun"), Weapons);
	AddSlotType(ZGameplayTags::Equipment_Slot_LongGun, 1, LOCTEXT("LongGunSlot", "Long gun"), Weapons);
	AddSlotType(ZGameplayTags::Equipment_Slot_Melee, 1, LOCTEXT("MeleeSlot", "Melee"), Weapons);
	AddSlotType(ZGameplayTags::Equipment_Slot_Head, 1, LOCTEXT("HeadSlot", "Head"), Armor);
	AddSlotType(ZGameplayTags::Equipment_Slot_Torso, 1, LOCTEXT("TorsoSlot", "Torso"), Armor);
	AddSlotType(ZGameplayTags::Equipment_Slot_Legs, 1, LOCTEXT("LegsSlot", "Legs"), Armor);
	AddSlotType(ZGameplayTags::Equipment_Slot_Neck, 1, LOCTEXT("NeckSlot", "Neck"), Jewelry);
	AddSlotType(ZGameplayTags::Equipment_Slot_Ring, 4, LOCTEXT("RingSlot", "Ring"), Jewelry);
}

const FZEquipmentSlotType* UZEquipmentSettings::FindSlotType(const FGameplayTag& Type) const
{
	return Type.IsValid() ? SlotTypes.FindByPredicate([&Type](const FZEquipmentSlotType& SlotType) { return SlotType.Type == Type; }) : nullptr;
}

int32 UZEquipmentSettings::GetSlotCount(const FGameplayTag& Type) const
{
	const FZEquipmentSlotType* SlotType = FindSlotType(Type);
	return SlotType ? FMath::Max(SlotType->Count, 1) : 0;
}

FText UZEquipmentSettings::GetSlotTypeName(const FGameplayTag& Type) const
{
	const FZEquipmentSlotType* SlotType = FindSlotType(Type);
	if (SlotType && !SlotType->DisplayName.IsEmpty())
	{
		return SlotType->DisplayName;
	}

	// "Equipment.Slot.LongGun" → "Long Gun"
	FString Name = Type.ToString();
	int32 LastDot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), LastDot))
	{
		Name.RightChopInline(LastDot + 1);
	}

	return FText::FromString(FName::NameToDisplayString(Name, false));
}

#undef LOCTEXT_NAMESPACE
