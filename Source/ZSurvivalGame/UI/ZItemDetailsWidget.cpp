// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZItemDetailsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"
#include "UI/ZWidgetLayout.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

void UZItemDetailsWidget::NativeOnInitialized()
{
	// no widgets from a Widget Blueprint: build the built-in look
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (EmptyText.IsEmpty())
	{
		EmptyText = LOCTEXT("NoItem", "Point at an item to see what it is.");
	}

	Super::NativeOnInitialized();

	Refresh();
}

void UZItemDetailsWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	UVerticalBox* Column = MakeVerticalBox(*WidgetTree, TEXT("Column"));
	WidgetTree->RootWidget = Column;

	// icon on the left of the name and type
	UHorizontalBox* Header = MakeHorizontalBox(*WidgetTree, TEXT("Header"));
	AddToVerticalBox(Column, Header, FMargin(0.f, 0.f, 0.f, 6.f));

	IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IconImage"));
	USizeBox* IconSize = MakeSizeBox(*WidgetTree, TEXT("IconSize"), 56.f, 56.f);
	IconSize->SetContent(IconImage);
	AddToHorizontalBox(Header, IconSize, FMargin(0.f, 0.f, 8.f, 0.f));

	UVerticalBox* Titles = MakeVerticalBox(*WidgetTree, TEXT("Titles"));
	AddToHorizontalBox(Header, Titles, FMargin(0.f), 1.f);

	NameText = MakeText(*WidgetTree, TEXT("NameText"), 16, AccentColor, true, true);
	AddToVerticalBox(Titles, NameText);

	TypeText = MakeText(*WidgetTree, TEXT("TypeText"), 11, MutedTextColor);
	AddToVerticalBox(Titles, TypeText);

	DescriptionText = MakeText(*WidgetTree, TEXT("DescriptionText"), 11, TextColor, false, true);
	AddToVerticalBox(Column, DescriptionText, FMargin(0.f, 0.f, 0.f, 6.f));

	StatsText = MakeText(*WidgetTree, TEXT("StatsText"), 11, GoodColor, false, true);
	AddToVerticalBox(Column, StatsText);
}

void UZItemDetailsWidget::SetItem(UZItemInstance* InItem, int32 InCount)
{
	Item = InItem;
	Count = InCount;
	Refresh();
}

void UZItemDetailsWidget::Refresh()
{
	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	const ESlateVisibility ItemVisibility = Definition ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (NameText)
	{
		NameText->SetText(!Definition ? EmptyText
			: Count > 1 ? FText::Format(LOCTEXT("NameWithCount", "{0} x{1}"), Definition->GetItemName(), Count)
			: Definition->GetItemName());
	}

	if (TypeText)
	{
		TypeText->SetVisibility(ItemVisibility);
		TypeText->SetText(Definition ? Definition->GetTypeName() : FText::GetEmpty());
	}

	if (IconImage)
	{
		const bool bHasIcon = Definition && !Definition->Icon.IsNull();
		if (bHasIcon)
		{
			IconImage->SetBrushFromSoftTexture(Definition->Icon);
		}

		IconImage->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (DescriptionText)
	{
		DescriptionText->SetVisibility(Definition && !Definition->Description.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		DescriptionText->SetText(Definition ? Definition->Description : FText::GetEmpty());
	}

	if (StatsText)
	{
		const FText Stats = GetStatsText();
		StatsText->SetVisibility(Stats.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		StatsText->SetText(Stats);
	}

	OnItemChanged(Item, Count);
}

FText UZItemDetailsWidget::GetStatsText() const
{
	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	return Definition ? FText::Join(FText::FromString(TEXT("\n")), Definition->GetDescriptionLines(Item)) : FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
