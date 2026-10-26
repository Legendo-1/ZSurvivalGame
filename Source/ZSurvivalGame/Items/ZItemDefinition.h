// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZItemDefinition.generated.h"

class APawn;
class UStaticMesh;
class UTexture2D;
class UZItemInstance;

/**
 *  A piece an item is built from (weapon, consumable, ...). An item gets a feature by adding the fragment for it,
 *  so a new kind of item feature is a new fragment class, not a change to every item
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class UZItemFragment : public UObject
{
	GENERATED_BODY()

public:

	/** Server: a new instance of the item was made, e.g. to set its starting stats */
	virtual void OnInstanceCreated(UZItemInstance* Instance) const {}

	/** Server: the item is used from the inventory. Returns true if it did something */
	virtual bool OnUse(UZItemInstance* Instance, APawn* User) const { return false; }

	/** True if using the item takes one from the stack */
	virtual bool ShouldConsumeOnUse() const { return false; }

	/** What using the item is called in the UI, e.g. "Use" or "Equip"; empty if this fragment doesn't make it usable */
	virtual FText GetUseActionName() const { return FText::GetEmpty(); }

	/** Lines for inventory screens about what this fragment does, e.g. "Heals 25" or "Durability 18 / 20" (Instance may be null: the item in general) */
	virtual void GetDescriptionLines(const UZItemInstance* Instance, TArray<FText>& OutLines) const {}

#if WITH_EDITOR
	/** Checks the fragment fits its item (called when the item is saved or validated) */
	virtual void ValidateForItem(const class UZItemDefinition* Item, class FDataValidationContext& Context) const {}
#endif
};

/**
 *  An item: its name, looks, stacking and what it can do (fragments). Make one Data Asset per item in Content/Items:
 *  right-click → Miscellaneous → Data Asset → ZItemDefinition
 */
UCLASS(BlueprintType)
class UZItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Primary asset type of every item (the Asset Manager finds them by it, e.g. for save games or the ZGiveItem command) */
	static FPrimaryAssetType GetItemAssetType() { return FPrimaryAssetType(TEXT("Item")); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Name shown in the UI */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	/** Icon for inventory screens (loaded when needed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Tags describing the item, e.g. Item.Type.Ammo; for sorting, filtering and rules (what a slot accepts) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (Categories = "Item"))
	FGameplayTagContainer ItemTags;

	/** How many fit in one inventory stack. 1 = not stackable, for items with their own stats (e.g. a weapon's ammo) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	/** Picked up by walking over it when it lies in the world (e.g. ammo and crafting materials); otherwise with the interact key */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bAutoPickup = false;

	/** Mesh shown when the item lies in the world (a small cube if empty) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMesh> PickupMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FVector PickupMeshScale = FVector(1.f);

	/** What the item can do: add a fragment for each feature (Weapon, Consumable, ...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = "Item")
	TArray<TObjectPtr<UZItemFragment>> Fragments;

	/** The item's fragment of this class, or null */
	UFUNCTION(BlueprintPure, Category = "Item", meta = (DeterminesOutputType = "FragmentClass"))
	const UZItemFragment* FindFragmentByClass(TSubclassOf<UZItemFragment> FragmentClass) const;

	template <typename FragmentType>
	const FragmentType* FindFragment() const
	{
		return Cast<FragmentType>(FindFragmentByClass(FragmentType::StaticClass()));
	}

	bool IsStackable() const { return MaxStackSize > 1; }

	/** Name for the UI: Display Name, or the asset name if that's empty */
	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetItemName() const;

	/** What using it is called, e.g. "Use" or "Equip"; empty if it can't be used */
	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetUseActionName() const;

	/** True if it can be used from the inventory (a fragment does something with it) */
	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsUsable() const { return !GetUseActionName().IsEmpty(); }

	/** Its Item.Type.* tag (e.g. Item.Type.Ammo), for sorting and filters; empty if it has none */
	UFUNCTION(BlueprintPure, Category = "Item")
	FGameplayTag GetItemTypeTag() const;

	/** "Weapon Mod" for Item.Type.WeaponMod; empty if it has no type */
	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetTypeName() const;

	/** What its fragments say about it for inventory screens, e.g. "Heals 25", "Durability 18 / 20" (for this instance's stats if one is given) */
	UFUNCTION(BlueprintPure, Category = "Item")
	TArray<FText> GetDescriptionLines(const UZItemInstance* Instance) const;

	/** Its quality as a tool (from its Tool fragment); 0 if it isn't a tool */
	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetToolQuality() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/** An item and how many, e.g. for starting items, loot and pickups */
USTRUCT(BlueprintType)
struct FZItemStack
{
	GENERATED_BODY()

	FZItemStack() {}

	FZItemStack(UZItemDefinition* InItem, int32 InCount)
		: Item(InItem)
		, Count(InCount)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TObjectPtr<UZItemDefinition> Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "1"))
	int32 Count = 1;
};
