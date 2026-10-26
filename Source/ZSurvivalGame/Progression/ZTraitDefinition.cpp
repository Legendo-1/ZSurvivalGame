// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression/ZTraitDefinition.h"
#include "AbilitySystem/ZAttributeSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZProgression"

FPrimaryAssetId UZTraitDefinition::GetPrimaryAssetId() const
{
	// every trait (also of child classes) is the same asset type
	return FPrimaryAssetId(GetTraitAssetType(), GetFName());
}

FText UZTraitDefinition::GetTraitName() const
{
	return DisplayName.IsEmpty() ? FText::FromName(GetFName()) : DisplayName;
}

FText UZTraitDefinition::GetEffectText(int32 Rank) const
{
	TArray<FText> Lines;
	for (const FZTraitModifier& Modifier : Modifiers)
	{
		if (Modifier.Attribute.IsValid())
		{
			Lines.Add(DescribeBonus(Modifier.Attribute, Modifier.ValuePerRank * Rank));
		}
	}

	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

FText UZTraitDefinition::DescribeBonus(const FGameplayAttribute& Attribute, float Value)
{
	// "DamageMultiplier" → "Damage", "MaxHealth" → "Max Health"
	FString Name = Attribute.GetName();
	const bool bMultiplier = Name.RemoveFromEnd(TEXT("Multiplier"));
	const bool bChance = Name.EndsWith(TEXT("Chance"));
	const FText StatName = FText::FromString(FName::NameToDisplayString(Name, false));

	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 1;
	Format.AlwaysSign = true;

	if (bMultiplier || bChance)
	{
		return FText::Format(LOCTEXT("PercentBonus", "{0} {1}%"), StatName, FText::AsNumber(Value * 100.f, &Format));
	}

	return FText::Format(LOCTEXT("NumberBonus", "{0} {1}"), StatName, FText::AsNumber(Value, &Format));
}

#if WITH_EDITOR
EDataValidationResult UZTraitDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("TraitNoName", "The trait has no Display Name."));
	}

	if (Modifiers.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("TraitNoModifiers", "The trait has no Modifiers, so it does nothing."));
	}

	const TArray<FGameplayAttribute> TraitAttributes = UZAttributeSet::GetTraitAttributes();
	for (const FZTraitModifier& Modifier : Modifiers)
	{
		if (!TraitAttributes.Contains(Modifier.Attribute))
		{
			Context.AddError(FText::Format(LOCTEXT("TraitBadAttribute", "The modifier for '{0}' can't work: traits can only change {1}."),
				FText::FromString(Modifier.Attribute.GetName()), FText::FromString(TEXT("MaxHealth, MaxStamina and the stats in ZAttributeSet (Damage, Recoil, Reload Speed, Dodge Distance, Stamina Regen, Crafting Yield Chance, Tradeskill Experience)"))));
		}
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
