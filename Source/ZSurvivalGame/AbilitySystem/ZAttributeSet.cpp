// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UZAttributeSet::UZAttributeSet()
{
	InitHealth(100.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitIncomingDamage(0.f);
	InitIncomingHealing(0.f);
}

void UZAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// everyone gets health (e.g. for teammates' health bars); only the owning player needs stamina, for the HUD
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, Stamina, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UZAttributeSet, MaxStamina, COND_OwnerOnly, REPNOTIFY_Always);
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

	// lowering a max also lowers the current value
	if (Attribute == GetMaxHealthAttribute() && GetHealth() > NewValue)
	{
		SetHealth(NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && GetStamina() > NewValue)
	{
		SetStamina(NewValue);
	}

	if (Attribute == GetHealthAttribute() && OldValue > 0.f && NewValue <= 0.f)
	{
		OnOutOfHealth.Broadcast(CurrentDamageInstigator.Get());
	}
}

void UZAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// damage effects add to IncomingDamage; turn it into lost health
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageTaken = GetIncomingDamage();
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
