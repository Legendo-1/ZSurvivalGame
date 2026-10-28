// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZMeleeWeaponData.h"
#include "Weapons/ZMeleeMoveset.h"
#include "Weapons/ZWeaponStats.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZWeapons"

UZMeleeWeaponData::UZMeleeWeaponData()
{
	WeaponType = EZWeaponType::MeleeOneHanded;
	EquipmentSlot = ZGameplayTags::Equipment_Slot_Melee;
}

float UZMeleeWeaponData::GetBaseStat(EZWeaponStat Stat) const
{
	switch (Stat)
	{
	case EZWeaponStat::Damage:			return Damage;
	case EZWeaponStat::PoiseDamage:		return PoiseDamage;
	case EZWeaponStat::AttackSpeed:		return AttackSpeed;
	case EZWeaponStat::StaminaCost:		return StaminaCostMultiplier;
	default:							return 0.f;
	}
}

#if WITH_EDITOR
EDataValidationResult UZMeleeWeaponData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!Moveset)
	{
		Context.AddError(LOCTEXT("NoMoveset", "The melee weapon has no Moveset, so it can't attack."));
	}

	if (EquipmentSlot.IsValid() && EquipmentSlot != ZGameplayTags::Equipment_Slot_Melee)
	{
		Context.AddWarning(LOCTEXT("NotMeleeSlot", "A melee weapon usually goes in the Melee slot (Equipment Slot)."));
	}

	if (WeaponType != EZWeaponType::MeleeOneHanded && WeaponType != EZWeaponType::MeleeTwoHanded)
	{
		Context.AddWarning(LOCTEXT("NotMeleeAnimations", "Weapon Type picks the walking animations: a melee weapon usually uses Melee One Handed or Melee Two Handed."));
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
