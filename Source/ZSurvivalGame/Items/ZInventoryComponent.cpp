// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZInventoryComponent.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZItemFragments.h"
#include "Items/ZItemInstance.h"
#include "Items/ZPickup.h"

void FZInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (OwnerComponent)
	{
		OwnerComponent->BroadcastChanged();
	}
}

UZInventoryComponent::UZInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Inventory(this)
{
	SetIsReplicatedByDefault(true);
}

UZInventoryComponent* UZInventoryComponent::FindInventoryComponent(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UZInventoryComponent* OwnInventory = Actor->FindComponentByClass<UZInventoryComponent>())
	{
		return OwnInventory;
	}

	// players keep their inventory on the PlayerState, so it survives respawns
	const APlayerState* PlayerState = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		PlayerState = Pawn->GetPlayerState();
	}
	else if (const AController* Controller = Cast<AController>(Actor))
	{
		PlayerState = Controller->GetPlayerState<APlayerState>();
	}

	return PlayerState ? PlayerState->FindComponentByClass<UZInventoryComponent>() : nullptr;
}

void UZInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only the owner sees what's in the inventory
	DOREPLIFETIME_CONDITION(UZInventoryComponent, Inventory, COND_OwnerOnly);
}

bool UZInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	// the item instances only go to the owner, like the list itself
	if (RepFlags->bNetOwner)
	{
		for (FZInventoryEntry& Entry : Inventory.Entries)
		{
			if (IsValid(Entry.Instance))
			{
				bWroteSomething |= Channel->ReplicateSubobject(Entry.Instance, *Bunch, *RepFlags);
			}
		}
	}

	return bWroteSomething;
}

void UZInventoryComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	if (IsUsingRegisteredSubObjectList())
	{
		for (const FZInventoryEntry& Entry : Inventory.Entries)
		{
			if (IsValid(Entry.Instance))
			{
				AddReplicatedSubObject(Entry.Instance, COND_OwnerOnly);
			}
		}
	}
}

int32 UZInventoryComponent::GetItemCount(const UZItemDefinition* Item) const
{
	int32 Total = 0;
	for (const FZInventoryEntry& Entry : Inventory.Entries)
	{
		if (Item && Entry.Instance && Entry.Instance->GetItemDefinition() == Item)
		{
			Total += Entry.StackCount;
		}
	}

	return Total;
}

TArray<FZInventoryItem> UZInventoryComponent::GetItems() const
{
	TArray<FZInventoryItem> Items;
	for (const FZInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.Instance)
		{
			FZInventoryItem& Item = Items.AddDefaulted_GetRef();
			Item.Instance = Entry.Instance;
			Item.Count = Entry.StackCount;
		}
	}

	return Items;
}

int32 UZInventoryComponent::GetStackCount(const UZItemInstance* Instance) const
{
	const int32 Index = FindEntryIndex(Instance);
	return Index != INDEX_NONE ? Inventory.Entries[Index].StackCount : 0;
}

int32 UZInventoryComponent::GetMatchingCount(const FZItemRequirement& Requirement) const
{
	int32 Total = 0;
	for (const FZInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.Instance && Requirement.Matches(Entry.Instance->GetItemDefinition()))
		{
			Total += Entry.StackCount;
		}
	}

	return Total;
}

int32 UZInventoryComponent::GetRoomFor(const UZItemDefinition* Item) const
{
	if (!Item)
	{
		return 0;
	}

	if (MaxSlots <= 0)
	{
		return MAX_int32;
	}

	// room left in its stacks, plus a full stack per free slot
	int64 Room = 0;
	for (const FZInventoryEntry& Entry : Inventory.Entries)
	{
		if (Item->IsStackable() && Entry.Instance && Entry.Instance->GetItemDefinition() == Item)
		{
			Room += FMath::Max(Item->MaxStackSize - Entry.StackCount, 0);
		}
	}

	Room += static_cast<int64>(FMath::Max(MaxSlots - Inventory.Entries.Num(), 0)) * Item->MaxStackSize;
	return static_cast<int32>(FMath::Min<int64>(Room, MAX_int32));
}

UZItemInstance* UZInventoryComponent::FindFirstInstance(const UZItemDefinition* Item) const
{
	return FindFirstInstanceMatching([Item](const UZItemInstance* Instance) { return Instance->GetItemDefinition() == Item; });
}

UZItemInstance* UZInventoryComponent::FindFirstInstanceMatching(TFunctionRef<bool(const UZItemInstance*)> Predicate) const
{
	for (const FZInventoryEntry& Entry : Inventory.Entries)
	{
		if (Entry.Instance && Predicate(Entry.Instance))
		{
			return Entry.Instance;
		}
	}

	return nullptr;
}

int32 UZInventoryComponent::AddItem(UZItemDefinition* Item, int32 Count)
{
	if (!GetOwner()->HasAuthority() || !Item || Count <= 0)
	{
		return 0;
	}

	int32 Remaining = Count;

	// top up existing stacks first
	if (Item->IsStackable())
	{
		for (FZInventoryEntry& Entry : Inventory.Entries)
		{
			if (Remaining <= 0)
			{
				break;
			}

			if (Entry.Instance && Entry.Instance->GetItemDefinition() == Item && Entry.StackCount < Item->MaxStackSize)
			{
				const int32 AddedToStack = FMath::Min(Remaining, Item->MaxStackSize - Entry.StackCount);
				Entry.StackCount += AddedToStack;
				Remaining -= AddedToStack;
				Inventory.MarkItemDirty(Entry);
			}
		}
	}

	// then new stacks, while there's room
	while (Remaining > 0 && HasFreeSlot())
	{
		const int32 StackSize = FMath::Min(Remaining, Item->MaxStackSize);
		AddNewStack(Item, StackSize);
		Remaining -= StackSize;
	}

	const int32 Added = Count - Remaining;
	if (Added > 0)
	{
		BroadcastChanged();
	}

	return Added;
}

UZItemInstance* UZInventoryComponent::AddItemCopy(const UZItemInstance* Source)
{
	if (!GetOwner()->HasAuthority() || !Source || !Source->GetItemDefinition() || !HasFreeSlot())
	{
		return nullptr;
	}

	UZItemInstance* NewInstance = AddNewStack(Source->GetItemDefinition(), 1, Source);
	BroadcastChanged();
	return NewInstance;
}

int32 UZInventoryComponent::RemoveItem(const UZItemDefinition* Item, int32 Count)
{
	if (!GetOwner()->HasAuthority() || !Item || Count <= 0)
	{
		return 0;
	}

	int32 Remaining = Count;
	for (int32 Index = Inventory.Entries.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		const FZInventoryEntry& Entry = Inventory.Entries[Index];
		if (Entry.Instance && Entry.Instance->GetItemDefinition() == Item)
		{
			const int32 Taken = FMath::Min(Remaining, Entry.StackCount);
			RemoveFromEntry(Index, Taken);
			Remaining -= Taken;
		}
	}

	const int32 Removed = Count - Remaining;
	if (Removed > 0)
	{
		BroadcastChanged();
	}

	return Removed;
}

int32 UZInventoryComponent::RemoveItemInstance(UZItemInstance* Instance, int32 Count)
{
	const int32 Index = FindEntryIndex(Instance);
	if (!GetOwner()->HasAuthority() || Index == INDEX_NONE || Count <= 0)
	{
		return 0;
	}

	const int32 Taken = FMath::Min(Count, Inventory.Entries[Index].StackCount);
	RemoveFromEntry(Index, Taken);
	BroadcastChanged();
	return Taken;
}

bool UZInventoryComponent::ReduceDurability(UZItemInstance* Instance, int32 Amount)
{
	const UZItemFragment_Durability* Durability = Instance ? Instance->FindFragment<UZItemFragment_Durability>() : nullptr;
	if (!GetOwner()->HasAuthority() || !Durability || Amount <= 0 || FindEntryIndex(Instance) == INDEX_NONE)
	{
		return false;
	}

	// the stat replicates with the instance; its OnRep refreshes the owner's inventory screen
	Instance->RemoveStatTagStack(ZGameplayTags::Item_Stat_Durability, Amount);
	const bool bBroke = Instance->GetDurability() <= 0;

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' lost %d durability (%d left)%s"), *GetNameSafe(Instance->GetItemDefinition()), Amount, Instance->GetDurability(), bBroke ? TEXT(" and broke") : TEXT(""));

	if (bBroke && Durability->bBreaksWhenEmpty)
	{
		RemoveItemInstance(Instance, GetStackCount(Instance));
	}
	else
	{
		BroadcastChanged();
	}

	return bBroke;
}

bool UZInventoryComponent::DropOnGround(UZItemDefinition* Item, int32 Count)
{
	if (!GetOwner()->HasAuthority() || !Item || Count <= 0)
	{
		return false;
	}

	// dropped by a player: picked up again with the interact key, not by standing on it
	return AZPickup::SpawnPickup(GetWorld(), DropPickupClass, FZItemStack(Item, Count), GetDropLocation(), nullptr, false) != nullptr;
}

void UZInventoryComponent::UseItem(UZItemInstance* Instance)
{
	// on the server this runs right away
	ServerUseItem(Instance);
}

void UZInventoryComponent::DropItem(UZItemInstance* Instance, int32 Count)
{
	ServerDropItem(Instance, Count);
}

void UZInventoryComponent::ServerUseItem_Implementation(UZItemInstance* Instance)
{
	APawn* User = GetOwningPawn();
	const UZItemDefinition* Item = Instance ? Instance->GetItemDefinition() : nullptr;
	if (FindEntryIndex(Instance) == INDEX_NONE || !Item || !User || UZCombatLibrary::IsDead(User))
	{
		return;
	}

	// every fragment gets a go (e.g. Consumable heals); it's used up if one of them says so
	bool bUsed = false;
	bool bConsume = false;
	for (const UZItemFragment* Fragment : Item->Fragments)
	{
		if (Fragment && Fragment->OnUse(Instance, User))
		{
			bUsed = true;
			bConsume |= Fragment->ShouldConsumeOnUse();
		}
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' used '%s': %s"), *GetNameSafe(User), *GetNameSafe(Item), bUsed ? TEXT("yes") : TEXT("nothing happened"));

	if (bConsume)
	{
		RemoveItemInstance(Instance, 1);
	}
}

void UZInventoryComponent::ServerDropItem_Implementation(UZItemInstance* Instance, int32 Count)
{
	const int32 Index = FindEntryIndex(Instance);
	UZItemDefinition* Item = Instance ? Instance->GetItemDefinition() : nullptr;
	if (Index == INDEX_NONE || !Item || Count <= 0)
	{
		return;
	}

	const int32 DropCount = FMath::Min(Count, Inventory.Entries[Index].StackCount);

	// a non-stackable item keeps its stats on the ground (e.g. a weapon's ammo): let listeners put it away first, so the stats are up to date
	const bool bKeepStats = !Item->IsStackable();
	if (bKeepStats)
	{
		OnItemRemoving.Broadcast(this, Instance);
	}

	// dropped by a player: picked up again with the interact key, not by standing on it
	if (AZPickup::SpawnPickup(GetWorld(), DropPickupClass, FZItemStack(Item, DropCount), GetDropLocation(), bKeepStats ? Instance : nullptr, false))
	{
		RemoveItemInstance(Instance, DropCount);
	}
}

UZItemInstance* UZInventoryComponent::AddNewStack(UZItemDefinition* Item, int32 Count, const UZItemInstance* StatsSource)
{
	// owned by the inventory's actor, like the inventory itself
	UZItemInstance* NewInstance = NewObject<UZItemInstance>(GetOwner());
	NewInstance->InitializeInstance(Item);
	if (StatsSource)
	{
		NewInstance->CopyStatsFrom(StatsSource);
	}

	FZInventoryEntry& NewEntry = Inventory.Entries.AddDefaulted_GetRef();
	NewEntry.Instance = NewInstance;
	NewEntry.StackCount = Count;
	Inventory.MarkItemDirty(NewEntry);

	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(NewInstance, COND_OwnerOnly);
	}

	return NewInstance;
}

void UZInventoryComponent::RemoveFromEntry(int32 Index, int32 Count)
{
	if (Count < Inventory.Entries[Index].StackCount)
	{
		FZInventoryEntry& Entry = Inventory.Entries[Index];
		Entry.StackCount -= Count;
		Inventory.MarkItemDirty(Entry);
		return;
	}

	// the whole stack leaves: let listeners react first (e.g. put away an equipped weapon)
	UZItemInstance* Instance = Inventory.Entries[Index].Instance;
	OnItemRemoving.Broadcast(this, Instance);

	// a listener may have changed the list
	const int32 CurrentIndex = FindEntryIndex(Instance);
	if (CurrentIndex == INDEX_NONE)
	{
		return;
	}

	if (Instance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(Instance);
	}

	Inventory.Entries.RemoveAt(CurrentIndex);
	Inventory.MarkArrayDirty();
}

bool UZInventoryComponent::HasFreeSlot() const
{
	return MaxSlots <= 0 || Inventory.Entries.Num() < MaxSlots;
}

int32 UZInventoryComponent::FindEntryIndex(const UZItemInstance* Instance) const
{
	if (!Instance)
	{
		return INDEX_NONE;
	}

	return Inventory.Entries.IndexOfByPredicate([Instance](const FZInventoryEntry& Entry) { return Entry.Instance == Instance; });
}

APawn* UZInventoryComponent::GetOwningPawn() const
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn;
	}

	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetPawn() : nullptr;
}

FVector UZInventoryComponent::GetDropLocation() const
{
	const APawn* Dropper = GetOwningPawn();
	return Dropper ? Dropper->GetActorLocation() + Dropper->GetActorForwardVector() * DropDistance : GetOwner()->GetActorLocation();
}

void UZInventoryComponent::BroadcastChanged()
{
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("Inventory of '%s' changed: %s"), *GetNameSafe(GetOwner()), *GetDebugString());

	OnInventoryChanged.Broadcast(this);
}

FString UZInventoryComponent::GetDebugString() const
{
	TArray<FString> Stacks;
	for (const FZInventoryEntry& Entry : Inventory.Entries)
	{
		Stacks.Add(FString::Printf(TEXT("%s x%d"), Entry.Instance ? *Entry.Instance->GetDebugString() : TEXT("(not arrived yet)"), Entry.StackCount));
	}

	return Stacks.Num() > 0 ? FString::Join(Stacks, TEXT(", ")) : TEXT("empty");
}
