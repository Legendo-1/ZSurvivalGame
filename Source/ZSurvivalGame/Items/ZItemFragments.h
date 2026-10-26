// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/ZItemDefinition.h"
#include "ZItemFragments.generated.h"

class UGameplayEffect;
class UZWeaponData;

/**
 *  Makes the item a weapon: equipping it puts the weapon in the character's hands
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
};
