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

	// IncomingPoiseDamage += the poise damage given by the caller
	FSetByCallerFloat PoiseDamageFromCaller;
	PoiseDamageFromCaller.DataTag = ZGameplayTags::SetByCaller_PoiseDamage;

	FGameplayModifierInfo PoiseModifier;
	PoiseModifier.Attribute = UZAttributeSet::GetIncomingPoiseDamageAttribute();
	PoiseModifier.ModifierOp = EGameplayModOp::AddBase;
	PoiseModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(PoiseDamageFromCaller);
	Modifiers.Add(PoiseModifier);
}
