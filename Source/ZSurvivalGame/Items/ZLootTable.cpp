// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZLootTable.h"
#include "Engine/World.h"
#include "ZSurvivalGame.h"
#include "Items/ZPickup.h"

namespace
{
	// drops lie in a ring around the location, this far out
	constexpr float LootRingRadius = 70.f;
}

TArray<FZItemStack> UZLootTable::RollLoot() const
{
	TArray<FZItemStack> Drops;
	for (const FZLootEntry& Entry : Entries)
	{
		if (Entry.Item && FMath::FRand() < Entry.Chance)
		{
			const int32 Count = FMath::RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount));
			Drops.Emplace(Entry.Item, Count);
		}
	}

	return Drops;
}

int32 UZLootTable::SpawnLoot(UWorld* World, const FVector& Location, TSubclassOf<AZPickup> PickupClass, float LifeSpan) const
{
	const TArray<FZItemStack> Drops = RollLoot();

	// spread around the location, starting at a random angle
	const float StartAngle = FMath::FRandRange(0.f, 2.f * PI);
	int32 NumSpawned = 0;

	for (int32 Index = 0; Index < Drops.Num(); ++Index)
	{
		const float Angle = StartAngle + 2.f * PI * Index / Drops.Num();
		const FVector Offset = Drops.Num() > 1 ? FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * LootRingRadius : FVector::ZeroVector;

		if (AZPickup::SpawnPickup(World, PickupClass, Drops[Index], Location + Offset, nullptr, true, LifeSpan))
		{
			++NumSpawned;
			UE_LOG(LogZSurvivalGame, Verbose, TEXT("Loot dropped: %s x%d"), *GetNameSafe(Drops[Index].Item), Drops[Index].Count);
		}
	}

	return NumSpawned;
}
