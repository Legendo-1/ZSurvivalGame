// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ZAttributeSet.generated.h"

/** Broadcast on the server when Health reaches 0, with who caused it (null if unknown, e.g. engine damage) */
DECLARE_MULTICAST_DELEGATE_OneParam(FZOutOfHealthDelegate, AActor* /*DamageInstigator*/);

/**
 *  A character's attributes: health and stamina, and the stats traits change (damage, recoil, reload speed, ...)
 *  Players have it on the PlayerState next to the ability system, AI characters on themselves
 *  Values only change on the server and replicate to the clients
 */
UCLASS()
class UZAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UZAttributeSet();

	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, Health);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, Stamina);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, MaxStamina);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, IncomingDamage);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, IncomingHealing);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, DamageMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, RecoilMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, ReloadSpeedMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, DodgeDistanceMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, StaminaRegenMultiplier);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, CraftingYieldChance);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, TradeskillExperienceMultiplier);

	/** The attributes traits can change (everything but the current values and the meta attributes) */
	static TArray<FGameplayAttribute> GetTraitAttributes();

	/** An attribute of an actor (or of its PlayerState's ability system), or Default if it has none */
	static float GetAttributeValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Default);

	/** An attribute by its name, e.g. "MaxHealth" or "DamageMultiplier" (not valid if there's none), e.g. for editor scripts */
	UFUNCTION(BlueprintPure, Category = "Attributes")
	static FGameplayAttribute FindAttributeByName(FName AttributeName);

	/** Broadcast on the server when Health reaches 0 */
	FZOutOfHealthDelegate OnOutOfHealth;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Keeps the current values in range */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Keeps the base values in range */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/** Keeps the same share of Health/Stamina when their max changes, and reports running out of health */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	/** Turns IncomingDamage and IncomingHealing from gameplay effects into Health changes */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	void OnRep_DamageMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RecoilMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ReloadSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_DodgeDistanceMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_StaminaRegenMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CraftingYieldChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_TradeskillExperienceMultiplier(const FGameplayAttributeData& OldValue);

private:

	/** Keeps Health and Stamina between 0 and their max, the max values at 1 or more, multipliers above 0 and chances between 0 and 1 */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	/** Who is dealing the damage being applied right now (set only during PostGameplayEffectExecute) */
	TWeakObjectPtr<AActor> CurrentDamageInstigator;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes|Stamina", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Stamina;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes|Stamina", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxStamina;

	/** Damage about to be taken. Not replicated: the server turns it into a Health change right away */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingDamage;

	/** Healing about to be received. Not replicated: the server turns it into a Health change right away */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingHealing;

	/** --- STATS, changed by traits (1 = normal) --- */

	/** Weapon damage dealt */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DamageMultiplier, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData DamageMultiplier;

	/** Weapon recoil (lower kicks less) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_RecoilMultiplier, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData RecoilMultiplier;

	/** How fast reloading is (higher is faster) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadSpeedMultiplier, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData ReloadSpeedMultiplier;

	/** How far a dodge goes */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_DodgeDistanceMultiplier, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData DodgeDistanceMultiplier;

	/** How fast stamina comes back */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegenMultiplier, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData StaminaRegenMultiplier;

	/** Chance (0..1) that a craft gives its results twice */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CraftingYieldChance, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData CraftingYieldChance;

	/** Tradeskill experience from crafting */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TradeskillExperienceMultiplier, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData TradeskillExperienceMultiplier;
};
