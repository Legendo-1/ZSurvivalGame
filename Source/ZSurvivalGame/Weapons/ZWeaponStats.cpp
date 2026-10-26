// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZWeaponStats.h"
#include "Items/ZItemFragments.h"
#include "Items/ZItemInstance.h"
#include "Weapons/ZWeaponData.h"

#define LOCTEXT_NAMESPACE "ZWeapons"

float UZWeaponStatsLibrary::GetWeaponStat(const UZWeaponData* WeaponData, const UZItemInstance* Item, EZWeaponStat Stat)
{
	if (!WeaponData)
	{
		return 0.f;
	}

	const float Value = GetBaseWeaponStat(WeaponData, Stat) * GetItemStatScale(WeaponData, Item, Stat);
	return Stat == EZWeaponStat::MagazineSize ? FMath::Max(FMath::RoundToFloat(Value), 1.f) : Value;
}

float UZWeaponStatsLibrary::GetItemStatScale(const UZWeaponData* WeaponData, const UZItemInstance* Item, EZWeaponStat Stat)
{
	// spread and recoil can go away completely; anything else gets small, but not 0 (e.g. a fire rate of 0 would never fire)
	const float MinScale = Stat == EZWeaponStat::Spread || Stat == EZWeaponStat::Recoil ? 0.f : 0.05f;
	return FMath::Max(1.f + GetItemStatBonus(WeaponData, Item, Stat), MinScale);
}

float UZWeaponStatsLibrary::GetBaseWeaponStat(const UZWeaponData* WeaponData, EZWeaponStat Stat)
{
	if (!WeaponData)
	{
		return 0.f;
	}

	switch (Stat)
	{
	case EZWeaponStat::Damage:			return WeaponData->Damage;
	case EZWeaponStat::FireRate:		return WeaponData->FireRate;
	case EZWeaponStat::Spread:			return WeaponData->HipSpread;
	case EZWeaponStat::Recoil:			return WeaponData->RecoilPitch;
	case EZWeaponStat::Range:			return WeaponData->Range;
	case EZWeaponStat::MagazineSize:	return WeaponData->MagazineSize;
	case EZWeaponStat::ReloadTime:		return WeaponData->ReloadTime;
	default:							return 0.f;
	}
}

float UZWeaponStatsLibrary::GetItemStatBonus(const UZWeaponData* WeaponData, const UZItemInstance* Item, EZWeaponStat Stat)
{
	if (!Item)
	{
		return 0.f;
	}

	float Bonus = 0.f;
	for (const UZItemDefinition* Mod : Item->GetAttachedMods())
	{
		const UZItemFragment_WeaponMod* ModFragment = Mod ? Mod->FindFragment<UZItemFragment_WeaponMod>() : nullptr;
		if (!ModFragment)
		{
			continue;
		}

		for (const FZWeaponStatModifier& Modifier : ModFragment->Modifiers)
		{
			if (Modifier.Stat == Stat)
			{
				Bonus += Modifier.Bonus;
			}
		}
	}

	// upgrades make it hit harder
	if (Stat == EZWeaponStat::Damage && WeaponData)
	{
		Bonus += Item->GetUpgradeLevel() * WeaponData->DamagePerUpgradeLevel;
	}

	return Bonus;
}

FText UZWeaponStatsLibrary::GetStatName(EZWeaponStat Stat)
{
	switch (Stat)
	{
	case EZWeaponStat::Damage:			return LOCTEXT("StatDamage", "Damage");
	case EZWeaponStat::FireRate:		return LOCTEXT("StatFireRate", "Fire Rate");
	case EZWeaponStat::Spread:			return LOCTEXT("StatSpread", "Spread");
	case EZWeaponStat::Recoil:			return LOCTEXT("StatRecoil", "Recoil");
	case EZWeaponStat::Range:			return LOCTEXT("StatRange", "Range");
	case EZWeaponStat::MagazineSize:	return LOCTEXT("StatMagazineSize", "Magazine Size");
	case EZWeaponStat::ReloadTime:		return LOCTEXT("StatReloadTime", "Reload Time");
	default:							return FText::GetEmpty();
	}
}

FText UZWeaponStatsLibrary::DescribeModifier(const FZWeaponStatModifier& Modifier)
{
	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 0;
	Format.AlwaysSign = true;
	return FText::Format(LOCTEXT("ModifierFormat", "{0} {1}%"), GetStatName(Modifier.Stat), FText::AsNumber(Modifier.Bonus * 100.f, &Format));
}

#undef LOCTEXT_NAMESPACE
