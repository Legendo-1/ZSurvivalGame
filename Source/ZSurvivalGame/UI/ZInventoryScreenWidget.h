// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ZItemSlotWidget.h"
#include "ZInventoryScreenWidget.generated.h"

class UButton;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UZCharacterPanelWidget;
class UZCraftingComponent;
class UZCraftingRecipe;
class UZInventoryComponent;
class UZItemDetailsWidget;
class UZItemInstance;
enum class EZCraftingResult : uint8;

/**
 *  The inventory screen: the player's items, combining them to craft, and the character panel (level, traits, tradeskills)
 *  The player controller opens and closes it (the inventory key, or using a crafting station), see AZSurvivalGamePlayerController::OpenInventory
 *
 *  Items: click selects, right-click or double-click uses, dragging one item onto another tries the two together, dragging one outside the window drops it on the ground
 *  Combine slots: put items in (drag them there, the Add button, or Y on a gamepad), choose how many and press Combine. Recipes stay secret: only trying finds them
 *  Everything it does is a request to the server (UZInventoryComponent, UZCraftingComponent, UZProgressionComponent), so it works the same for every player
 *
 *  Your own look: a Widget Blueprint with this parent class. Widgets with the names below are used, all optional.
 *  A Widget Blueprint without any widgets of its own gets the complete built-in look
 */
UCLASS()
class UZInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** The owning player's inventory */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZInventoryComponent* GetInventory() const { return Inventory.Get(); }

	/** The owning player's crafting */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZCraftingComponent* GetCrafting() const { return Crafting.Get(); }

	/** --- SELECTING --- */

	/** Selects an item: it's highlighted, and the buttons work on it */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SelectItem(UZItemInstance* Item);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZItemInstance* GetSelectedItem() const { return SelectedItem; }

	/** The mouse or the gamepad focus is on an item: the details show it */
	void SetHoveredItem(UZItemInstance* Item);

	/** The mouse or the focus left this item: the details go back to the selected one */
	void ClearHoveredItem(UZItemInstance* Item);

	/** --- DOING THINGS WITH ITEMS (requests to the server) --- */

	/** Uses the item: equips a weapon, uses a bandage, reads a tome */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseItem(UZItemInstance* Item);

	/** Drops items from this stack on the ground in front of the character; Count 0 drops the whole stack */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropItem(UZItemInstance* Item, int32 Count = 0);

	/** Tries exactly these two items together, like dragging one onto the other */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CombineItems(UZItemInstance* First, UZItemInstance* Second);

	/** --- COMBINE SLOTS --- */

	/** Puts an item in the combine slots. Returns false if it's already there or the slots are full */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Combine")
	bool AddToCombine(UZItemInstance* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Combine")
	void RemoveFromCombine(UZItemInstance* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory|Combine")
	void ClearCombine();

	UFUNCTION(BlueprintPure, Category = "Inventory|Combine")
	TArray<UZItemInstance*> GetCombineItems() const;

	/** Tries the items in the combine slots together, Craft Count times */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Combine")
	void CombineChosenItems();

	/** How many to craft at once with the Combine button (1 up to the crafting component's limit) */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Combine")
	void SetCraftCount(int32 NewCount);

	UFUNCTION(BlueprintPure, Category = "Inventory|Combine")
	int32 GetCraftCount() const { return CraftCount; }

	/** Stops the current craft (what's made stays) */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Combine")
	void CancelCrafting();

	/** --- THE SCREEN --- */

	/** Asks the player controller to close the screen (it gives the controls back to the game) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseScreen();

	/** Rebuilds the item slots (happens by itself when the inventory changes) */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshItems();

	/** Gives the keyboard and gamepad focus to the first item (or the screen if there's none), e.g. right after opening */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void FocusFirstItem();

	/** Level, traits and tradeskills (null if the screen's Widget Blueprint has no CharacterPanel) */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UZCharacterPanelWidget* GetCharacterPanel() const { return CharacterPanel; }

	/** "6 stacks (ID_Rifle, ...); selected: ID_Rifle; combine: ID_Cloth x9; ..." for logs and tests */
	FString GetDebugString() const;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	/** Where an item goes in the list, lower first: weapons, mods, ammo, consumables, tools, materials, salvage */
	virtual int32 GetItemSortOrder(const UZItemInstance* Item) const;

	/** A Blueprint can react here, e.g. play a sound */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnSelectionChanged(UZItemInstance* NewSelection);

	/** A combine finished or failed (the message is in Result Text too) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnCraftingResult(EZCraftingResult Result, const FText& Message);

	UFUNCTION()
	void HandleInventoryChanged(UZInventoryComponent* ChangedInventory);

	UFUNCTION()
	void HandleCraftingResult(UZCraftingComponent* ChangedCrafting, EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message);

	UFUNCTION()
	void HandleCraftingProgressChanged(UZCraftingComponent* ChangedCrafting);

	UFUNCTION()
	void HandleUseClicked();

	UFUNCTION()
	void HandleDropClicked();

	UFUNCTION()
	void HandleAddToCombineClicked();

	UFUNCTION()
	void HandleCombineClicked();

	UFUNCTION()
	void HandleClearCombineClicked();

	UFUNCTION()
	void HandleCountDownClicked();

	UFUNCTION()
	void HandleCountUpClicked();

	UFUNCTION()
	void HandleCancelCraftingClicked();

	UFUNCTION()
	void HandleCloseClicked();

	/** The widget for each item (your own Widget Blueprint with UZItemSlotWidget as its parent class, or the built-in look) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UZItemSlotWidget> ItemSlotClass;

	/** Size of the built-in window; the UI scale shrinks it on smaller screens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	FVector2D DefaultWindowSize = FVector2D(1320.f, 760.f);

	/** --- WIDGETS (all optional, found by name) --- */

	/** The panel holding everything: an item dragged outside it is dropped on the ground */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> WindowPanel;

	/** The item slots go in here, e.g. a Wrap Box (in a Scroll Box) */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> ItemsPanel;

	/** "9 items" or "9 / 30 slots" */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemCountText;

	/** Shows the item under the mouse, or the selected one */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UZItemDetailsWidget> ItemDetails;

	/** Uses the selected item */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UButton> UseButton;

	/** The Use button's label: the item's action, e.g. "Equip", "Use", "Read" */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> UseButtonText;

	/** Drops the selected stack on the ground */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UButton> DropButton;

	/** Puts the selected item in the combine slots */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UButton> AddToCombineButton;

	/** Items dragged onto this go in the combine slots */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CombineArea;

	/** The combine slots go in here */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> CombinePanel;

	/** Shown while the combine slots are empty, e.g. "Drag items here" */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CombineHint;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CombineButton;

	/** Empties the combine slots */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ClearCombineButton;

	/** "x3": how many the Combine button crafts */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftCountText;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CraftCountDownButton;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CraftCountUpButton;

	/** Fills while something is being crafted */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> CraftingProgressBar;

	/** "Crafting Bandage (3 left)" while crafting */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CraftingStatusText;

	/** Stops crafting */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelCraftingButton;

	/** "At the Workbench" or "No crafting station nearby" */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StationText;

	/** What the last combine did, e.g. "New discovery! Crafted Bandage x1." */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Combine", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultText;

	/** Level, traits and tradeskills */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UZCharacterPanelWidget> CharacterPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	/** The Close button's label, with the inventory key, e.g. "Close (I)" */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CloseButtonText;

private:

	/** Listens to the owning player's inventory and crafting */
	void BindToPlayer();

	void UnbindFromPlayer();

	/** Forgets items that left the inventory (selected, under the mouse, in the combine slots) */
	void ForgetMissingItems();

	void RefreshCombineSlots();
	void RefreshDetails();
	void RefreshButtons();
	void RefreshCrafting();
	void RefreshStation();

	/** A few times a second while open: nearby stations, and which item is in the hands */
	void RefreshPeriodic();

	/** The slot at Index in a pool, made and added to the panel if there isn't one yet */
	UZItemSlotWidget* GetOrCreateSlot(TArray<TObjectPtr<UZItemSlotWidget>>& Pool, int32 Index, EZItemSlotRole Role, UPanelWidget* Panel);

	/** True if the screen position is over the widget (as it was last drawn) */
	static bool IsOverWidget(const UWidget* Widget, const FVector2D& ScreenPosition);

	int32 GetMaxCraftCount() const;

	/** How many are in the item's stack (0 if it isn't in the inventory any more) */
	int32 GetStackCount(const UZItemInstance* Item) const;

	TWeakObjectPtr<UZInventoryComponent> Inventory;
	TWeakObjectPtr<UZCraftingComponent> Crafting;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZItemSlotWidget>> ItemSlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZItemSlotWidget>> CombineSlots;

	/** The items in the combine slots */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UZItemInstance>> ChosenItems;

	UPROPERTY(Transient)
	TObjectPtr<UZItemInstance> SelectedItem;

	UPROPERTY(Transient)
	TObjectPtr<UZItemInstance> HoveredItem;

	int32 CraftCount = 1;

	/** The inventory changed: the slots are rebuilt on the next frame (several changes often come at once) */
	bool bItemsDirty = true;

	FTimerHandle PeriodicRefreshTimer;

	/** The item in the character's hands when the slots last showed it (taking a weapon out doesn't change the inventory) */
	TWeakObjectPtr<UZItemInstance> LastEquippedItem;
};
