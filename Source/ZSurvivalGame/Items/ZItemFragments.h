// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZSurvivalGameTypes.h"
#include "Items/ZItemDefinition.h"
#include "Weapons/ZWeaponStats.h"
#include "ZItemFragments.generated.h"

class UGameplayEffect;
class UZWeaponData;

/**
 *  Makes the item a weapon: using (equipping) it puts the weapon in the character's hands
 *  Each weapon item keeps its own magazine ammo (Item.Stat.MagazineAmmo), starting full
 */
UCLASS(meta = (DisplayName = "Weapon"))
class UZItemFragment_Weapon : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** The weapon's stats and looks */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UZWeaponData> WeaponData;

	virtual void OnInstanceCreated(UZItemInstance* Instance) const override;

	/** Keeps the mods that fit (the others go back to the inventory) and no more rounds than the magazine holds */
	virtual void OnInstanceLoaded(UZItemInstance* Instance) const override;

	/** Puts the weapon in its slot and in the user's hands */
	virtual bool OnUse(UZItemInstance* Instance, APawn* User) const override;

	virtual FText GetUseActionName() const override;

	/** Its weapon data's slot: short gun, long gun or melee */
	virtual FGameplayTag GetEquipmentSlotType() const override;

	/** Its stats with the item's upgrades and mods, and what's in each mod slot */
	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
};

/**
 *  Makes the item a weapon mod: crafted with tradeskills and installed on a weapon by combining them (see CRAFTING.md)
 *  A weapon holds one mod per slot; the mod changes the weapon's stats while it's installed
 */
UCLASS(meta = (DisplayName = "Weapon Mod"))
class UZItemFragment_WeaponMod : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** The slot it goes into, e.g. Weapon.ModSlot.Barrel (the weapon needs that slot) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Mod", meta = (Categories = "Weapon.ModSlot"))
	FGameplayTag Slot;

	/** What it changes on the weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Mod")
	TArray<FZWeaponStatModifier> Modifiers;

	/** Only weapons of these types take it (empty: every weapon with the slot) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Mod")
	TArray<EZWeaponType> AllowedWeaponTypes;

	/** True if it fits this weapon (it has the slot, and the type is allowed) */
	bool FitsWeapon(const UZWeaponData* WeaponData) const;

	/** "Recoil -25%", one line per modifier */
	FText GetModifiersText() const;

	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
};

/**
 *  Makes the item usable from the inventory: heals and/or applies a gameplay effect, then one is used up
 *  (Later: a use time with an animation would be an ability started from here)
 */
UCLASS(meta = (DisplayName = "Consumable"))
class UZItemFragment_Consumable : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** Health restored when used */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0"))
	float HealAmount = 0.f;

	/** An extra effect on the user, e.g. stamina regeneration for a while (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	TSubclassOf<UGameplayEffect> EffectToApply;

	/** Takes one from the stack when used */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bConsumeOnUse = true;

	/** Heals and applies the effect; does nothing (and isn't used up) if it would have no effect, e.g. healing at full health */
	virtual bool OnUse(UZItemInstance* Instance, APawn* User) const override;

	virtual bool ShouldConsumeOnUse() const override { return bConsumeOnUse; }

	virtual FText GetUseActionName() const override;

	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;
};

/**
 *  Gives the item durability (Item.Stat.Durability), starting full. Tools lose some each time a recipe uses them
 *  Only for items that don't stack, since each one keeps its own durability
 */
UCLASS(meta = (DisplayName = "Durability"))
class UZItemFragment_Durability : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** Durability of a new item */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "1"))
	int32 MaxDurability = 20;

	/** Removed from the inventory when its durability runs out; otherwise it stays, broken, until it's repaired */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Durability")
	bool bBreaksWhenEmpty = true;

	virtual void OnInstanceCreated(UZItemInstance* Instance) const override;

	/** Durability between 0 and the max */
	virtual void OnInstanceLoaded(UZItemInstance* Instance) const override;

	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
};

/**
 *  Gives progression when used, e.g. a Tome of Knowledge (a trait point) found in a hidden place; then it's used up
 */
UCLASS(meta = (DisplayName = "Grants Progression"))
class UZItemFragment_Progression : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** Trait points the user gets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "0"))
	int32 TraitPoints = 1;

	/** Character experience the user gets */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "0"))
	int32 Experience = 0;

	virtual bool OnUse(UZItemInstance* Instance, APawn* User) const override;

	virtual bool ShouldConsumeOnUse() const override { return true; }

	virtual FText GetUseActionName() const override;

	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;
};

/**
 *  Makes the item a tool. Recipes ask for a kind of tool by its tags (e.g. Item.Tool.Screwdriver) and sometimes a minimum quality,
 *  so better tools (found in hidden places or crafted) open up harder recipes
 *  Give tools a Durability fragment too if they should wear out
 */
UCLASS(meta = (DisplayName = "Tool"))
class UZItemFragment_Tool : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** How good the tool is: 1 = basic, 2 = precision, 3 = advanced, ... It meets every recipe asking for this quality or less */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool", meta = (ClampMin = "1"))
	int32 Quality = 1;

	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
};

/**
 *  Makes the item armor or jewelry: worn in an equipment slot (e.g. Equipment.Slot.Torso or Equipment.Slot.Ring), it changes the player's stats,
 *  e.g. Armor (less damage taken) or MaxHealth. Using it from the inventory puts it on
 *  Only for items that don't stack, since each one is worn on its own
 */
UCLASS(meta = (DisplayName = "Wearable"))
class UZItemFragment_Wearable : public UZItemFragment
{
	GENERATED_BODY()

public:

	/** Where it's worn, e.g. Equipment.Slot.Head or Equipment.Slot.Ring (players need that slot: Project Settings → Game → Equipment) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wearable", meta = (Categories = "Equipment.Slot"))
	FGameplayTag Slot;

	/** What it changes while it's worn, e.g. Armor +10 or MaxHealth +15 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wearable")
	TArray<FZStatModifier> Modifiers;

	/** Putting it on (Source Object = the item). A child tag gives it its own sound (see AUDIO.md); empty = nothing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Item.Equip"))
	FGameplayTag EquipCue = ZGameplayTags::GameplayCue_Item_Equip;

	virtual FGameplayTag GetEquipmentSlotType() const override { return Slot; }

	virtual void GetEquippedStatModifiers(TArray<FZStatModifier>& OutModifiers) const override;

	/** Plays the Equip Cue */
	virtual void OnEquippedByPlayer(UZItemInstance* Instance, APawn* User) const override;

	/** Puts it on, in a free slot of its kind (or in place of what's in the first one) */
	virtual bool OnUse(UZItemInstance* Instance, APawn* User) const override;

	virtual FText GetUseActionName() const override;

	/** Where it's worn and what it changes */
	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const override;

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
};
