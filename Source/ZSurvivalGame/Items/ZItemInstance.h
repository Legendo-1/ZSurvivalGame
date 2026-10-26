// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZTagStack.h"
#include "ZItemInstance.generated.h"

/**
 *  One item in an inventory (or a stack of a stackable item), made by the server from a UZItemDefinition
 *  Holds what's special about this one item as stats, e.g. a weapon's magazine ammo, later durability or attached mods
 *  Replicates with the inventory that holds it, to the inventory's owner
 */
UCLASS(BlueprintType)
class UZItemInstance : public UObject
{
	GENERATED_BODY()

public:

	virtual bool IsSupportedForNetworking() const override { return true; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: sets what item this is and lets its fragments set the starting stats */
	void InitializeInstance(UZItemDefinition* InItemDefinition);

	UFUNCTION(BlueprintPure, Category = "Item")
	UZItemDefinition* GetItemDefinition() const { return ItemDefinition; }

	/** The item's fragment of this class, or null */
	UFUNCTION(BlueprintPure, Category = "Item", meta = (DeterminesOutputType = "FragmentClass"))
	const UZItemFragment* FindFragmentByClass(TSubclassOf<UZItemFragment> FragmentClass) const;

	template <typename FragmentType>
	const FragmentType* FindFragment() const
	{
		return ItemDefinition ? ItemDefinition->FindFragment<FragmentType>() : nullptr;
	}

	/** A stat of this item, e.g. Item.Stat.MagazineAmmo (0 if it doesn't have it) */
	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item")
	void AddStatTagStack(FGameplayTag Tag, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item")
	void RemoveStatTagStack(FGameplayTag Tag, int32 Count);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item")
	void SetStatTagStack(FGameplayTag Tag, int32 Count);

	/** Takes over another instance's stats, e.g. when a dropped weapon is picked up */
	void CopyStatsFrom(const UZItemInstance* Other);

	/** True if the item has a Durability fragment */
	UFUNCTION(BlueprintPure, Category = "Item|Durability")
	bool HasDurability() const;

	/** Durability left (Item.Stat.Durability); 0 for items without durability */
	UFUNCTION(BlueprintPure, Category = "Item|Durability")
	int32 GetDurability() const;

	/** Durability of a new item; 0 for items without durability */
	UFUNCTION(BlueprintPure, Category = "Item|Durability")
	int32 GetMaxDurability() const;

	/** Durability as 0..1, for UI bars (1 for items without durability) */
	UFUNCTION(BlueprintPure, Category = "Item|Durability")
	float GetDurabilityPercent() const;

	/** True if it has durability and it's used up (a broken tool can't be used for crafting) */
	UFUNCTION(BlueprintPure, Category = "Item|Durability")
	bool IsBroken() const;

	/** "ID_Rifle (Item.Stat.MagazineAmmo 30)", for logs */
	FString GetDebugString() const;

protected:

	/** Owner's machine: tells the inventory holding this item that it changed, so inventory screens refresh */
	void NotifyInventory() const;

	UFUNCTION()
	void OnRep_ItemDefinition();

	UFUNCTION()
	void OnRep_StatTags();

private:

	UPROPERTY(ReplicatedUsing = OnRep_ItemDefinition)
	TObjectPtr<UZItemDefinition> ItemDefinition;

	UPROPERTY(ReplicatedUsing = OnRep_StatTags)
	FZTagStackContainer StatTags;
};
