// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ZItemRequirement.generated.h"

class UZItemDefinition;

/**
 *  "This many of this item", or "this many of any item with these tags" (e.g. any item tagged Item.Tool.Screwdriver),
 *  optionally only tools of at least some quality
 *  Used by crafting recipes; later also by doors that need a key, upgrades, traders, ...
 */
USTRUCT(BlueprintType)
struct FZItemRequirement
{
	GENERATED_BODY()

	/** A specific item. Leave empty to accept any item that has all the Item Tags */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	TObjectPtr<UZItemDefinition> Item;

	/** Used when Item is empty: any item with all of these tags counts, e.g. Item.Tool.Screwdriver for any screwdriver */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (Categories = "Item", EditCondition = "Item == nullptr"))
	FGameplayTagContainer ItemTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Only tools of at least this quality count (their Tool fragment's Quality); 0 = any item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement", meta = (ClampMin = "0"))
	int32 MinToolQuality = 0;

	/** Name in messages and logs for tag requirements, e.g. "Any Screwdriver" (empty: the item's name, or one made from the tag) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirement")
	FText DisplayName;

	/** True if it names an item or tags */
	bool IsValid() const;

	/** True if this item counts toward the requirement (bCheckToolQuality false: the right kind of item, whatever its quality) */
	bool Matches(const UZItemDefinition* Candidate, bool bCheckToolQuality = true) const;

	/** Name for messages and logs, e.g. "Cloth" or "Any Screwdriver" */
	FText GetDisplayName() const;
};
