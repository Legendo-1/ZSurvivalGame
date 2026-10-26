// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ZAttributeSet.generated.h"

/** Broadcast on the server when Health reaches 0, with who caused it (null if unknown, e.g. engine damage) */
DECLARE_MULTICAST_DELEGATE_OneParam(FZOutOfHealthDelegate, AActor* /*DamageInstigator*/);

/**
 *  The player's attributes: health and stamina
 *  Lives on the PlayerState next to the ability system
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

	/** Broadcast on the server when Health reaches 0 */
	FZOutOfHealthDelegate OnOutOfHealth;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Keeps the current values in range */
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Keeps the base values in range */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/** Lowers Health/Stamina when their max drops, and reports running out of health */
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

private:

	/** Keeps Health and Stamina between 0 and their max, and the max values at 1 or more */
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
};
