// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ZItemDefinition.h"
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

	/** Puts the weapon in the user's hands */
	virtual bool OnUse(UZItemInstance* Instance, APawn* User) const override;

	virtual FText GetUseActionName() const override;

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

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
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

#if WITH_EDITOR
	virtual void ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const override;
#endif
};
