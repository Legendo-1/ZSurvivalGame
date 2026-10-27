// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemRequirement.h"
#include "ZInventoryComponent.generated.h"

class AZPickup;
class UZInventoryComponent;
class UZItemInstance;

/** One stack in an inventory: an item instance and how many */
USTRUCT(BlueprintType)
struct FZInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

private:

	friend struct FZInventoryList;
	friend class UZInventoryComponent;

	UPROPERTY()
	TObjectPtr<UZItemInstance> Instance;

	UPROPERTY()
	int32 StackCount = 0;
};

/** The replicated list of stacks (a fast array: only the changed stacks are sent) */
USTRUCT(BlueprintType)
struct FZInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FZInventoryList() {}

	explicit FZInventoryList(UZInventoryComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	/** Owner's machine: tells the inventory component something changed */
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FZInventoryEntry, FZInventoryList>(Entries, DeltaParms, *this);
	}

private:

	friend class UZInventoryComponent;

	UPROPERTY()
	TArray<FZInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UZInventoryComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FZInventoryList> : public TStructOpsTypeTraitsBase2<FZInventoryList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/** A stack in an inventory, for Blueprints and UI */
USTRUCT(BlueprintType)
struct FZInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UZItemInstance> Instance;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnInventoryChanged, UZInventoryComponent*, Inventory);
DECLARE_MULTICAST_DELEGATE_TwoParams(FZOnInventoryItemRemoving, UZInventoryComponent* /*Inventory*/, UZItemInstance* /*Instance*/);

/**
 *  Holds items as stacks. Only the server changes it; it replicates to the owner (players: on their PlayerState)
 *  Can also go on other actors later, e.g. storage chests (a child class can change who it replicates to with DOREPLIFETIME_CHANGE_CONDITION)
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The inventory of an actor: its own, or for players the one on their PlayerState */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static UZInventoryComponent* FindInventoryComponent(const AActor* Actor);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sends the item instances to the owner (for actors that replicate subobjects the classic way) */
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	/** Registers the item instances (for actors that use the registered subobject list) */
	virtual void ReadyForReplication() override;

	/** --- READING (the server, and the owner's copy on their machine) --- */

	/** How many of this item, over all stacks */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UZItemDefinition* Item) const;

	/** All stacks */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FZInventoryItem> GetItems() const;

	/** How many are in this instance's stack (0 if it isn't in here) */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetStackCount(const UZItemInstance* Instance) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZItemInstance* FindFirstInstance(const UZItemDefinition* Item) const;

	/** The first item instance that matches, e.g. the first weapon of a type */
	UZItemInstance* FindFirstInstanceMatching(TFunctionRef<bool(const UZItemInstance*)> Predicate) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetNumStacks() const { return Inventory.Entries.Num(); }

	/** True when every stack's item is here: on the owner's machine an item can arrive a moment after the list, e.g. right after joining */
	bool HaveAllItemsArrived() const;

	/** Most stacks it can hold; 0 = no limit */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxSlots() const { return MaxSlots; }

	/** How many items count toward the requirement (e.g. all cloth, or every item tagged Item.Tool.Screwdriver), over all stacks */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMatchingCount(const FZItemRequirement& Requirement) const;

	/** How many more of this item fit (room in its stacks plus free slots); MAX_int32 if there's no slot limit */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetRoomFor(const UZItemDefinition* Item) const;

	/** --- CHANGING (server only) --- */

	/** Adds items, filling existing stacks first. Returns how many fit */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 AddItem(UZItemDefinition* Item, int32 Count = 1);

	/** Adds one item that keeps the stats of another instance (e.g. a dropped weapon and its ammo). Returns the new instance, or null if it didn't fit */
	UZItemInstance* AddItemCopy(const UZItemInstance* Source);

	/** Adds a stack of its own (not topped up into others), e.g. to put back a saved inventory as it was. Returns its instance, or null if there's no free slot */
	UZItemInstance* AddStack(UZItemDefinition* Item, int32 Count);

	/** Takes items, from the last stacks first. Returns how many were taken */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItem(const UZItemDefinition* Item, int32 Count = 1);

	/** Takes from this instance's stack. Returns how many were taken */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItemInstance(UZItemInstance* Instance, int32 Count = 1);

	/** Wears down an item with durability, e.g. a tool used for crafting. Returns true if it broke (and was removed, if its Durability fragment says so) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool ReduceDurability(UZItemInstance* Instance, int32 Amount);

	/** Puts items on the ground in front of the owner's character as a pickup (picked up with the interact key), e.g. crafted items that didn't fit */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool DropOnGround(UZItemDefinition* Item, int32 Count);

	/** --- REQUESTS FROM THE OWNING PLAYER (checked on the server) --- */

	/** Uses the item (e.g. a bandage heals), if it can be used */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseItem(UZItemInstance* Instance);

	/** Drops items on the ground in front of the character, as a pickup */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropItem(UZItemInstance* Instance, int32 Count = 1);

	/** Something in the inventory changed (on the server and on the owner's machine), e.g. to refresh an inventory screen */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FZOnInventoryChanged OnInventoryChanged;

	/** Server: an item instance is about to leave the inventory (e.g. so the character puts the weapon away first) */
	FZOnInventoryItemRemoving OnItemRemoving;

	/** Lets listeners know the contents changed */
	void BroadcastChanged();

	/** "ID_Rifle x1, ID_Ammo_Rifle x90", for logs */
	FString GetDebugString() const;

protected:

	UFUNCTION(Server, Reliable)
	void ServerUseItem(UZItemInstance* Instance);

	UFUNCTION(Server, Reliable)
	void ServerDropItem(UZItemInstance* Instance, int32 Count);

	/** Server: makes a new item instance owned by this inventory and adds it as a stack */
	UZItemInstance* AddNewStack(UZItemDefinition* Item, int32 Count, const UZItemInstance* StatsSource = nullptr);

	/** Server: takes Count from the stack at Index, removing the stack when it's empty */
	void RemoveFromEntry(int32 Index, int32 Count);

	/** True if one more stack fits */
	bool HasFreeSlot() const;

	int32 FindEntryIndex(const UZItemInstance* Instance) const;

	/** The character using this inventory (for players, their PlayerState's pawn) */
	APawn* GetOwningPawn() const;

	/** Where dropped items land: in front of the owner's character */
	FVector GetDropLocation() const;

	/** Most stacks it can hold; 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 MaxSlots = 0;

	/** Spawned when items are dropped (AZPickup if empty) */
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<AZPickup> DropPickupClass;

	/** How far in front of the character dropped items land, in cm */
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = "0"))
	float DropDistance = 120.f;

private:

	UPROPERTY(Replicated)
	FZInventoryList Inventory;
};
