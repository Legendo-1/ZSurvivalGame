// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "ZEquipmentSettings.generated.h"

/** One kind of equipment slot and how many of it a player has, e.g. 4 ring slots */
USTRUCT(BlueprintType)
struct FZEquipmentSlotType
{
	GENERATED_BODY()

	/** The kind of slot; items with the same tag go in it (a weapon's weapon data, a Wearable fragment) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag Type;

	/** How many slots of this kind */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "1", ClampMax = "8"))
	int32 Count = 1;

	/** Name in the UI, e.g. "Long gun" (the tag's last part if empty) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FText DisplayName;

	/** The equipment window shows the slots of a group together under this title, e.g. "Weapons" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FText Group;
};

/**
 *  The equipment slots players have, and what armor does
 *  Project Settings → Game → Equipment (saved in Config/DefaultGame.ini)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Equipment"))
class UZEquipmentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UZEquipmentSettings();

	static const UZEquipmentSettings* Get() { return GetDefault<UZEquipmentSettings>(); }

	/** Every kind of slot, in the order the equipment window shows them. A new kind (e.g. hands): a tag under Equipment.Slot (Project Settings → Gameplay Tags) and a line here */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Slots")
	TArray<FZEquipmentSlotType> SlotTypes;

	/** Armor that halves the damage taken. Damage × N / (N + Armor): with 100, 50 armor takes 67%, 100 takes 50%, 300 takes 25% */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (ClampMin = "1"))
	float ArmorForHalfDamage = 100.f;

	/** The settings for a kind of slot, or null if players have none of it */
	const FZEquipmentSlotType* FindSlotType(const FGameplayTag& Type) const;

	/** How many slots of this kind players have (0 if none) */
	int32 GetSlotCount(const FGameplayTag& Type) const;

	/** "Long gun" for Equipment.Slot.LongGun */
	FText GetSlotTypeName(const FGameplayTag& Type) const;
};
