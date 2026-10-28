// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZItemSlotWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "InputCoreTypes.h"
#include "ZSurvivalGameCharacter.h"
#include "Items/ZEquipmentComponent.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"
#include "UI/ZInventoryScreenWidget.h"
#include "UI/ZItemDragDropOperation.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

void UZItemSlotWidget::NativeOnInitialized()
{
	// no widgets from a Widget Blueprint: build the built-in look
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	Super::NativeOnInitialized();

	if (NameText)
	{
		NameTextColor = NameText->GetColorAndOpacity();
	}

	// gamepad and keyboard move between slots; the mouse needs it to be hit
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	Refresh();
}

void UZItemSlotWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	USizeBox* SlotSize = MakeSizeBox(*WidgetTree, TEXT("SlotSize"), DefaultSize, DefaultSize);
	WidgetTree->RootWidget = SlotSize;

	Background = MakeBorder(*WidgetTree, TEXT("Background"), NormalColor, FMargin(5.f));
	SlotSize->SetContent(Background);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Layers"));
	Background->SetContent(Layers);

	// the icon fills the slot, the texts go on top
	IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IconImage"));
	if (UOverlaySlot* IconSlot = Layers->AddChildToOverlay(IconImage))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UVerticalBox* Column = MakeVerticalBox(*WidgetTree, TEXT("Column"));
	if (UOverlaySlot* ColumnSlot = Layers->AddChildToOverlay(Column))
	{
		ColumnSlot->SetHorizontalAlignment(HAlign_Fill);
		ColumnSlot->SetVerticalAlignment(VAlign_Fill);
	}

	NameText = MakeText(*WidgetTree, TEXT("NameText"), 10, TextColor, false, true);
	AddToVerticalBox(Column, NameText);

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("Gap"));
	AddToVerticalBox(Column, Gap, FMargin(0.f), true);

	DurabilityBar = MakeProgressBar(*WidgetTree, TEXT("DurabilityBar"), AccentColor);
	USizeBox* BarSize = MakeSizeBox(*WidgetTree, TEXT("DurabilitySize"), 0.f, 4.f);
	BarSize->SetContent(DurabilityBar);
	AddToVerticalBox(Column, BarSize, FMargin(0.f, 2.f));

	UHorizontalBox* BottomRow = MakeHorizontalBox(*WidgetTree, TEXT("BottomRow"));
	AddToVerticalBox(Column, BottomRow);

	UTextBlock* EquippedText = MakeText(*WidgetTree, TEXT("EquippedMarker"), 9, AccentColor, true);
	EquippedText->SetText(LOCTEXT("Equipped", "In hands"));
	EquippedMarker = EquippedText;
	AddToHorizontalBox(BottomRow, EquippedText, FMargin(0.f), 1.f, VAlign_Bottom);

	CountText = MakeText(*WidgetTree, TEXT("CountText"), 12, TextColor, true);
	CountText->SetJustification(ETextJustify::Right);
	AddToHorizontalBox(BottomRow, CountText, FMargin(0.f), 0.f, VAlign_Bottom);
}

void UZItemSlotWidget::SetItem(UZItemInstance* InItem, int32 InCount)
{
	Item = InItem;
	Count = InCount;
	Refresh();
}

void UZItemSlotWidget::SetScreen(UZInventoryScreenWidget* InScreen, EZItemSlotRole InRole)
{
	Screen = InScreen;
	Role = InRole;
	Refresh();
}

void UZItemSlotWidget::SetEquipmentSlot(const FZEquipmentSlot& InEquipmentSlot)
{
	EquipmentSlot = InEquipmentSlot;
	Refresh();
}

void UZItemSlotWidget::SetSelected(bool bInSelected)
{
	if (bSelected != bInSelected)
	{
		bSelected = bInSelected;
		SetVisualState(bHovered, bDropTarget);
	}
}

void UZItemSlotWidget::Refresh()
{
	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;

	if (NameText)
	{
		// an empty equipment slot says which slot it is
		const bool bEmptyEquipmentSlot = !Definition && Role == EZItemSlotRole::Equipment;
		const FText SlotName = EmptyText.IsEmpty() && EquipmentSlot.IsValid() ? EquipmentSlot.GetDisplayName() : EmptyText;
		NameText->SetText(Definition ? Definition->GetItemName() : bEmptyEquipmentSlot ? SlotName : FText::GetEmpty());
		NameText->SetColorAndOpacity(bEmptyEquipmentSlot ? FSlateColor(EmptyTextColor) : NameTextColor);
	}

	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Count));
		CountText->SetVisibility(Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IconImage)
	{
		const bool bHasIcon = Definition && !Definition->Icon.IsNull();
		if (bHasIcon)
		{
			// loads the icon in the background if it isn't loaded yet
			IconImage->SetBrushFromSoftTexture(Definition->Icon);
		}

		IconImage->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (DurabilityBar)
	{
		const bool bHasDurability = Item && Item->HasDurability();
		DurabilityBar->SetVisibility(bHasDurability ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DurabilityBar->SetPercent(bHasDurability ? Item->GetDurabilityPercent() : 0.f);
	}

	if (EquippedMarker)
	{
		// in the equipment window everything is equipped, so only the weapon in the hands is marked
		const AZSurvivalGameCharacter* Character = Cast<AZSurvivalGameCharacter>(GetOwningPlayerPawn());
		const UZEquipmentComponent* Equipment = UZEquipmentComponent::FindEquipmentComponent(GetOwningPlayer());
		const bool bInHands = Item && Character && Character->GetEquippedItem() == Item;
		const bool bEquipped = Item && Equipment && Equipment->IsEquipped(Item) && Role != EZItemSlotRole::Equipment;
		EquippedMarker->SetVisibility(bInHands || bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (UTextBlock* MarkerText = Cast<UTextBlock>(EquippedMarker))
		{
			MarkerText->SetText(bInHands ? LOCTEXT("InHands", "In hands") : LOCTEXT("Equipped", "Equipped"));
		}
	}

	UpdateBackground();
	OnItemChanged(Item, Count);
}

FReply UZItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!Item || !Screen.IsValid())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (Role == EZItemSlotRole::Combine)
		{
			Screen->RemoveFromCombine(Item);
		}
		else if (Role == EZItemSlotRole::Equipment)
		{
			Screen->UnequipItem(Item);
		}
		else
		{
			Screen->UseItem(Item);
		}

		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (Role != EZItemSlotRole::Combine)
		{
			Screen->SelectItem(Item);
		}

		// a drag starts if the mouse moves while the button is held; otherwise the button comes up here (a click)
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UZItemSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// a click on a combine slot (no drag) takes the item out again
	if (Role == EZItemSlotRole::Combine && Item && Screen.IsValid() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Screen->RemoveFromCombine(Item);
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UZItemSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Role != EZItemSlotRole::Combine && Item && Screen.IsValid() && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (Role == EZItemSlotRole::Equipment)
		{
			Screen->UnequipItem(Item);
		}
		else
		{
			Screen->UseItem(Item);
		}

		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UZItemSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	SetVisualState(true, bDropTarget);
	if (Screen.IsValid())
	{
		Screen->SetHoveredItem(Item);
	}
}

void UZItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	SetVisualState(false, bDropTarget);
	if (Screen.IsValid())
	{
		Screen->ClearHoveredItem(Item);
	}
}

void UZItemSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!Item)
	{
		return;
	}

	UZItemDragDropOperation* Operation = NewObject<UZItemDragDropOperation>(GetTransientPackage());
	Operation->Item = Item;
	Operation->Count = Count;
	Operation->bFromCombineSlot = Role == EZItemSlotRole::Combine;
	Operation->bFromEquipmentSlot = Role == EZItemSlotRole::Equipment;
	Operation->Pivot = EDragPivot::CenterCenter;

	// a copy of this slot follows the mouse
	if (UZItemSlotWidget* DragVisual = CreateWidget<UZItemSlotWidget>(GetOwningPlayer(), GetClass()))
	{
		DragVisual->SetItem(Item, Count);
		DragVisual->SetRenderOpacity(0.8f);
		Operation->DefaultDragVisual = DragVisual;
	}

	OutOperation = Operation;
}

void UZItemSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	// another item over this one: dropping it here combines them (or puts it in the combine slots, or equips it in this slot)
	SetVisualState(bHovered, IsDropTargetFor(Cast<UZItemDragDropOperation>(InOperation)));
}

void UZItemSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);

	SetVisualState(bHovered, false);
}

bool UZItemSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetVisualState(bHovered, false);

	const UZItemDragDropOperation* ItemOperation = Cast<UZItemDragDropOperation>(InOperation);
	if (!ItemOperation || !ItemOperation->Item || !Screen.IsValid())
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	if (ItemOperation->bFromCombineSlot)
	{
		// taken back out of the combine slots (dropped on another combine slot: it stays)
		if (Role != EZItemSlotRole::Combine)
		{
			Screen->RemoveFromCombine(ItemOperation->Item);
		}
	}
	else if (Role == EZItemSlotRole::Combine)
	{
		Screen->AddToCombine(ItemOperation->Item);
	}
	else if (Role == EZItemSlotRole::Equipment)
	{
		// into this slot if it goes there (what's here comes out, or swaps places); otherwise tried together with what's here, e.g. a mod onto the weapon
		if (Screen->CanEquip(ItemOperation->Item, EquipmentSlot))
		{
			Screen->EquipItem(ItemOperation->Item, EquipmentSlot);
		}
		else if (Item && ItemOperation->Item != Item && !ItemOperation->bFromEquipmentSlot)
		{
			Screen->CombineItems(ItemOperation->Item, Item);
		}
	}
	else if (ItemOperation->bFromEquipmentSlot)
	{
		// out of the equipment window onto the items: taken off
		Screen->UnequipItem(ItemOperation->Item);
	}
	else if (ItemOperation->Item != Item)
	{
		// one item onto another: try them together
		Screen->CombineItems(ItemOperation->Item, Item);
	}

	return true;
}

bool UZItemSlotWidget::IsDropTargetFor(const UZItemDragDropOperation* Operation) const
{
	if (!Operation || !Operation->Item || Operation->bFromCombineSlot || !Screen.IsValid())
	{
		return false;
	}

	if (Role == EZItemSlotRole::Equipment)
	{
		return Screen->CanEquip(Operation->Item, EquipmentSlot) || (Item && Operation->Item != Item && !Operation->bFromEquipmentSlot);
	}

	return Item && Operation->Item != Item && !Operation->bFromEquipmentSlot;
}

FReply UZItemSlotWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Item && Screen.IsValid())
	{
		if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			if (Role == EZItemSlotRole::Combine)
			{
				Screen->RemoveFromCombine(Item);
			}
			else
			{
				Screen->SelectItem(Item);
			}

			return FReply::Handled();
		}

		if (Role != EZItemSlotRole::Combine && Key == EKeys::Gamepad_FaceButton_Left)
		{
			if (Role == EZItemSlotRole::Equipment)
			{
				Screen->UnequipItem(Item);
			}
			else
			{
				Screen->UseItem(Item);
			}

			return FReply::Handled();
		}

		if (Role != EZItemSlotRole::Combine && Key == EKeys::Gamepad_FaceButton_Top)
		{
			Screen->AddToCombine(Item);
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UZItemSlotWidget::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	// focused with a gamepad or the keyboard works like hovering with the mouse
	SetVisualState(true, bDropTarget);
	if (Screen.IsValid())
	{
		Screen->SetHoveredItem(Item);
	}
}

void UZItemSlotWidget::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	SetVisualState(false, bDropTarget);
	if (Screen.IsValid())
	{
		Screen->ClearHoveredItem(Item);
	}
}

void UZItemSlotWidget::SetVisualState(bool bInHovered, bool bInDropTarget)
{
	bHovered = bInHovered;
	bDropTarget = bInDropTarget;
	UpdateBackground();
	OnStateChanged(bSelected, bHovered, bDropTarget);
}

void UZItemSlotWidget::UpdateBackground()
{
	if (Background)
	{
		Background->SetBrushColor(bDropTarget ? DropTargetColor : bSelected ? SelectedColor : bHovered ? HoveredColor : NormalColor);
	}
}

#undef LOCTEXT_NAMESPACE
