// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZWeaponStats.generated.h"

class UZItemInstance;
class UZWeaponDataBase;

/** A weapon stat that upgrades and mods can change. Each one is the weapon data's value times (1 + the bonuses) */
UENUM(BlueprintType)
enum class EZWeaponStat : uint8
{
	/** Damage per bullet, or per hit of a melee weapon */
	Damage,
	/** Shots per second */
	FireRate,
	/** Spread cone, from the hip and while aiming */
	Spread,
	/** Camera kick per shot */
	Recoil,
	/** How far shots reach */
	Range,
	/** Rounds per magazine */
	MagazineSize,
	/** Seconds a reload takes (lower is faster) */
	ReloadTime,
	/** How much each hit wears down the target's poise toward a stagger */
	PoiseDamage,
	/** How fast melee attacks play */
	AttackSpeed,
	/** Stamina melee attacks cost (lower is cheaper) */
	StaminaCost
};

/** A change to one weapon stat, as a share of its normal value */
USTRUCT(BlueprintType)
struct FZWeaponStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	EZWeaponStat Stat = EZWeaponStat::Damage;

	/** 0.15 = +15%, -0.25 = -25% (less spread, recoil and reload time are good, so those are usually negative) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Bonus = 0.f;
};

/**
 *  A weapon item's stats: its weapon data changed by its upgrade level and its mods (not by the traits of who holds it; AZWeapon adds those)
 *  For the UI too, e.g. to show a weapon's damage in the inventory
 */
UCLASS()
class UZWeaponStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** The stat for this weapon data and item (the item may be null: just the data) */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static float GetWeaponStat(const UZWeaponDataBase* WeaponData, const UZItemInstance* Item, EZWeaponStat Stat);

	/** The stat straight from the weapon data, without upgrades or mods */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static float GetBaseWeaponStat(const UZWeaponDataBase* WeaponData, EZWeaponStat Stat);

	/** What the item's upgrade level and mods multiply the stat by: 1.2 = 20% more (at least 0.05, or 0 for spread and recoil) */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static float GetItemStatScale(const UZWeaponDataBase* WeaponData, const UZItemInstance* Item, EZWeaponStat Stat);

	/** How much the item's upgrade level and mods change the stat: 0.2 = 20% more */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static float GetItemStatBonus(const UZWeaponDataBase* WeaponData, const UZItemInstance* Item, EZWeaponStat Stat);

	/** "Damage", "Fire Rate", ... */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	static FText GetStatName(EZWeaponStat Stat);

	/** "Damage +15%" or "Recoil -25%" */
	static FText DescribeModifier(const FZWeaponStatModifier& Modifier);
};
