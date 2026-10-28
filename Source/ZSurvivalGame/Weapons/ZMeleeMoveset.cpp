// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZMeleeMoveset.h"
#include "Animation/AnimMontage.h"
#include "Combat/ZAnimNotifyState_WeaponHitbox.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZWeapons"

const FZMeleeAttack* UZMeleeMoveset::GetAttack(EZMeleeAttackType Type, int32 Index) const
{
	const TArray<FZMeleeAttack>& Attacks = GetAttacks(Type);
	return Attacks.IsValidIndex(Index) ? &Attacks[Index] : nullptr;
}

int32 UZMeleeMoveset::GetFollowUp(EZMeleeAttackType CurrentType, int32 CurrentIndex, EZMeleeAttackType NextType) const
{
	const int32 NumNext = GetAttacks(NextType).Num();
	if (NumNext <= 0)
	{
		return INDEX_NONE;
	}

	// the attack's own choice (numbers from 1), if it made one that exists
	if (const FZMeleeAttack* Current = GetAttack(CurrentType, CurrentIndex))
	{
		const int32 Chosen = (NextType == EZMeleeAttackType::Light ? Current->NextLightAttack : Current->NextHeavyAttack) - 1;
		if (Chosen >= 0 && Chosen < NumNext)
		{
			return Chosen;
		}
	}

	// otherwise the next one of the same combo (starting again after the last), or the first of the other combo
	return CurrentType == NextType ? (CurrentIndex + 1) % NumNext : 0;
}

#if WITH_EDITOR
EDataValidationResult UZMeleeMoveset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (LightAttacks.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoLightAttacks", "The moveset has no Light Attacks, so the left mouse button does nothing with its weapons."));
	}

	const auto CheckAttacks = [this, &Context](const TArray<FZMeleeAttack>& Attacks, const FText& ListName)
	{
		for (int32 Index = 0; Index < Attacks.Num(); ++Index)
		{
			const FZMeleeAttack& Attack = Attacks[Index];
			const FText AttackName = FText::Format(LOCTEXT("AttackName", "{0} {1}"), ListName, Index + 1);
			if (!Attack.Montage)
			{
				Context.AddError(FText::Format(LOCTEXT("NoMontage", "{0} has no Montage."), AttackName));
				continue;
			}

			if (!Attack.Section.IsNone() && Attack.Montage->GetSectionIndex(Attack.Section) == INDEX_NONE)
			{
				Context.AddError(FText::Format(LOCTEXT("NoSection", "{0}: the montage '{1}' has no section '{2}'."), AttackName, FText::FromString(Attack.Montage->GetName()), FText::FromName(Attack.Section)));
			}

			if (UZAnimNotifyState_WeaponHitbox::CountHitboxes(Attack.Montage, Attack.Section) <= 0)
			{
				Context.AddWarning(FText::Format(LOCTEXT("NoHitbox", "{0}: the montage '{1}' has no Weapon Hitbox notify, so the attack never hits."), AttackName, FText::FromString(Attack.Montage->GetName())));
			}

			if (Attack.NextLightAttack > LightAttacks.Num() || Attack.NextHeavyAttack > HeavyAttacks.Num())
			{
				Context.AddWarning(FText::Format(LOCTEXT("BadFollowUp", "{0}: Next Light Attack or Next Heavy Attack is a number past the end of its list; the next one in order plays instead."), AttackName));
			}
		}
	};

	CheckAttacks(LightAttacks, LOCTEXT("LightList", "Light attack"));
	CheckAttacks(HeavyAttacks, LOCTEXT("HeavyList", "Heavy attack"));

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
