// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ZEquipmentComponent.generated.h"

class UAbilitySystemComponent;
class UZEquipmentComponent;
class UZInventoryComponent;
class UZItemInstance;

/** One equipment slot: its kind and which one of that kind, e.g. the third ring slot is Equipment.Slot.Ring, 2 */
USTRUCT(BlueprintType)
struct FZEquipmentSlot
{
	GENERATED_BODY()

	FZEquipmentSlot() {}

	FZEquipmentSlot(const FGameplayTag& InType, int32 InIndex)
		: Type(InType)
		, Index(InIndex)
	{
	}

	/** The kind of slot, e.g. Equipment.Slot.Ring */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag Type;

	/** Which one of that kind, from 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (ClampMin = "0"))
	int32 Index = 0;

	bool IsValid() const { return Type.IsValid(); }

	bool operator==(const FZEquipmentSlot& Other) const { return Type == Other.Type && Index == Other.Index; }
	bool operator!=(const FZEquipmentSlot& Other) const { return !(*this == Other); }

	/** "Ring 3" (or "Long gun" when there's only one of that kind) */
	FText GetDisplayName() const;

	/** "Equipment.Slot.Ring 2", for logs */
	FString ToString() const;
};

/** An item in an equipment slot */
USTRUCT(BlueprintType)
struct FZEquippedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FZEquipmentSlot Slot;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UZItemInstance> Item;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnEquipmentChanged, UZEquipmentComponent*, Equipment);
DECLARE_MULTICAST_DELEGATE_TwoParams(FZOnItemEquippedByPlayer, UZEquipmentComponent* /*Equipment*/, UZItemInstance* /*Item*/);

/**
 *  What a player has equipped: one short gun, one long gun and one melee weapon (taken out with 1, 2 and 3), armor and jewelry.
 *  The slots are in Project Settings → Game → Equipment. On the PlayerState next to the inventory, so it survives dying
 *  Equipped items stay in the inventory; this only says which slot each one is in. Only the server changes it, and it replicates to the owner only
 *  The server puts the stats of everything equipped on the player as one gameplay effect (UZGameplayEffect_Equipment), e.g. Armor from a jacket
 *  Which equipped weapon is in the hands is the character's (AZSurvivalGameCharacter); it takes the weapons out
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZEquipmentComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The equipment of an actor: its own, or for players the one on their PlayerState */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	static UZEquipmentComponent* FindEquipmentComponent(const AActor* Actor);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: listens to the inventory, so items that leave it come out of their slots */
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** --- READING (the server, and the owner's copy on their machine) --- */

	/** Every slot players have, in the order the equipment window shows them */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	static TArray<FZEquipmentSlot> GetAllSlots();

	/** True if players have this slot */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	static bool IsValidSlot(const FZEquipmentSlot& Slot);

	/** The item in a slot, or null */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	UZItemInstance* GetItemInSlot(const FZEquipmentSlot& Slot) const;

	/** The slot an item is in (not valid if it isn't equipped) */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	FZEquipmentSlot GetSlotOfItem(const UZItemInstance* Item) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsEquipped(const UZItemInstance* Item) const;

	/** True if the item is in this player's inventory and goes in this kind of slot */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanEquip(const UZItemInstance* Item, const FZEquipmentSlot& Slot) const;

	/** Everything equipped, and where */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	TArray<FZEquippedItem> GetEquippedItems() const { return Equipped; }

	/** The items in the slots of one kind, in slot order (e.g. the rings) */
	TArray<UZItemInstance*> GetItemsOfSlotType(const FGameplayTag& Type) const;

	/** True when every equipped item is here: on the owner's machine an item can arrive a moment after the list, e.g. right after joining */
	bool HaveAllItemsArrived() const;

	/** --- REQUESTS FROM THE OWNING PLAYER (checked on the server) --- */

	/** Puts an item from the inventory in a slot. Without a slot: a free one of its kind, or the first one. What was in the slot goes back to the inventory (or swaps places, if the item came from another slot) */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RequestEquip(UZItemInstance* Item, FZEquipmentSlot Slot);

	/** Takes an item out of its slot (it stays in the inventory) */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RequestUnequip(UZItemInstance* Item);

	/** --- CHANGING (server only) --- */

	/** Puts an item in a slot (see RequestEquip). Returns false if it doesn't go there */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool EquipItem(UZItemInstance* Item, FZEquipmentSlot Slot);

	/** EquipItem for something the player did themselves: also tells the item's fragments (e.g. a sound) and listeners (the character takes out a weapon) */
	bool EquipItemByPlayer(UZItemInstance* Item, FZEquipmentSlot Slot);

	/** Puts an item in a free slot of its kind; does nothing if they're all taken, e.g. for starting items. Returns true if it's equipped now */
	bool EquipInFreeSlot(UZItemInstance* Item);

	/** Takes an item out of its slot. Returns false if it wasn't equipped */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool UnequipItem(UZItemInstance* Item);

	/**
	 *  Server: puts back a saved character's equipment, within the game's rules: only items that go in their slot, one per slot
	 *  (an item without a slot goes in a free one of its kind, e.g. the weapon in the hands from a save made before there was equipment). OutFixes gets a line for each change
	 */
	void RestoreEquipment(const TArray<TPair<UZItemInstance*, FZEquipmentSlot>>& SavedEquipment, TArray<FString>& OutFixes);

	/** --- EVENTS --- */

	/** Something was equipped or taken off (the server, and the owner's machine) */
	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FZOnEquipmentChanged OnEquipmentChanged;

	/** Server: the player equipped an item themselves (see EquipItemByPlayer) */
	FZOnItemEquippedByPlayer OnItemEquippedByPlayer;

	/** "Long gun: ID_Rifle, Ring 1: ID_Ring_Iron", for logs */
	FString GetDebugString() const;

protected:

	UFUNCTION(Server, Reliable)
	void ServerEquip(UZItemInstance* Item, FZEquipmentSlot Slot);

	UFUNCTION(Server, Reliable)
	void ServerUnequip(UZItemInstance* Item);

	UFUNCTION()
	void OnRep_Equipped();

private:

	/** Server: an item is leaving the inventory; it leaves its slot too */
	void HandleItemRemoving(UZInventoryComponent* Inventory, UZItemInstance* Item);

	/** Server: puts the stats on again and tells everyone */
	void HandleEquipmentChanged();

	/** Server: puts the stats of everything equipped on the ability system (replacing the old ones) */
	void ApplyEquipmentEffects();

	/** Where an item goes when no slot is given: where it is now, a free slot of its kind, or the first one of its kind */
	FZEquipmentSlot FindSlotFor(const UZItemInstance* Item) const;

	UZInventoryComponent* GetInventory() const;

	UAbilitySystemComponent* GetAbilitySystem() const;

	UPROPERTY(ReplicatedUsing = OnRep_Equipped)
	TArray<FZEquippedItem> Equipped;

	/** Server: the effect holding the equipment's stats */
	FActiveGameplayEffectHandle EquipmentEffectHandle;
};
