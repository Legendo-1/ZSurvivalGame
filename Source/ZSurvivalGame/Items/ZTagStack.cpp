// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZTagStack.h"

void FZTagStackContainer::AddStack(FGameplayTag Tag, int32 Count)
{
	if (!Tag.IsValid() || Count <= 0)
	{
		return;
	}

	for (FZTagStack& Stack : Stacks)
	{
		if (Stack.Tag == Tag)
		{
			Stack.Count += Count;
			TagToCount.Add(Tag, Stack.Count);
			MarkItemDirty(Stack);
			return;
		}
	}

	FZTagStack& NewStack = Stacks.Emplace_GetRef(Tag, Count);
	TagToCount.Add(Tag, Count);
	MarkItemDirty(NewStack);
}

void FZTagStackContainer::RemoveStack(FGameplayTag Tag, int32 Count)
{
	if (!Tag.IsValid() || Count <= 0)
	{
		return;
	}

	for (auto It = Stacks.CreateIterator(); It; ++It)
	{
		FZTagStack& Stack = *It;
		if (Stack.Tag != Tag)
		{
			continue;
		}

		if (Stack.Count <= Count)
		{
			It.RemoveCurrent();
			TagToCount.Remove(Tag);
			MarkArrayDirty();
		}
		else
		{
			Stack.Count -= Count;
			TagToCount.Add(Tag, Stack.Count);
			MarkItemDirty(Stack);
		}

		return;
	}
}

void FZTagStackContainer::SetStack(FGameplayTag Tag, int32 Count)
{
	if (!Tag.IsValid())
	{
		return;
	}

	const int32 CurrentCount = GetStackCount(Tag);
	if (Count > CurrentCount)
	{
		AddStack(Tag, Count - CurrentCount);
	}
	else if (Count < CurrentCount)
	{
		RemoveStack(Tag, CurrentCount - Count);
	}
}

void FZTagStackContainer::CopyFrom(const FZTagStackContainer& Other)
{
	// remove stats the other doesn't have, then set the rest
	TArray<FGameplayTag> CurrentTags;
	TagToCount.GetKeys(CurrentTags);
	for (const FGameplayTag& Tag : CurrentTags)
	{
		if (!Other.ContainsTag(Tag))
		{
			SetStack(Tag, 0);
		}
	}

	for (const TPair<FGameplayTag, int32>& Pair : Other.TagToCount)
	{
		SetStack(Pair.Key, Pair.Value);
	}
}

void FZTagStackContainer::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (const int32 Index : RemovedIndices)
	{
		TagToCount.Remove(Stacks[Index].Tag);
	}
}

void FZTagStackContainer::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (const int32 Index : AddedIndices)
	{
		const FZTagStack& Stack = Stacks[Index];
		TagToCount.Add(Stack.Tag, Stack.Count);
	}
}

void FZTagStackContainer::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (const int32 Index : ChangedIndices)
	{
		const FZTagStack& Stack = Stacks[Index];
		TagToCount.Add(Stack.Tag, Stack.Count);
	}
}
