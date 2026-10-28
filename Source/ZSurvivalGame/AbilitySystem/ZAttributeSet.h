// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ZAttributeSet.generated.h"

/** Broadcast on the server when Health reaches 0, with who caused it (null if unknown, e.g. engine damage) */
DECLARE_MULTICAST_DELEGATE_OneParam(FZOutOfHealthDelegate, AActor* /*DamageInstigator*/);

/** Broadcast on the server when poise breaks (the character staggers), with who broke it (null if unknown) */
DECLARE_MULTICAST_DELEGATE_OneParam(FZPoiseBrokenDelegate, AActor* /*Instigator*/);

/**
 *  A character's attributes: health, stamina and poise, and the stats traits and equipment change (damage, recoil, reload speed, armor, ...)
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
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, Armor);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, Poise);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, MaxPoise);
	ATTRIBUTE_ACCESSORS_BASIC(UZAttributeSet, IncomingPoiseDamage);

	/** The attributes traits and equipment can change (everything but the current values and the meta attributes) */
	static TArray<FGameplayAttribute> GetTraitAttributes();

	/** What's left of damage after armor: Damage × N / (N + Armor), N = Armor For Half Damage (Project Settings → Game → Equipment) */
	static float GetDamageAfterArmor(float Damage, float ArmorValue);

	/** An attribute of an actor (or of its PlayerState's ability system), or Default if it has none */
	static float GetAttributeValue(const AActor* Actor, const FGameplayAttribute& Attribute, float Default);

	/** An attribute by its name, e.g. "MaxHealth" or "DamageMultiplier" (not valid if there's none), e.g. for editor scripts */
	UFUNCTION(BlueprintPure, Category = "Attributes")
	static FGameplayAttribute FindAttributeByName(FName AttributeName);

	/** Broadcast on the server when Health reaches 0 */
	FZOutOfHealthDelegate OnOutOfHealth;

	/** Broadcast on the server when Poise reaches 0: the character staggers, and its poise is full again */
	FZPoiseBrokenDelegate OnPoiseBroken;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Keeps the current values in range */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Keeps the base values in range */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/** Keeps Health/Stamina within a lower max (a higher max leaves a gap to heal), and reports running out of health */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	/** Turns IncomingDamage and IncomingHealing from gameplay effects into Health changes, and IncomingPoiseDamage into lost poise */
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

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Poise(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxPoise(const FGameplayAttributeData& OldValue);

private:

	/** Keeps Health, Stamina and Poise between 0 and their max, max health and stamina at 1 or more, multipliers above 0, chances between 0 and 1, and armor and max poise at 0 or more */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	/** Server: poise damage taken; poise breaks at 0 */
	void ApplyPoiseDamage(float PoiseDamage, AActor* Instigator);

	/** Server: poise is full again (a while after the last poise damage) */
	void RecoverPoise();

	/** Server: refills poise once no poise damage came for Poise Recovery Delay (Project Settings → Game → Combat) */
	FTimerHandle PoiseRecoveryTimer;

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

	/** Lowers the damage taken (0 = none; how much: GetDamageAfterArmor). Mostly from armor that's worn (equipment) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Attributes|Stats", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Armor;

	/** --- POISE: how much it takes to stagger the character. Hits take some (their poise damage); at 0 it staggers, and a while without hits fills it again --- */

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Poise, Category = "Attributes|Poise", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData Poise;

	/** Full poise; traits and heavy armor can raise it */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPoise, Category = "Attributes|Poise", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData MaxPoise;

	/** Poise damage about to be taken. Not replicated: the server turns it into a Poise change right away */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta", meta = (AllowPrivateAccess = "true"))
	FGameplayAttributeData IncomingPoiseDamage;
};
