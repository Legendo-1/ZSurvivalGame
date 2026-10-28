// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "ZItemDragDropOperation.generated.h"

class UZItemInstance;

/**
 *  An item stack being dragged in the inventory screen
 *  Dropped on another item it combines them, on the combine slots it's added there, on an equipment slot it's equipped there, outside the inventory window it's dropped on the ground
 */
UCLASS()
class UZItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:

	/** The dragged stack */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UZItemInstance> Item;

	/** How many are in the stack */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Count = 0;

	/** True if it was dragged out of the combine slots: dropped anywhere else, it's just taken out of them */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bFromCombineSlot = false;

	/** True if it was dragged out of an equipment slot: onto another slot of its kind it moves there, anywhere else it's taken off (never dropped on the ground) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bFromEquipmentSlot = false;
};
