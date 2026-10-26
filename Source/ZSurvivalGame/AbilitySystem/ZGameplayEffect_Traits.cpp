// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayEffect_Traits.h"
#include "AbilitySystem/ZAttributeSet.h"

UZGameplayEffect_Traits::UZGameplayEffect_Traits()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// one modifier per attribute traits can change; the bonus for each is filled in when it's applied
	for (const FGameplayAttribute& Attribute : UZAttributeSet::GetTraitAttributes())
	{
		FSetByCallerFloat BonusFromCaller;
		BonusFromCaller.DataName = GetMagnitudeName(Attribute);

		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::AddBase;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(BonusFromCaller);
	}
}

FName UZGameplayEffect_Traits::GetMagnitudeName(const FGameplayAttribute& Attribute)
{
	return FName(*(TEXT("Trait.") + Attribute.GetName()));
}
