// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZTradeskillRowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Crafting/ZCraftingComponent.h"
#include "UI/ZWidgetLayout.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

void UZTradeskillRowWidget::NativeOnInitialized()
{
	// no widgets from a Widget Blueprint: build the built-in look
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	Super::NativeOnInitialized();
}

void UZTradeskillRowWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	UVerticalBox* Column = MakeVerticalBox(*WidgetTree, TEXT("Column"));
	WidgetTree->RootWidget = Column;

	UHorizontalBox* TitleRow = MakeHorizontalBox(*WidgetTree, TEXT("TitleRow"));
	AddToVerticalBox(Column, TitleRow);

	NameText = MakeText(*WidgetTree, TEXT("NameText"), 11, TextColor);
	AddToHorizontalBox(TitleRow, NameText, FMargin(0.f), 1.f);

	LevelText = MakeText(*WidgetTree, TEXT("LevelText"), 11, AccentColor, true);
	AddToHorizontalBox(TitleRow, LevelText);

	LevelProgressBar = MakeProgressBar(*WidgetTree, TEXT("LevelProgressBar"), AccentColor);
	USizeBox* BarSize = MakeSizeBox(*WidgetTree, TEXT("BarSize"), 0.f, 4.f);
	BarSize->SetContent(LevelProgressBar);
	AddToVerticalBox(Column, BarSize, FMargin(0.f, 2.f, 0.f, 0.f));
}

void UZTradeskillRowWidget::SetSkill(UZCraftingComponent* InCrafting, FGameplayTag InSkill)
{
	Crafting = InCrafting;
	Skill = InSkill;
	Refresh();
}

void UZTradeskillRowWidget::Refresh()
{
	const UZCraftingComponent* PlayerCrafting = Crafting.Get();
	const int32 Level = PlayerCrafting ? PlayerCrafting->GetSkillLevel(Skill) : 0;

	if (NameText)
	{
		NameText->SetText(UZCraftingComponent::GetSkillName(Skill));
	}

	if (LevelText)
	{
		LevelText->SetText(FText::AsNumber(Level));
	}

	if (LevelProgressBar)
	{
		LevelProgressBar->SetPercent(PlayerCrafting ? PlayerCrafting->GetSkillLevelProgress(Skill) : 0.f);
	}

	OnSkillChanged(Skill, Level);
}

#undef LOCTEXT_NAMESPACE
