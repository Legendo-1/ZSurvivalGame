// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/ZItemDefinition.h"
#include "ZLootTable.generated.h"

class AZPickup;

/** One possible drop: an item, how many, and the chance it drops */
USTRUCT(BlueprintType)
struct FZLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UZItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;

	/** Chance to drop: 1 = always, 0.25 = one in four */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;
};

/**
 *  What an enemy (later also a container or a harvest node) can drop. Every entry is rolled on its own
 *  Make one per kind of enemy and share it: right-click → Miscellaneous → Data Asset → ZLootTable
 */
UCLASS(BlueprintType)
class UZLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TArray<FZLootEntry> Entries;

	/** Rolls every entry and returns what dropped */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	TArray<FZItemStack> RollLoot() const;

	/** Server: rolls the table and spawns what dropped as pickups around a location. Returns how many pickups were spawned */
	int32 SpawnLoot(UWorld* World, const FVector& Location, TSubclassOf<AZPickup> PickupClass, float LifeSpan) const;
};
