// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Items/ZEquipmentSettings.h"

UZAttributeSet::UZAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitIncomingDamage(0.f);
	InitIncomingHealing(0.f);
	InitDamageMultiplier(1.f);
	InitRecoilMultiplier(1.f);
	InitReloadSpeedMultiplier(1.f);
	InitDodgeDistanceMultiplier(1.f);
	InitStaminaRegenMultiplier(1.f);
	InitCraftingYieldChance(0.f);
	InitTradeskillExperienceMultiplier(1.f);
	InitArmor(0.f);
}

TArray<FGameplayAttribute> UZAttributeSet::GetTraitAttributes()
{
	return {
		GetMaxHealthAttribute(),
		GetMaxStaminaAttribute(),
		GetDamageMultiplierAttribute(),
		GetRecoilMultiplierAttribute(),
		GetReloadSpeedMultiplierAttribute(),
		GetDodgeDistanceMultiplierAttribute(),
		GetStaminaRegenMultiplierAttribute(),
		GetCraftingYieldChanceAttribute(),
		GetTradeskillExperienceMultiplierAttribute(),
		GetArmorAttribute()
	};
}

float UZAttributeSet::GetDamageAfterArmor(float Damage, float ArmorValue)
{
	// each bit of armor helps a little less than the one before, and damage never reaches 0
	const float HalfDamageArmor = UZEquipmentSettings::Get()->ArmorForHalfDamage;
	return ArmorValue > 0.f && HalfDamageArmor > 0.f ? Damage * HalfDamageArmor / (HalfDamageArmor + ArmorValue) : Damage;
}

float UZAttributeSet::GetAttributeValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Default)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : Default;
}

FGameplayAttribute UZAttributeSet::FindAttributeByName(FName AttributeName)
{
	FProperty* Property = FindFProperty<FProperty>(StaticClass(), AttributeName);
	return Property && FGameplayAttribute::IsGameplayAttributeDataProperty(Property) ? FGameplayAttribute(Property) : FGameplayAttribute();
}

void UZAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// everyone gets health (e.g. for teammates' health bars); only the owning player needs stamina, for the HUD
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, Stamina, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, MaxStamina, COND_OwnerOnly, REPNOTIFY_Always);

	// the stats only matter to the owning player (the server uses its own values)
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, DamageMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, RecoilMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, ReloadSpeedMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, DodgeDistanceMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, StaminaRegenMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, CraftingYieldChance, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, TradeskillExperienceMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, Armor, COND_OwnerOnly, REPNOTIFY_Always);
}

void UZAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UZAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	ClampAttribute(Attribute, NewValue);
}

void UZAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// only the server changes attributes; clients just receive the values
	const AActor* OwningActor = GetOwningActor();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return;
	}

	// a changed max keeps the same share of the current value (e.g. a trait adding max health heals that part too)
	if (Attribute == GetMaxHealthAttribute() && OldValue > 0.f && GetHealth() > 0.f)
	{
		SetHealth(FMath::Min(GetHealth() * NewValue / OldValue, NewValue));
	}
	else if (Attribute == GetMaxStaminaAttribute() && OldValue > 0.f)
	{
		SetStamina(FMath::Min(GetStamina() * NewValue / OldValue, NewValue));
	}

	if (Attribute == GetHealthAttribute() && OldValue > 0.f && NewValue <= 0.f)
	{
		OnOutOfHealth.Broadcast(CurrentDamageInstigator.Get());
	}
}

void UZAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// damage effects add to IncomingDamage; turn it into lost health (less with armor)
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageTaken = GetDamageAfterArmor(GetIncomingDamage(), GetArmor());
		SetIncomingDamage(0.f);

		if (DamageTaken > 0.f)
		{
			// remember who did it while the health changes, so OnOutOfHealth can tell
			CurrentDamageInstigator = Data.EffectSpec.GetContext().GetInstigator();
			SetHealth(GetHealth() - DamageTaken);
			CurrentDamageInstigator = nullptr;
		}
	}

	// healing effects add to IncomingHealing; turn it into health (the dead stay dead)
	if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float HealingReceived = GetIncomingHealing();
		SetIncomingHealing(0.f);

		if (HealingReceived > 0.f && GetHealth() > 0.f)
		{
			SetHealth(GetHealth() + HealingReceived);
		}
	}
}

void UZAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetCraftingYieldChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	}
	else if (Attribute == GetArmorAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
	else if (Attribute == GetDamageMultiplierAttribute() || Attribute == GetRecoilMultiplierAttribute() || Attribute == GetReloadSpeedMultiplierAttribute() || Attribute == GetDodgeDistanceMultiplierAttribute() || Attribute == GetStaminaRegenMultiplierAttribute() || Attribute == GetTradeskillExperienceMultiplierAttribute())
	{
		// a multiplier of 0 or less would break things (e.g. dividing the reload time by it)
		NewValue = FMath::Max(NewValue, 0.05f);
	}
}

void UZAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, Health, OldHealth);
}

void UZAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, MaxHealth, OldMaxHealth);
}

void UZAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, Stamina, OldStamina);
}

void UZAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, MaxStamina, OldMaxStamina);
}
void UZAttributeSet::OnRep_DamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, DamageMultiplier, OldValue);
}

void UZAttributeSet::OnRep_RecoilMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, RecoilMultiplier, OldValue);
}

void UZAttributeSet::OnRep_ReloadSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, ReloadSpeedMultiplier, OldValue);
}

void UZAttributeSet::OnRep_DodgeDistanceMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, DodgeDistanceMultiplier, OldValue);
}

void UZAttributeSet::OnRep_StaminaRegenMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, StaminaRegenMultiplier, OldValue);
}

void UZAttributeSet::OnRep_CraftingYieldChance(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, CraftingYieldChance, OldValue);
}

void UZAttributeSet::OnRep_TradeskillExperienceMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, TradeskillExperienceMultiplier, OldValue);
}

void UZAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UZAttributeSet, Armor, OldValue);
}
