// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ZTagStack.generated.h"

struct FZTagStackContainer;

/** One gameplay tag with a number, e.g. Item.Stat.MagazineAmmo = 12 */
USTRUCT(BlueprintType)
struct FZTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FZTagStack() {}

	FZTagStack(FGameplayTag InTag, int32 InCount)
		: Tag(InTag)
		, Count(InCount)
	{
	}

private:

	friend FZTagStackContainer;

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 Count = 0;
};

/**
 *  Gameplay tags with a number each, for stats stored per item (magazine ammo, durability, ...)
 *  Replicated as a fast array, so only the changed stats are sent
 */
USTRUCT(BlueprintType)
struct FZTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	/** Adds to the tag's number (creates it if needed) */
	void AddStack(FGameplayTag Tag, int32 Count);

	/** Subtracts from the tag's number; removes the tag at 0 */
	void RemoveStack(FGameplayTag Tag, int32 Count);

	/** Sets the tag's number; 0 or less removes the tag */
	void SetStack(FGameplayTag Tag, int32 Count);

	/** The tag's number, 0 if it isn't there */
	int32 GetStackCount(FGameplayTag Tag) const { return TagToCount.FindRef(Tag); }

	bool ContainsTag(FGameplayTag Tag) const { return TagToCount.Contains(Tag); }

	/** Replaces all stats with another container's */
	void CopyFrom(const FZTagStackContainer& Other);

	/** All stats, e.g. for logs or UI */
	const TMap<FGameplayTag, int32>& GetAllStacks() const { return TagToCount; }

	// fast array callbacks, keep the lookup map in sync on clients
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FZTagStack, FZTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:

	/** The replicated list */
	UPROPERTY()
	TArray<FZTagStack> Stacks;

	/** Fast lookup, rebuilt from Stacks (not replicated) */
	TMap<FGameplayTag, int32> TagToCount;
};

template<>
struct TStructOpsTypeTraits<FZTagStackContainer> : public TStructOpsTypeTraitsBase2<FZTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
