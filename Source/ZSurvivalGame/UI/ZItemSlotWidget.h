// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/ZEquipmentComponent.h"
#include "UI/ZWidgetLayout.h"
#include "ZItemSlotWidget.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UZInventoryScreenWidget;
class UZItemDragDropOperation;
class UZItemInstance;

/** Where a slot is in the inventory screen */
UENUM(BlueprintType)
enum class EZItemSlotRole : uint8
{
	/** One of the player's stacks */
	Inventory,
	/** An item put in the combine slots */
	Combine,
	/** An equipment slot: the item in it, or empty */
	Equipment
};

/**
 *  One item stack in the inventory screen, or one equipment slot in the equipment window
 *  Mouse: click selects, right-click or double-click uses (equip, read, ...; on an equipped item: takes it off), drag onto another item combines them, drag onto the combine slots adds it there,
 *  drag onto an equipment slot equips it there, drag outside the window drops it on the ground. In the combine slots a click takes it out again
 *  Gamepad/keyboard: A/Enter selects (takes it out of the combine slots), X uses (takes off what's in an equipment slot), Y puts it in the combine slots
 *  Your own look: a Widget Blueprint with this parent class; widgets with these names are filled in (all optional): Background, IconImage, NameText, CountText, DurabilityBar, EquippedMarker
 */
UCLASS()
class UZItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Shows this stack (the screen calls it) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetItem(UZItemInstance* InItem, int32 InCount);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZItemInstance* GetItem() const { return Item; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetCount() const { return Count; }

	/** The screen this slot belongs to, and whether it's an inventory, combine or equipment slot (set by the screen) */
	void SetScreen(UZInventoryScreenWidget* InScreen, EZItemSlotRole InRole);

	/** Which equipment slot it shows, in the equipment window (the built-in window sets it; in your own, set it in the slot's Details) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetEquipmentSlot(const FZEquipmentSlot& InEquipmentSlot);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const FZEquipmentSlot& GetEquipmentSlot() const { return EquipmentSlot; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	EZItemSlotRole GetRole() const { return Role; }

	/** Highlights it as the selected item */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSelected() const { return bSelected; }

	/** Updates the widgets from the item, e.g. after its durability changed */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void Refresh();

protected:

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	/** Colors the Background for the current state */
	void UpdateBackground();

	/** A Blueprint can add to the look here, e.g. a rarity color */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnItemChanged(UZItemInstance* NewItem, int32 NewCount);

	/** A Blueprint can change the look for the state here (hovered, selected, something dragged over it) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnStateChanged(bool bIsSelected, bool bIsHovered, bool bIsDropTarget);

	/** Tinted for the state (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Background;

	/** The item's icon; hidden if it has none (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	/** The item's name (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** How many, hidden for a single item (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** Durability left, hidden for items without durability (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DurabilityBar;

	/** Shown while the item is equipped; a Text says "Equipped", or "In hands" for the weapon in the hands (in the equipment window: only "In hands") (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> EquippedMarker;

	/** In the equipment window: which slot this is (the kind, and which one of that kind from 0). Set it on each slot you put in your own equipment window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FZEquipmentSlot EquipmentSlot;

	/** Shown in NameText while the equipment slot is empty; the slot's name (e.g. "Ring 2") if left empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FText EmptyText;

	/** NameText's color while the equipment slot is empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor EmptyTextColor = ZWidgetLayout::MutedTextColor;

	/** Background colors for each state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor NormalColor = ZWidgetLayout::SlotColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor HoveredColor = ZWidgetLayout::SlotHoveredColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor SelectedColor = ZWidgetLayout::SlotSelectedColor;

	/** While another item is dragged over it (dropping combines them) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FLinearColor DropTargetColor = ZWidgetLayout::SlotDropTargetColor;

	/** Size of the built-in look, in slate units */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	float DefaultSize = 92.f;

private:

	/** Sends the state to UpdateBackground and the Blueprint event */
	void SetVisualState(bool bInHovered, bool bInDropTarget);

	/** True if dropping this here does something worth highlighting: combines with this item, or equips it in this equipment slot */
	bool IsDropTargetFor(const UZItemDragDropOperation* Operation) const;

	/** NameText's own color, put back when an empty equipment slot gets an item */
	FSlateColor NameTextColor;

	UPROPERTY(Transient)
	TObjectPtr<UZItemInstance> Item;

	int32 Count = 0;

	TWeakObjectPtr<UZInventoryScreenWidget> Screen;

	EZItemSlotRole Role = EZItemSlotRole::Inventory;

	bool bSelected = false;
	bool bHovered = false;
	bool bDropTarget = false;
};
