// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemInstance.h"
#include "Net/UnrealNetwork.h"

void UZItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UZItemInstance, ItemDefinition);
	DOREPLIFETIME(UZItemInstance, StatTags);
}

void UZItemInstance::InitializeInstance(UZItemDefinition* InItemDefinition)
{
	ItemDefinition = InItemDefinition;

	if (ItemDefinition)
	{
		for (const UZItemFragment* Fragment : ItemDefinition->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnInstanceCreated(this);
			}
		}
	}
}

const UZItemFragment* UZItemInstance::FindFragmentByClass(TSubclassOf<UZItemFragment> FragmentClass) const
{
	return ItemDefinition ? ItemDefinition->FindFragmentByClass(FragmentClass) : nullptr;
}

int32 UZItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

void UZItemInstance::AddStatTagStack(FGameplayTag Tag, int32 Count)
{
	StatTags.AddStack(Tag, Count);
}

void UZItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 Count)
{
	StatTags.RemoveStack(Tag, Count);
}

void UZItemInstance::SetStatTagStack(FGameplayTag Tag, int32 Count)
{
	StatTags.SetStack(Tag, Count);
}

void UZItemInstance::CopyStatsFrom(const UZItemInstance* Other)
{
	if (Other)
	{
		StatTags.CopyFrom(Other->StatTags);
	}
}

FString UZItemInstance::GetDebugString() const
{
	FString Result = GetNameSafe(ItemDefinition);

	TArray<FString> Stats;
	for (const TPair<FGameplayTag, int32>& Stat : StatTags.GetAllStacks())
	{
		Stats.Add(FString::Printf(TEXT("%s %d"), *Stat.Key.ToString(), Stat.Value));
	}

	if (Stats.Num() > 0)
	{
		Result += FString::Printf(TEXT(" (%s)"), *FString::Join(Stats, TEXT(", ")));
	}

	return Result;
}
