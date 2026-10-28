// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZWeaponData.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZWeapons"

#if WITH_EDITOR
EDataValidationResult UZWeaponData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// it starts on every machine from the equip cue, without prediction, so moving the character would put its owner out of step with the server
	if (EquipMontage && EquipMontage->HasRootMotion())
	{
		Context.AddError(FText::Format(LOCTEXT("EquipMontageRootMotion", "The Equip Montage '{0}' moves the character (root motion), so it isn't played. Take-out animations must stay in place: turn off Enable Root Motion in its animations, or pick another montage."),
			FText::FromString(EquipMontage->GetName())));
	}

	if (!EquipmentSlot.IsValid())
	{
		Context.AddError(LOCTEXT("NoEquipmentSlot", "The weapon has no Equipment Slot (Short Gun, Long Gun or Melee), so it can't be equipped or taken out."));
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
