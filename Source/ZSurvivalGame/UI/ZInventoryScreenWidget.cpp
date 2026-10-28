// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZInventoryScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "ZSurvivalGameCharacter.h"
#include "ZSurvivalGamePlayerController.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Crafting/ZCraftingComponent.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZCraftingStation.h"
#include "Items/ZEquipmentComponent.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"
#include "UI/ZCharacterPanelWidget.h"
#include "UI/ZEquipmentPanelWidget.h"
#include "UI/ZItemDetailsWidget.h"
#include "UI/ZItemDragDropOperation.h"
#include "UI/ZWidgetLayout.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

namespace
{
	/** Seconds between checks for nearby crafting stations and the item in the hands */
	constexpr float PeriodicRefreshInterval = 0.25f;

	/** Combine slots when there's no crafting component to ask */
	constexpr int32 FallbackMaxCombineItems = 6;
}

void UZInventoryScreenWidget::NativeOnInitialized()
{
	// no widgets from a Widget Blueprint: build the built-in look
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (!ItemSlotClass)
	{
		ItemSlotClass = UZItemSlotWidget::StaticClass();
	}

	Super::NativeOnInitialized();

	// keys reach the screen (to close it), and so do drops anywhere on it (to drop items on the ground)
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);

	if (UseButton)
	{
		UseButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleUseClicked);
	}

	if (DropButton)
	{
		DropButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleDropClicked);
	}

	if (AddToCombineButton)
	{
		AddToCombineButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleAddToCombineClicked);
	}

	if (CombineButton)
	{
		CombineButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCombineClicked);
	}

	if (ClearCombineButton)
	{
		ClearCombineButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleClearCombineClicked);
	}

	if (CraftCountDownButton)
	{
		CraftCountDownButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCountDownClicked);
	}

	if (CraftCountUpButton)
	{
		CraftCountUpButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCountUpClicked);
	}

	if (CancelCraftingButton)
	{
		CancelCraftingButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCancelCraftingClicked);
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCloseClicked);
	}

	// its slots select, describe and equip through this screen
	if (EquipmentPanel)
	{
		EquipmentPanel->SetScreen(this);
	}
}

void UZInventoryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToPlayer();

	if (CloseButtonText)
	{
		const AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOwningPlayer());
		const FText KeyName = PlayerController ? PlayerController->GetInventoryKeyName() : FText::GetEmpty();
		CloseButtonText->SetText(KeyName.IsEmpty() ? LOCTEXT("Close", "Close") : FText::Format(LOCTEXT("CloseWithKey", "Close ({0})"), KeyName));
	}

	RefreshItems();
	RefreshCrafting();
	RefreshPeriodic();

	// a game timer rather than the widget's tick, so it keeps going however the screen is drawn
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PeriodicRefreshTimer, FTimerDelegate::CreateUObject(this, &UZInventoryScreenWidget::RefreshPeriodic), PeriodicRefreshInterval, true);
	}
}

void UZInventoryScreenWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicRefreshTimer);
	}

	UnbindFromPlayer();
	HoveredItem = nullptr;

	Super::NativeDestruct();
}

void UZInventoryScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// the progress bar moves smoothly
	RefreshCrafting();
}

void UZInventoryScreenWidget::RefreshPeriodic()
{
	RefreshStation();

	// taking a weapon out doesn't change the inventory, but the slots show which item is in the hands
	const AZSurvivalGameCharacter* Character = Cast<AZSurvivalGameCharacter>(GetOwningPlayerPawn());
	UZItemInstance* EquippedItem = Character ? Character->GetEquippedItem() : nullptr;
	if (EquippedItem != LastEquippedItem.Get())
	{
		LastEquippedItem = EquippedItem;
		for (UZItemSlotWidget* ItemSlot : ItemSlots)
		{
			ItemSlot->Refresh();
		}

		if (EquipmentPanel)
		{
			EquipmentPanel->Refresh();
		}
	}
}

FReply UZInventoryScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// the inventory key closes it again, like Escape and B on a gamepad
	const FKey Key = InKeyEvent.GetKey();
	const AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOwningPlayer());
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || (PlayerController && PlayerController->IsInventoryKey(Key)))
	{
		CloseScreen();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UZInventoryScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// a click on an empty spot keeps the keys on the screen, so the inventory key still closes it
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

bool UZInventoryScreenWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// a drop no slot took: where it landed decides
	const UZItemDragDropOperation* ItemOperation = Cast<UZItemDragDropOperation>(InOperation);
	if (!ItemOperation || !ItemOperation->Item)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	const FVector2D Position = InDragDropEvent.GetScreenSpacePosition();
	if (IsOverWidget(CombineArea, Position))
	{
		if (!ItemOperation->bFromCombineSlot)
		{
			AddToCombine(ItemOperation->Item);
		}

		return true;
	}

	// out of an equipment slot: taken off, never on the ground by accident (between the slots of the equipment window, nothing happens)
	if (ItemOperation->bFromEquipmentSlot)
	{
		if (!IsOverWidget(EquipmentPanel, Position))
		{
			UnequipItem(ItemOperation->Item);
		}

		return true;
	}

	// out of the combine slots: back to just being in the inventory, never on the ground by accident
	if (ItemOperation->bFromCombineSlot)
	{
		RemoveFromCombine(ItemOperation->Item);
		return true;
	}

	// outside the window: on the ground (inside it, nothing happens)
	const bool bInsideWindow = WindowPanel ? IsOverWidget(WindowPanel, Position) : IsOverWidget(ItemsPanel, Position);
	if (!bInsideWindow)
	{
		DropItem(ItemOperation->Item, 0);
	}

	return true;
}

void UZInventoryScreenWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// darkens the game behind the window; items dropped on it land on the ground
	UBorder* Backdrop = MakeBorder(*WidgetTree, TEXT("Backdrop"), BackdropColor, FMargin(0.f));
	if (UOverlaySlot* BackdropSlot = Root->AddChildToOverlay(Backdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	USizeBox* WindowSize = MakeSizeBox(*WidgetTree, TEXT("WindowSize"), DefaultWindowSize.X, DefaultWindowSize.Y);
	if (UOverlaySlot* WindowSlot = Root->AddChildToOverlay(WindowSize))
	{
		WindowSlot->SetHorizontalAlignment(HAlign_Center);
		WindowSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* Window = MakeBorder(*WidgetTree, TEXT("WindowPanel"), PanelColor, FMargin(16.f));
	WindowSize->SetContent(Window);
	WindowPanel = Window;

	UVerticalBox* WindowColumn = MakeVerticalBox(*WidgetTree, TEXT("WindowColumn"));
	Window->SetContent(WindowColumn);

	// --- header: title and close button
	UHorizontalBox* Header = MakeHorizontalBox(*WidgetTree, TEXT("Header"));
	AddToVerticalBox(WindowColumn, Header, FMargin(0.f, 0.f, 0.f, 12.f));

	UTextBlock* Title = MakeText(*WidgetTree, TEXT("Title"), 20, AccentColor, true);
	Title->SetText(LOCTEXT("Title", "INVENTORY"));
	AddToHorizontalBox(Header, Title, FMargin(0.f), 1.f);

	UTextBlock* CloseLabel = nullptr;
	CloseButton = MakeButton(*WidgetTree, TEXT("CloseButton"), LOCTEXT("Close", "Close"), 12, &CloseLabel);
	CloseButtonText = CloseLabel;
	AddToHorizontalBox(Header, CloseButton);

	UHorizontalBox* Body = MakeHorizontalBox(*WidgetTree, TEXT("Body"));
	AddToVerticalBox(WindowColumn, Body, FMargin(0.f), true);

	// --- far left: the equipment slots (three to a row)
	UBorder* EquipmentSection = MakeBorder(*WidgetTree, TEXT("EquipmentSection"), SectionColor, FMargin(10.f));
	USizeBox* EquipmentSize = MakeSizeBox(*WidgetTree, TEXT("EquipmentSize"), 316.f, 0.f);
	EquipmentSize->SetContent(EquipmentSection);
	AddToHorizontalBox(Body, EquipmentSize, FMargin(0.f, 0.f, 12.f, 0.f), 0.f, VAlign_Fill);

	EquipmentPanel = WidgetTree->ConstructWidget<UZEquipmentPanelWidget>(UZEquipmentPanelWidget::StaticClass(), TEXT("EquipmentPanel"));
	EquipmentSection->SetContent(EquipmentPanel);

	// --- left: the items
	UVerticalBox* ItemsColumn = MakeVerticalBox(*WidgetTree, TEXT("ItemsColumn"));
	AddToHorizontalBox(Body, ItemsColumn, FMargin(0.f, 0.f, 12.f, 0.f), 1.45f, VAlign_Fill);

	ItemCountText = MakeText(*WidgetTree, TEXT("ItemCountText"), 11, MutedTextColor);
	AddToVerticalBox(ItemsColumn, ItemCountText, FMargin(0.f, 0.f, 0.f, 6.f));

	UBorder* ItemsSection = MakeBorder(*WidgetTree, TEXT("ItemsSection"), SectionColor, FMargin(8.f));
	AddToVerticalBox(ItemsColumn, ItemsSection, FMargin(0.f), true);

	UScrollBox* ItemsScroll = MakeScrollBox(*WidgetTree, TEXT("ItemsScroll"));
	ItemsSection->SetContent(ItemsScroll);

	UWrapBox* Items = MakeWrapBox(*WidgetTree, TEXT("ItemsPanel"), 6.f);
	ItemsScroll->AddChild(Items);
	ItemsPanel = Items;

	UTextBlock* ItemsHint = MakeText(*WidgetTree, TEXT("ItemsHint"), 10, MutedTextColor, false, true);
	ItemsHint->SetText(LOCTEXT("ItemsHint", "Right-click to use or equip. Drag an item onto another to try them together, onto an equipment slot to equip it, or out of the window to drop it."));
	AddToVerticalBox(ItemsColumn, ItemsHint, FMargin(0.f, 6.f, 0.f, 0.f));

	// --- middle: details, item buttons and the combine slots
	UVerticalBox* MiddleColumn = MakeVerticalBox(*WidgetTree, TEXT("MiddleColumn"));
	AddToHorizontalBox(Body, MiddleColumn, FMargin(0.f, 0.f, 12.f, 0.f), 1.f, VAlign_Fill);

	UBorder* DetailsSection = MakeBorder(*WidgetTree, TEXT("DetailsSection"), SectionColor, FMargin(10.f));
	AddToVerticalBox(MiddleColumn, DetailsSection, FMargin(0.f), true);

	UScrollBox* DetailsScroll = MakeScrollBox(*WidgetTree, TEXT("DetailsScroll"));
	DetailsSection->SetContent(DetailsScroll);

	ItemDetails = WidgetTree->ConstructWidget<UZItemDetailsWidget>(UZItemDetailsWidget::StaticClass(), TEXT("ItemDetails"));
	DetailsScroll->AddChild(ItemDetails);

	UHorizontalBox* ItemActions = MakeHorizontalBox(*WidgetTree, TEXT("ItemActions"));
	AddToVerticalBox(MiddleColumn, ItemActions, FMargin(0.f, 6.f));

	UTextBlock* UseLabel = nullptr;
	UseButton = MakeButton(*WidgetTree, TEXT("UseButton"), LOCTEXT("UseLabel", "Use"), 12, &UseLabel);
	UseButtonText = UseLabel;
	AddToHorizontalBox(ItemActions, UseButton, FMargin(0.f, 0.f, 6.f, 0.f), 1.f);

	DropButton = MakeButton(*WidgetTree, TEXT("DropButton"), LOCTEXT("DropLabel", "Drop"));
	AddToHorizontalBox(ItemActions, DropButton, FMargin(0.f, 0.f, 6.f, 0.f), 1.f);

	AddToCombineButton = MakeButton(*WidgetTree, TEXT("AddToCombineButton"), LOCTEXT("AddToCombineLabel", "Add to combine"));
	AddToHorizontalBox(ItemActions, AddToCombineButton, FMargin(0.f), 1.4f);

	UBorder* CombineSection = MakeBorder(*WidgetTree, TEXT("CombineArea"), SectionColor, FMargin(10.f));
	AddToVerticalBox(MiddleColumn, CombineSection);
	CombineArea = CombineSection;

	UVerticalBox* CombineColumn = MakeVerticalBox(*WidgetTree, TEXT("CombineColumn"));
	CombineSection->SetContent(CombineColumn);

	UHorizontalBox* CombineHeader = MakeHorizontalBox(*WidgetTree, TEXT("CombineHeader"));
	AddToVerticalBox(CombineColumn, CombineHeader, FMargin(0.f, 0.f, 0.f, 6.f));

	UTextBlock* CombineTitle = MakeText(*WidgetTree, TEXT("CombineTitle"), 13, MutedTextColor, true);
	CombineTitle->SetText(LOCTEXT("CombineTitle", "COMBINE"));
	AddToHorizontalBox(CombineHeader, CombineTitle, FMargin(0.f, 0.f, 10.f, 0.f));

	StationText = MakeText(*WidgetTree, TEXT("StationText"), 10, MutedTextColor);
	StationText->SetJustification(ETextJustify::Right);
	AddToHorizontalBox(CombineHeader, StationText, FMargin(0.f), 1.f);

	UTextBlock* Hint = MakeText(*WidgetTree, TEXT("CombineHint"), 10, MutedTextColor, false, true);
	Hint->SetText(LOCTEXT("CombineHint", "Drag items here (or select one and press Add) to try them together. Nobody tells you what works."));
	AddToVerticalBox(CombineColumn, Hint, FMargin(0.f, 0.f, 0.f, 6.f));
	CombineHint = Hint;

	UWrapBox* Chosen = MakeWrapBox(*WidgetTree, TEXT("CombinePanel"), 6.f);
	AddToVerticalBox(CombineColumn, Chosen, FMargin(0.f, 0.f, 0.f, 6.f));
	CombinePanel = Chosen;

	UHorizontalBox* CombineButtons = MakeHorizontalBox(*WidgetTree, TEXT("CombineButtons"));
	AddToVerticalBox(CombineColumn, CombineButtons, FMargin(0.f, 0.f, 0.f, 6.f));

	CraftCountDownButton = MakeButton(*WidgetTree, TEXT("CraftCountDownButton"), LOCTEXT("CountDown", "-"), 13);
	AddToHorizontalBox(CombineButtons, CraftCountDownButton);

	CraftCountText = MakeText(*WidgetTree, TEXT("CraftCountText"), 13, TextColor, true);
	CraftCountText->SetJustification(ETextJustify::Center);
	USizeBox* CountSize = MakeSizeBox(*WidgetTree, TEXT("CraftCountSize"), 40.f, 0.f);
	CountSize->SetContent(CraftCountText);
	AddToHorizontalBox(CombineButtons, CountSize);

	CraftCountUpButton = MakeButton(*WidgetTree, TEXT("CraftCountUpButton"), LOCTEXT("CountUp", "+"), 13);
	AddToHorizontalBox(CombineButtons, CraftCountUpButton, FMargin(0.f, 0.f, 8.f, 0.f));

	CombineButton = MakeButton(*WidgetTree, TEXT("CombineButton"), LOCTEXT("CombineLabel", "Combine"), 13);
	CombineButton->SetBackgroundColor(PrimaryButtonColor);
	AddToHorizontalBox(CombineButtons, CombineButton, FMargin(0.f, 0.f, 6.f, 0.f), 1.f);

	ClearCombineButton = MakeButton(*WidgetTree, TEXT("ClearCombineButton"), LOCTEXT("ClearLabel", "Clear"));
	AddToHorizontalBox(CombineButtons, ClearCombineButton);

	UHorizontalBox* ProgressRow = MakeHorizontalBox(*WidgetTree, TEXT("ProgressRow"));
	AddToVerticalBox(CombineColumn, ProgressRow, FMargin(0.f, 0.f, 0.f, 4.f));

	CraftingProgressBar = MakeProgressBar(*WidgetTree, TEXT("CraftingProgressBar"), AccentColor);
	USizeBox* ProgressSize = MakeSizeBox(*WidgetTree, TEXT("CraftingProgressSize"), 0.f, 8.f);
	ProgressSize->SetContent(CraftingProgressBar);
	AddToHorizontalBox(ProgressRow, ProgressSize, FMargin(0.f, 0.f, 6.f, 0.f), 1.f);

	CancelCraftingButton = MakeButton(*WidgetTree, TEXT("CancelCraftingButton"), LOCTEXT("StopLabel", "Stop"), 10);
	AddToHorizontalBox(ProgressRow, CancelCraftingButton);

	CraftingStatusText = MakeText(*WidgetTree, TEXT("CraftingStatusText"), 10, MutedTextColor);
	AddToVerticalBox(CombineColumn, CraftingStatusText);

	ResultText = MakeText(*WidgetTree, TEXT("ResultText"), 11, AccentColor, false, true);
	AddToVerticalBox(CombineColumn, ResultText, FMargin(0.f, 4.f, 0.f, 0.f));

	// --- right: the character
	UBorder* CharacterSection = MakeBorder(*WidgetTree, TEXT("CharacterSection"), SectionColor, FMargin(10.f));
	AddToHorizontalBox(Body, CharacterSection, FMargin(0.f), 1.f, VAlign_Fill);

	CharacterPanel = WidgetTree->ConstructWidget<UZCharacterPanelWidget>(UZCharacterPanelWidget::StaticClass(), TEXT("CharacterPanel"));
	CharacterSection->SetContent(CharacterPanel);
}

void UZInventoryScreenWidget::BindToPlayer()
{
	// they live on the PlayerState, so they're the same after dying and respawning
	UZInventoryComponent* NewInventory = UZInventoryComponent::FindInventoryComponent(GetOwningPlayer());
	UZEquipmentComponent* NewEquipment = UZEquipmentComponent::FindEquipmentComponent(GetOwningPlayer());
	UZCraftingComponent* NewCrafting = UZCraftingComponent::FindCraftingComponent(GetOwningPlayer());
	if (NewInventory == Inventory.Get() && NewEquipment == Equipment.Get() && NewCrafting == Crafting.Get())
	{
		return;
	}

	UnbindFromPlayer();
	Inventory = NewInventory;
	Equipment = NewEquipment;
	Crafting = NewCrafting;

	if (NewInventory)
	{
		NewInventory->OnInventoryChanged.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleInventoryChanged);
	}

	if (NewEquipment)
	{
		NewEquipment->OnEquipmentChanged.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleEquipmentChanged);
	}

	if (NewCrafting)
	{
		NewCrafting->OnCraftingResult.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCraftingResult);
		NewCrafting->OnCraftingProgressChanged.AddUniqueDynamic(this, &UZInventoryScreenWidget::HandleCraftingProgressChanged);
	}

	bItemsDirty = true;
}

void UZInventoryScreenWidget::UnbindFromPlayer()
{
	if (UZInventoryComponent* OldInventory = Inventory.Get())
	{
		OldInventory->OnInventoryChanged.RemoveDynamic(this, &UZInventoryScreenWidget::HandleInventoryChanged);
	}

	if (UZEquipmentComponent* OldEquipment = Equipment.Get())
	{
		OldEquipment->OnEquipmentChanged.RemoveDynamic(this, &UZInventoryScreenWidget::HandleEquipmentChanged);
	}

	if (UZCraftingComponent* OldCrafting = Crafting.Get())
	{
		OldCrafting->OnCraftingResult.RemoveDynamic(this, &UZInventoryScreenWidget::HandleCraftingResult);
		OldCrafting->OnCraftingProgressChanged.RemoveDynamic(this, &UZInventoryScreenWidget::HandleCraftingProgressChanged);
	}

	Inventory.Reset();
	Equipment.Reset();
	Crafting.Reset();
}

void UZInventoryScreenWidget::SelectItem(UZItemInstance* Item)
{
	SelectedItem = Item && GetStackCount(Item) > 0 ? Item : nullptr;

	for (UZItemSlotWidget* ItemSlot : ItemSlots)
	{
		ItemSlot->SetSelected(SelectedItem && ItemSlot->GetItem() == SelectedItem);
	}

	if (EquipmentPanel)
	{
		EquipmentPanel->SetSelectedItem(SelectedItem);
	}

	RefreshDetails();
	RefreshButtons();
	OnSelectionChanged(SelectedItem);
}

void UZInventoryScreenWidget::SetHoveredItem(UZItemInstance* Item)
{
	HoveredItem = Item;
	RefreshDetails();
}

void UZInventoryScreenWidget::ClearHoveredItem(UZItemInstance* Item)
{
	if (HoveredItem == Item)
	{
		HoveredItem = nullptr;
		RefreshDetails();
	}
}

void UZInventoryScreenWidget::UseItem(UZItemInstance* Item)
{
	// what's equipped comes off again, so the same click puts things on and takes them off
	if (IsEquipped(Item))
	{
		UnequipItem(Item);
		return;
	}

	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	if (UZInventoryComponent* PlayerInventory = Inventory.Get(); PlayerInventory && Definition && Definition->IsUsable() && GetStackCount(Item) > 0)
	{
		PlayerInventory->UseItem(Item);
	}
}

void UZInventoryScreenWidget::EquipItem(UZItemInstance* Item, FZEquipmentSlot EquipmentSlot)
{
	if (UZEquipmentComponent* PlayerEquipment = Equipment.Get(); PlayerEquipment && GetStackCount(Item) > 0)
	{
		PlayerEquipment->RequestEquip(Item, EquipmentSlot);
	}
}

void UZInventoryScreenWidget::UnequipItem(UZItemInstance* Item)
{
	if (UZEquipmentComponent* PlayerEquipment = Equipment.Get(); PlayerEquipment && PlayerEquipment->IsEquipped(Item))
	{
		PlayerEquipment->RequestUnequip(Item);
	}
}

bool UZInventoryScreenWidget::CanEquip(const UZItemInstance* Item, const FZEquipmentSlot& EquipmentSlot) const
{
	return Equipment.IsValid() && Equipment->CanEquip(Item, EquipmentSlot);
}

bool UZInventoryScreenWidget::IsEquipped(const UZItemInstance* Item) const
{
	return Item && Equipment.IsValid() && Equipment->IsEquipped(Item);
}

void UZInventoryScreenWidget::DropItem(UZItemInstance* Item, int32 Count)
{
	const int32 StackCount = GetStackCount(Item);
	if (UZInventoryComponent* PlayerInventory = Inventory.Get(); PlayerInventory && StackCount > 0)
	{
		PlayerInventory->DropItem(Item, Count > 0 ? FMath::Min(Count, StackCount) : StackCount);
	}
}

void UZInventoryScreenWidget::CombineItems(UZItemInstance* First, UZItemInstance* Second)
{
	if (UZCraftingComponent* PlayerCrafting = Crafting.Get(); PlayerCrafting && First && Second && First != Second)
	{
		PlayerCrafting->CombineItems({ First, Second }, 1);
	}
}

bool UZInventoryScreenWidget::AddToCombine(UZItemInstance* Item)
{
	const int32 MaxItems = Crafting.IsValid() ? Crafting->GetMaxCombineItems() : FallbackMaxCombineItems;
	if (!Item || GetStackCount(Item) <= 0 || ChosenItems.Contains(Item) || ChosenItems.Num() >= MaxItems)
	{
		return false;
	}

	ChosenItems.Add(Item);
	RefreshCombineSlots();
	RefreshButtons();
	return true;
}

void UZInventoryScreenWidget::RemoveFromCombine(UZItemInstance* Item)
{
	if (ChosenItems.Remove(Item) > 0)
	{
		RefreshCombineSlots();
		RefreshButtons();
	}
}

void UZInventoryScreenWidget::ClearCombine()
{
	ChosenItems.Reset();
	RefreshCombineSlots();
	RefreshButtons();
}

TArray<UZItemInstance*> UZInventoryScreenWidget::GetCombineItems() const
{
	TArray<UZItemInstance*> Items;
	for (UZItemInstance* Item : ChosenItems)
	{
		Items.Add(Item);
	}

	return Items;
}

void UZInventoryScreenWidget::CombineChosenItems()
{
	// the items stay in the slots (the ones used up drop out), so the same combination can be tried again
	if (UZCraftingComponent* PlayerCrafting = Crafting.Get(); PlayerCrafting && !ChosenItems.IsEmpty())
	{
		PlayerCrafting->CombineItems(GetCombineItems(), CraftCount);
	}
}

void UZInventoryScreenWidget::SetCraftCount(int32 NewCount)
{
	CraftCount = FMath::Clamp(NewCount, 1, GetMaxCraftCount());
	RefreshButtons();
}

void UZInventoryScreenWidget::CancelCrafting()
{
	if (UZCraftingComponent* PlayerCrafting = Crafting.Get())
	{
		PlayerCrafting->CancelCrafting();
	}
}

void UZInventoryScreenWidget::CloseScreen()
{
	if (AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOwningPlayer()))
	{
		PlayerController->CloseInventory();
	}
	else
	{
		RemoveFromParent();
	}
}

void UZInventoryScreenWidget::RefreshItems()
{
	bItemsDirty = false;
	ForgetMissingItems();

	TArray<FZInventoryItem> Stacks = Inventory.IsValid() ? Inventory->GetItems() : TArray<FZInventoryItem>();
	Stacks.RemoveAll([](const FZInventoryItem& Stack) { return !Stack.Instance || !Stack.Instance->GetItemDefinition(); });
	Stacks.StableSort([this](const FZInventoryItem& A, const FZInventoryItem& B)
	{
		const int32 OrderA = GetItemSortOrder(A.Instance);
		const int32 OrderB = GetItemSortOrder(B.Instance);
		return OrderA != OrderB ? OrderA < OrderB : A.Instance->GetItemDefinition()->GetItemName().CompareTo(B.Instance->GetItemDefinition()->GetItemName()) < 0;
	});

	if (ItemsPanel)
	{
		// the slot widgets are kept and reused, only their items change
		for (int32 Index = 0; Index < Stacks.Num(); ++Index)
		{
			if (UZItemSlotWidget* ItemSlot = GetOrCreateSlot(ItemSlots, Index, EZItemSlotRole::Inventory, ItemsPanel))
			{
				ItemSlot->SetItem(Stacks[Index].Instance, Stacks[Index].Count);
				ItemSlot->SetSelected(SelectedItem && Stacks[Index].Instance == SelectedItem);
				ItemSlot->SetVisibility(ESlateVisibility::Visible);
			}
		}

		for (int32 Index = Stacks.Num(); Index < ItemSlots.Num(); ++Index)
		{
			ItemSlots[Index]->SetItem(nullptr, 0);
			ItemSlots[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ItemCountText)
	{
		const int32 MaxSlots = Inventory.IsValid() ? Inventory->GetMaxSlots() : 0;
		ItemCountText->SetText(MaxSlots > 0
			? FText::Format(LOCTEXT("SlotCount", "{0} / {1} slots"), Stacks.Num(), MaxSlots)
			: FText::Format(LOCTEXT("ItemCount", "{0} {0}|plural(one=item,other=items)"), Stacks.Num()));
	}

	// an equipped item can arrive a moment after the list says where it is
	if (EquipmentPanel)
	{
		EquipmentPanel->Refresh();
	}

	RefreshCombineSlots();
	RefreshDetails();
	RefreshButtons();
}

void UZInventoryScreenWidget::FocusFirstItem()
{
	for (UZItemSlotWidget* ItemSlot : ItemSlots)
	{
		if (ItemSlot->GetItem() && ItemSlot->IsVisible())
		{
			ItemSlot->SetFocus();
			return;
		}
	}

	SetFocus();
}

void UZInventoryScreenWidget::ForgetMissingItems()
{
	if (SelectedItem && GetStackCount(SelectedItem) <= 0)
	{
		SelectedItem = nullptr;
		OnSelectionChanged(nullptr);
	}

	if (HoveredItem && GetStackCount(HoveredItem) <= 0)
	{
		HoveredItem = nullptr;
	}

	ChosenItems.RemoveAll([this](const TObjectPtr<UZItemInstance>& Item) { return GetStackCount(Item) <= 0; });
}

void UZInventoryScreenWidget::RefreshCombineSlots()
{
	if (CombinePanel)
	{
		for (int32 Index = 0; Index < ChosenItems.Num(); ++Index)
		{
			if (UZItemSlotWidget* CombineSlot = GetOrCreateSlot(CombineSlots, Index, EZItemSlotRole::Combine, CombinePanel))
			{
				CombineSlot->SetItem(ChosenItems[Index], GetStackCount(ChosenItems[Index]));
				CombineSlot->SetVisibility(ESlateVisibility::Visible);
			}
		}

		for (int32 Index = ChosenItems.Num(); Index < CombineSlots.Num(); ++Index)
		{
			CombineSlots[Index]->SetItem(nullptr, 0);
			CombineSlots[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (CombineHint)
	{
		CombineHint->SetVisibility(ChosenItems.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UZInventoryScreenWidget::RefreshDetails()
{
	if (ItemDetails)
	{
		UZItemInstance* ShownItem = HoveredItem ? HoveredItem.Get() : SelectedItem.Get();
		ItemDetails->SetItem(ShownItem, GetStackCount(ShownItem));
	}
}

void UZInventoryScreenWidget::RefreshButtons()
{
	const UZItemDefinition* Selected = SelectedItem ? SelectedItem->GetItemDefinition() : nullptr;
	const bool bSelectedIsEquipped = IsEquipped(SelectedItem);
	const bool bUsable = Selected && (Selected->IsUsable() || bSelectedIsEquipped);
	const bool bCrafting = Crafting.IsValid() && Crafting->IsCrafting();
	const int32 MaxItems = Crafting.IsValid() ? Crafting->GetMaxCombineItems() : FallbackMaxCombineItems;

	if (UseButton)
	{
		UseButton->SetIsEnabled(bUsable);
	}

	if (UseButtonText)
	{
		UseButtonText->SetText(bSelectedIsEquipped ? LOCTEXT("UnequipLabel", "Unequip") : bUsable ? Selected->GetUseActionName() : LOCTEXT("UseLabel", "Use"));
	}

	if (DropButton)
	{
		DropButton->SetIsEnabled(Selected != nullptr);
	}

	if (AddToCombineButton)
	{
		AddToCombineButton->SetIsEnabled(Selected && !ChosenItems.Contains(SelectedItem) && ChosenItems.Num() < MaxItems);
	}

	if (CombineButton)
	{
		CombineButton->SetIsEnabled(!ChosenItems.IsEmpty() && !bCrafting);
	}

	if (ClearCombineButton)
	{
		ClearCombineButton->SetIsEnabled(!ChosenItems.IsEmpty());
	}

	if (CraftCountText)
	{
		CraftCountText->SetText(FText::Format(LOCTEXT("CraftCount", "x{0}"), CraftCount));
	}

	if (CraftCountDownButton)
	{
		CraftCountDownButton->SetIsEnabled(CraftCount > 1);
	}

	if (CraftCountUpButton)
	{
		CraftCountUpButton->SetIsEnabled(CraftCount < GetMaxCraftCount());
	}

	if (CancelCraftingButton)
	{
		CancelCraftingButton->SetIsEnabled(bCrafting);
	}
}

void UZInventoryScreenWidget::RefreshCrafting()
{
	const UZCraftingComponent* PlayerCrafting = Crafting.Get();
	const bool bCrafting = PlayerCrafting && PlayerCrafting->IsCrafting();

	if (CraftingProgressBar)
	{
		CraftingProgressBar->SetPercent(bCrafting ? PlayerCrafting->GetCraftingProgressPercent() : 0.f);
	}

	if (CraftingStatusText)
	{
		FText Status;
		if (bCrafting)
		{
			const FZCraftingProgress& Progress = PlayerCrafting->GetCraftingProgress();
			const FText RecipeName = Progress.Recipe ? Progress.Recipe->GetRecipeName() : FText::GetEmpty();
			Status = Progress.RemainingCount > 1
				? FText::Format(LOCTEXT("CraftingMany", "Crafting {0} ({1} left)"), RecipeName, Progress.RemainingCount)
				: FText::Format(LOCTEXT("CraftingOne", "Crafting {0}"), RecipeName);
		}

		CraftingStatusText->SetText(Status);
		CraftingStatusText->SetVisibility(bCrafting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UZInventoryScreenWidget::RefreshStation()
{
	if (!StationText)
	{
		return;
	}

	TArray<FText> StationNames;
	TSet<FString> SeenNames;
	if (const UZCraftingComponent* PlayerCrafting = Crafting.Get())
	{
		for (const AZCraftingStation* Station : PlayerCrafting->GetNearbyStations())
		{
			const FText StationName = Station->GetStationName();
			if (!SeenNames.Contains(StationName.ToString()))
			{
				SeenNames.Add(StationName.ToString());
				StationNames.Add(StationName);
			}
		}
	}

	StationText->SetText(StationNames.IsEmpty()
		? LOCTEXT("NoStation", "No crafting station nearby")
		: FText::Format(LOCTEXT("AtStation", "At the {0}"), FText::Join(LOCTEXT("ListSeparator", ", "), StationNames)));
	ZWidgetLayout::SetTextColor(StationText, StationNames.IsEmpty() ? ZWidgetLayout::MutedTextColor : ZWidgetLayout::GoodColor);
}

UZItemSlotWidget* UZInventoryScreenWidget::GetOrCreateSlot(TArray<TObjectPtr<UZItemSlotWidget>>& Pool, int32 Index, EZItemSlotRole Role, UPanelWidget* Panel)
{
	if (Pool.IsValidIndex(Index))
	{
		return Pool[Index];
	}

	UZItemSlotWidget* NewSlot = CreateWidget<UZItemSlotWidget>(this, ItemSlotClass);
	if (!NewSlot)
	{
		return nullptr;
	}

	NewSlot->SetScreen(this, Role);
	ZWidgetLayout::AddToPanel(Panel, NewSlot);
	Pool.Add(NewSlot);
	return NewSlot;
}

bool UZInventoryScreenWidget::IsOverWidget(const UWidget* Widget, const FVector2D& ScreenPosition)
{
	return Widget && Widget->IsVisible() && Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

int32 UZInventoryScreenWidget::GetMaxCraftCount() const
{
	return Crafting.IsValid() ? Crafting->GetMaxCraftsPerRequest() : 1;
}

int32 UZInventoryScreenWidget::GetStackCount(const UZItemInstance* Item) const
{
	return Item && Inventory.IsValid() ? Inventory->GetStackCount(Item) : 0;
}

int32 UZInventoryScreenWidget::GetItemSortOrder(const UZItemInstance* Item) const
{
	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	const FGameplayTag Type = Definition ? Definition->GetItemTypeTag() : FGameplayTag();

	static const TArray<FGameplayTag> Order = {
		ZGameplayTags::Item_Type_Weapon, ZGameplayTags::Item_Type_Armor, ZGameplayTags::Item_Type_Jewelry, ZGameplayTags::Item_Type_WeaponMod, ZGameplayTags::Item_Type_Ammo,
		ZGameplayTags::Item_Type_Consumable, ZGameplayTags::Item_Type_Tool, ZGameplayTags::Item_Type_Material, ZGameplayTags::Item_Type_Salvage
	};

	// anything else goes last
	const int32 Index = Order.IndexOfByKey(Type);
	return Index != INDEX_NONE ? Index : Order.Num();
}

void UZInventoryScreenWidget::HandleInventoryChanged(UZInventoryComponent* ChangedInventory)
{
	// several changes often come at once (a craft takes items and gives others): rebuild once, on the next frame
	UWorld* World = GetWorld();
	if (!bItemsDirty && World)
	{
		bItemsDirty = true;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bItemsDirty)
			{
				RefreshItems();
			}
		}));
	}
}

void UZInventoryScreenWidget::HandleEquipmentChanged(UZEquipmentComponent* ChangedEquipment)
{
	// the items show "Equipped", and the Use button says Unequip for an equipped item (the equipment window listens by itself)
	for (UZItemSlotWidget* ItemSlot : ItemSlots)
	{
		ItemSlot->Refresh();
	}

	RefreshDetails();
	RefreshButtons();
}

void UZInventoryScreenWidget::HandleCraftingResult(UZCraftingComponent* ChangedCrafting, EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message, bool bNewDiscovery)
{
	if (ResultText)
	{
		ResultText->SetText(Message);
	}

	OnCraftingResult(Result, Message, bNewDiscovery);
}

void UZInventoryScreenWidget::HandleCraftingProgressChanged(UZCraftingComponent* ChangedCrafting)
{
	RefreshCrafting();
	RefreshButtons();
}

void UZInventoryScreenWidget::HandleUseClicked()
{
	UseItem(SelectedItem);
}

void UZInventoryScreenWidget::HandleDropClicked()
{
	DropItem(SelectedItem, 0);
}

void UZInventoryScreenWidget::HandleAddToCombineClicked()
{
	AddToCombine(SelectedItem);
}

void UZInventoryScreenWidget::HandleCombineClicked()
{
	CombineChosenItems();
}

void UZInventoryScreenWidget::HandleClearCombineClicked()
{
	ClearCombine();
}

void UZInventoryScreenWidget::HandleCountDownClicked()
{
	SetCraftCount(CraftCount - 1);
}

void UZInventoryScreenWidget::HandleCountUpClicked()
{
	SetCraftCount(CraftCount + 1);
}

void UZInventoryScreenWidget::HandleCancelCraftingClicked()
{
	CancelCrafting();
}

void UZInventoryScreenWidget::HandleCloseClicked()
{
	CloseScreen();
}

FString UZInventoryScreenWidget::GetDebugString() const
{
	TArray<FString> Shown;
	for (const UZItemSlotWidget* ItemSlot : ItemSlots)
	{
		if (const UZItemInstance* Item = ItemSlot->GetItem())
		{
			Shown.Add(FString::Printf(TEXT("%s x%d"), *GetNameSafe(Item->GetItemDefinition()), ItemSlot->GetCount()));
		}
	}

	TArray<FString> Chosen;
	for (const UZItemInstance* Item : ChosenItems)
	{
		Chosen.Add(GetNameSafe(Item ? Item->GetItemDefinition() : nullptr));
	}

	const UZItemInstance* DetailsItem = ItemDetails ? ItemDetails->GetItem() : nullptr;
	return FString::Printf(TEXT("%d stacks (%s); selected: %s; use button: %s; combine: [%s] x%d; details: %s (%s); station: %s; result: %s; equipment window: %s"),
		Shown.Num(), *FString::Join(Shown, TEXT(", ")), *GetNameSafe(SelectedItem ? SelectedItem->GetItemDefinition() : nullptr),
		UseButtonText ? *UseButtonText->GetText().ToString() : TEXT("-"),
		*FString::Join(Chosen, TEXT(", ")), CraftCount, *GetNameSafe(DetailsItem ? DetailsItem->GetItemDefinition() : nullptr),
		ItemDetails ? *ItemDetails->GetStatsText().ToString().Replace(TEXT("\n"), TEXT(" | ")) : TEXT(""),
		StationText ? *StationText->GetText().ToString() : TEXT("-"), ResultText ? *ResultText->GetText().ToString() : TEXT("-"),
		EquipmentPanel ? *EquipmentPanel->GetDebugString() : TEXT("none"));
}

#undef LOCTEXT_NAMESPACE
