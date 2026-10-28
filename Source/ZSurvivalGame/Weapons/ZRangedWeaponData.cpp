// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZRangedWeaponData.h"
#include "Weapons/ZWeaponStats.h"

float UZRangedWeaponData::GetBaseStat(EZWeaponStat Stat) const
{
	switch (Stat)
	{
	case EZWeaponStat::Damage:			return Damage;
	case EZWeaponStat::FireRate:		return FireRate;
	case EZWeaponStat::Spread:			return HipSpread;
	case EZWeaponStat::Recoil:			return RecoilPitch;
	case EZWeaponStat::Range:			return Range;
	case EZWeaponStat::MagazineSize:	return MagazineSize;
	case EZWeaponStat::ReloadTime:		return ReloadTime;
	case EZWeaponStat::PoiseDamage:		return PoiseDamage;
	default:							return 0.f;
	}
}
