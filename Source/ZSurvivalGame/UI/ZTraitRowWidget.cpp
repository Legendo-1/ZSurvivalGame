// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZTraitRowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Progression/ZProgressionComponent.h"
#include "Progression/ZTraitDefinition.h"
#include "UI/ZWidgetLayout.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

void UZTraitRowWidget::NativeOnInitialized()
{
	// no widgets from a Widget Blueprint: build the built-in look
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	Super::NativeOnInitialized();

	if (RaiseButton)
	{
		RaiseButton->OnClicked.AddUniqueDynamic(this, &UZTraitRowWidget::HandleRaiseClicked);
	}
}

void UZTraitRowWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	UHorizontalBox* Row = MakeHorizontalBox(*WidgetTree, TEXT("Row"));
	WidgetTree->RootWidget = Row;

	UVerticalBox* Texts = MakeVerticalBox(*WidgetTree, TEXT("Texts"));
	AddToHorizontalBox(Row, Texts, FMargin(0.f, 0.f, 6.f, 0.f), 1.f);

	UHorizontalBox* TitleRow = MakeHorizontalBox(*WidgetTree, TEXT("TitleRow"));
	AddToVerticalBox(Texts, TitleRow);

	NameText = MakeText(*WidgetTree, TEXT("NameText"), 12, TextColor, true);
	AddToHorizontalBox(TitleRow, NameText, FMargin(0.f, 0.f, 8.f, 0.f), 1.f);

	RankText = MakeText(*WidgetTree, TEXT("RankText"), 11, AccentColor, true);
	AddToHorizontalBox(TitleRow, RankText);

	EffectText = MakeText(*WidgetTree, TEXT("EffectText"), 10, MutedTextColor, false, true);
	AddToVerticalBox(Texts, EffectText);

	RaiseButton = MakeButton(*WidgetTree, TEXT("RaiseButton"), LOCTEXT("RaiseLabel", "+"), 14);
	USizeBox* ButtonSize = MakeSizeBox(*WidgetTree, TEXT("RaiseSize"), 34.f, 30.f);
	ButtonSize->SetContent(RaiseButton);
	AddToHorizontalBox(Row, ButtonSize);
}

void UZTraitRowWidget::SetTrait(UZProgressionComponent* InProgression, UZTraitDefinition* InTrait)
{
	Progression = InProgression;
	Trait = InTrait;
	SetToolTipText(Trait ? Trait->Description : FText::GetEmpty());
	Refresh();
}

void UZTraitRowWidget::Refresh()
{
	if (!Trait)
	{
		return;
	}

	const UZProgressionComponent* PlayerProgression = Progression.Get();
	const int32 Rank = PlayerProgression ? PlayerProgression->GetTraitRank(Trait) : 0;
	const bool bCanRaise = PlayerProgression && PlayerProgression->CanRaiseTrait(Trait);

	if (NameText)
	{
		NameText->SetText(Trait->GetTraitName());
	}

	if (RankText)
	{
		RankText->SetText(Trait->PointsPerRank > 1
			? FText::Format(LOCTEXT("RankWithCost", "{0} / {1}  ({2} points each)"), Rank, Trait->MaxRank, Trait->PointsPerRank)
			: FText::Format(LOCTEXT("Rank", "{0} / {1}"), Rank, Trait->MaxRank));
	}

	if (EffectText)
	{
		// what it does now, and what the next rank would do (one line each, the modifiers side by side)
		const auto OneLine = [](const FText& Text) { return FText::FromString(Text.ToString().Replace(TEXT("\n"), TEXT(", "))); };
		const FText Now = Rank > 0 ? OneLine(Trait->GetEffectText(Rank)) : LOCTEXT("NotLearned", "Not learned yet");
		EffectText->SetText(Rank < Trait->MaxRank
			? FText::Format(LOCTEXT("EffectWithNext", "{0}\nNext: {1}"), Now, OneLine(Trait->GetEffectText(Rank + 1)))
			: Now);
	}

	if (RaiseButton)
	{
		RaiseButton->SetIsEnabled(bCanRaise);
	}

	OnTraitChanged(Trait, Rank, bCanRaise);
}

void UZTraitRowWidget::RaiseTrait()
{
	// a request to the server; the panel refreshes when the new rank arrives
	if (UZProgressionComponent* PlayerProgression = Progression.Get())
	{
		if (Trait && PlayerProgression->CanRaiseTrait(Trait))
		{
			PlayerProgression->RaiseTrait(Trait);
		}
	}
}

void UZTraitRowWidget::HandleRaiseClicked()
{
	RaiseTrait();
}

#undef LOCTEXT_NAMESPACE
