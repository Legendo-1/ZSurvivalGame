// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayEffect_Heal.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"

UZGameplayEffect_Heal::UZGameplayEffect_Heal()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// IncomingHealing += the healing given by the caller
	FSetByCallerFloat HealingFromCaller;
	HealingFromCaller.DataTag = ZGameplayTags::SetByCaller_Healing;

	FGameplayModifierInfo HealingModifier;
	HealingModifier.Attribute = UZAttributeSet::GetIncomingHealingAttribute();
	HealingModifier.ModifierOp = EGameplayModOp::AddBase;
	HealingModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealingFromCaller);
	Modifiers.Add(HealingModifier);
}
