// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemInstance.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemFragments.h"

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

bool UZItemInstance::HasDurability() const
{
	return FindFragment<UZItemFragment_Durability>() != nullptr;
}

int32 UZItemInstance::GetDurability() const
{
	return HasDurability() ? StatTags.GetStackCount(ZGameplayTags::Item_Stat_Durability) : 0;
}

int32 UZItemInstance::GetMaxDurability() const
{
	const UZItemFragment_Durability* Durability = FindFragment<UZItemFragment_Durability>();
	return Durability ? Durability->MaxDurability : 0;
}

float UZItemInstance::GetDurabilityPercent() const
{
	const int32 MaxDurability = GetMaxDurability();
	return MaxDurability > 0 ? FMath::Clamp(static_cast<float>(GetDurability()) / MaxDurability, 0.f, 1.f) : 1.f;
}

bool UZItemInstance::IsBroken() const
{
	return HasDurability() && GetDurability() <= 0;
}

void UZItemInstance::NotifyInventory() const
{
	// instances are owned by the actor whose inventory holds them (for players: their PlayerState)
	if (UZInventoryComponent* Inventory = UZInventoryComponent::FindInventoryComponent(GetTypedOuter<AActor>()))
	{
		Inventory->BroadcastChanged();
	}
}

void UZItemInstance::OnRep_ItemDefinition()
{
	NotifyInventory();
}

void UZItemInstance::OnRep_StatTags()
{
	NotifyInventory();
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
