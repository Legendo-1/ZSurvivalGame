// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZItemDetailsWidget.generated.h"

class UImage;
class UTextBlock;
class UZItemInstance;

/**
 *  Describes one item: name, type, description, and what its fragments say about it (a weapon's stats and mods, durability, healing, ...)
 *  Recipes stay secret: it never says what an item can be combined with
 *  Your own look: a Widget Blueprint with this parent class; widgets with these names are filled in (all optional): NameText, TypeText, IconImage, DescriptionText, StatsText
 */
UCLASS()
class UZItemDetailsWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Shows this item (null: shows Empty Text) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetItem(UZItemInstance* InItem, int32 InCount);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZItemInstance* GetItem() const { return Item; }

	/** Updates the texts, e.g. after the item's durability changed */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void Refresh();

	/** One line per thing the item's fragments say, e.g. "Damage 18, 9 shots per second" and "Durability 18 / 20" */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FText GetStatsText() const;

protected:

	virtual void NativeOnInitialized() override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	/** A Blueprint can add to the look here */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnItemChanged(UZItemInstance* NewItem, int32 NewCount);

	/** The item's name, and how many (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** "Tool", "Weapon Mod", ... (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TypeText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** See GetStatsText (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatsText;

	/** Shown in NameText when there's no item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FText EmptyText;

private:

	UPROPERTY(Transient)
	TObjectPtr<UZItemInstance> Item;

	int32 Count = 0;
};
