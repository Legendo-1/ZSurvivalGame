// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZEquipmentPanelWidget.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "Items/ZEquipmentSettings.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"
#include "UI/ZInventoryScreenWidget.h"
#include "UI/ZItemSlotWidget.h"
#include "UI/ZWidgetLayout.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

void UZEquipmentPanelWidget::NativeOnInitialized()
{
	if (!ItemSlotClass)
	{
		ItemSlotClass = UZItemSlotWidget::StaticClass();
	}

	// no widgets from a Widget Blueprint: build the built-in look (it makes the slots); otherwise use the slots placed in it
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	else if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			UZItemSlotWidget* SlotWidget = Cast<UZItemSlotWidget>(Widget);
			if (SlotWidget && SlotWidget->GetEquipmentSlot().IsValid())
			{
				SlotWidgets.Add(SlotWidget);
			}
		});
	}

	Super::NativeOnInitialized();
}

void UZEquipmentPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToPlayer();
	Refresh();
}

void UZEquipmentPanelWidget::NativeDestruct()
{
	UnbindFromPlayer();

	Super::NativeDestruct();
}

void UZEquipmentPanelWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	UScrollBox* Scroll = MakeScrollBox(*WidgetTree, TEXT("Scroll"));
	WidgetTree->RootWidget = Scroll;

	UVerticalBox* Column = MakeVerticalBox(*WidgetTree, TEXT("Column"));
	Scroll->AddChild(Column);

	UTextBlock* Title = MakeText(*WidgetTree, TEXT("EquipmentTitle"), 13, MutedTextColor, true);
	Title->SetText(LOCTEXT("EquipmentTitle", "EQUIPMENT"));
	AddToVerticalBox(Column, Title, FMargin(0.f, 0.f, 0.f, 4.f));

	ArmorText = MakeText(*WidgetTree, TEXT("ArmorText"), 11, TextColor, false, true);
	AddToVerticalBox(Column, ArmorText, FMargin(0.f, 0.f, 0.f, 8.f));

	// the slots of each group together, in the order of the settings
	TArray<FString> Groups;
	TMap<FString, UWrapBox*> GroupPanels;
	for (const FZEquipmentSlotType& SlotType : UZEquipmentSettings::Get()->SlotTypes)
	{
		if (!SlotType.Type.IsValid())
		{
			continue;
		}

		const FString GroupName = SlotType.Group.ToString();
		UWrapBox*& GroupPanel = GroupPanels.FindOrAdd(GroupName);
		if (!GroupPanel)
		{
			const int32 GroupIndex = Groups.Add(GroupName);
			if (!SlotType.Group.IsEmpty())
			{
				UTextBlock* GroupTitle = MakeText(*WidgetTree, *FString::Printf(TEXT("GroupTitle%d"), GroupIndex), 11, MutedTextColor, true);
				GroupTitle->SetText(SlotType.Group);
				AddToVerticalBox(Column, GroupTitle, FMargin(0.f, 4.f, 0.f, 4.f));
			}

			GroupPanel = MakeWrapBox(*WidgetTree, *FString::Printf(TEXT("GroupSlots%d"), GroupIndex), 6.f);
			AddToVerticalBox(Column, GroupPanel, FMargin(0.f, 0.f, 0.f, 6.f));
		}

		for (int32 Index = 0; Index < FMath::Max(SlotType.Count, 1); ++Index)
		{
			if (UZItemSlotWidget* SlotWidget = CreateWidget<UZItemSlotWidget>(this, ItemSlotClass))
			{
				SlotWidget->SetEquipmentSlot(FZEquipmentSlot(SlotType.Type, Index));
				AddToPanel(GroupPanel, SlotWidget);
				SlotWidgets.Add(SlotWidget);
			}
		}
	}

	UTextBlock* Hint = MakeText(*WidgetTree, TEXT("EquipmentHint"), 10, MutedTextColor, false, true);
	Hint->SetText(LOCTEXT("EquipmentHint", "Drag items onto a slot to equip them. Right-click to take one off. Keys 1, 2 and 3 take out your weapons."));
	AddToVerticalBox(Column, Hint, FMargin(0.f, 6.f, 0.f, 0.f));
}

void UZEquipmentPanelWidget::SetScreen(UZInventoryScreenWidget* InScreen)
{
	Screen = InScreen;
	for (UZItemSlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->SetScreen(InScreen, EZItemSlotRole::Equipment);
	}
}

void UZEquipmentPanelWidget::BindToPlayer()
{
	// on the PlayerState, so they're the same after dying and respawning
	APlayerController* PlayerController = GetOwningPlayer();
	UZEquipmentComponent* NewEquipment = UZEquipmentComponent::FindEquipmentComponent(PlayerController);
	UAbilitySystemComponent* NewAbilitySystem = PlayerController ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->PlayerState) : nullptr;
	if (NewEquipment == Equipment.Get() && NewAbilitySystem == AbilitySystem.Get())
	{
		return;
	}

	UnbindFromPlayer();
	Equipment = NewEquipment;
	AbilitySystem = NewAbilitySystem;

	if (NewEquipment)
	{
		NewEquipment->OnEquipmentChanged.AddUniqueDynamic(this, &UZEquipmentPanelWidget::HandleEquipmentChanged);
	}

	if (NewAbilitySystem)
	{
		ArmorChangedHandle = NewAbilitySystem->GetGameplayAttributeValueChangeDelegate(UZAttributeSet::GetArmorAttribute()).AddUObject(this, &UZEquipmentPanelWidget::HandleArmorChanged);
	}
}

void UZEquipmentPanelWidget::UnbindFromPlayer()
{
	if (UZEquipmentComponent* OldEquipment = Equipment.Get())
	{
		OldEquipment->OnEquipmentChanged.RemoveDynamic(this, &UZEquipmentPanelWidget::HandleEquipmentChanged);
	}

	if (UAbilitySystemComponent* OldAbilitySystem = AbilitySystem.Get())
	{
		OldAbilitySystem->GetGameplayAttributeValueChangeDelegate(UZAttributeSet::GetArmorAttribute()).Remove(ArmorChangedHandle);
	}

	ArmorChangedHandle.Reset();
	Equipment.Reset();
	AbilitySystem.Reset();
}

void UZEquipmentPanelWidget::Refresh()
{
	BindToPlayer();

	const UZEquipmentComponent* PlayerEquipment = Equipment.Get();
	const UZItemInstance* Selected = Screen.IsValid() ? Screen->GetSelectedItem() : nullptr;
	for (UZItemSlotWidget* SlotWidget : SlotWidgets)
	{
		UZItemInstance* Item = PlayerEquipment ? PlayerEquipment->GetItemInSlot(SlotWidget->GetEquipmentSlot()) : nullptr;
		SlotWidget->SetItem(Item, Item ? 1 : 0);
		SlotWidget->SetSelected(Item && Item == Selected);
	}

	RefreshArmor();
}

void UZEquipmentPanelWidget::SetSelectedItem(const UZItemInstance* Item)
{
	for (UZItemSlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->SetSelected(Item && SlotWidget->GetItem() == Item);
	}
}

UZItemSlotWidget* UZEquipmentPanelWidget::FindSlotWidget(const FZEquipmentSlot& EquipmentSlot) const
{
	const TObjectPtr<UZItemSlotWidget>* Found = SlotWidgets.FindByPredicate([&EquipmentSlot](const UZItemSlotWidget* SlotWidget) { return SlotWidget->GetEquipmentSlot() == EquipmentSlot; });
	return Found ? Found->Get() : nullptr;
}

void UZEquipmentPanelWidget::HandleEquipmentChanged(UZEquipmentComponent* ChangedEquipment)
{
	Refresh();
}

void UZEquipmentPanelWidget::HandleArmorChanged(const FOnAttributeChangeData& Data)
{
	RefreshArmor();
}

void UZEquipmentPanelWidget::RefreshArmor()
{
	if (!ArmorText)
	{
		return;
	}

	// the owner's copy of their armor (it comes with the other stats)
	const float Armor = AbilitySystem.IsValid() ? AbilitySystem->GetNumericAttribute(UZAttributeSet::GetArmorAttribute()) : 0.f;
	const int32 DamageTakenPercent = FMath::RoundToInt32(UZAttributeSet::GetDamageAfterArmor(100.f, Armor));

	ArmorText->SetText(FText::Format(LOCTEXT("ArmorLine", "Armor {0}: you take {1}% damage"), FMath::RoundToInt32(Armor), DamageTakenPercent));
}

FString UZEquipmentPanelWidget::GetDebugString() const
{
	TArray<FString> Slots;
	for (const UZItemSlotWidget* SlotWidget : SlotWidgets)
	{
		const UZItemInstance* Item = SlotWidget->GetItem();
		Slots.Add(FString::Printf(TEXT("%s: %s"), *SlotWidget->GetEquipmentSlot().GetDisplayName().ToString(), Item ? *GetNameSafe(Item->GetItemDefinition()) : TEXT("(empty)")));
	}

	return FString::Printf(TEXT("%s; %s"), ArmorText ? *ArmorText->GetText().ToString() : TEXT("-"), *FString::Join(Slots, TEXT(", ")));
}

#undef LOCTEXT_NAMESPACE
