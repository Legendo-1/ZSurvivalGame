// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayEffect_Damage.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"

UZGameplayEffect_Damage::UZGameplayEffect_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// IncomingDamage += the damage given by the caller
	FSetByCallerFloat DamageFromCaller;
	DamageFromCaller.DataTag = ZGameplayTags::SetByCaller_Damage;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = UZAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::AddBase;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageFromCaller);
	Modifiers.Add(DamageModifier);
}
