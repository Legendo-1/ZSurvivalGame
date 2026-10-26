// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZCharacterPanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Crafting/ZCraftingComponent.h"
#include "Progression/ZProgressionComponent.h"
#include "Progression/ZProgressionSettings.h"
#include "Progression/ZTraitDefinition.h"
#include "UI/ZTradeskillRowWidget.h"
#include "UI/ZTraitRowWidget.h"
#include "UI/ZWidgetLayout.h"

#define LOCTEXT_NAMESPACE "ZInventoryUI"

void UZCharacterPanelWidget::NativeOnInitialized()
{
	// no widgets from a Widget Blueprint: build the built-in look
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	if (!TraitRowClass)
	{
		TraitRowClass = UZTraitRowWidget::StaticClass();
	}

	if (!TradeskillRowClass)
	{
		TradeskillRowClass = UZTradeskillRowWidget::StaticClass();
	}

	Super::NativeOnInitialized();

	if (ResetTraitsButton)
	{
		ResetTraitsButton->OnClicked.AddUniqueDynamic(this, &UZCharacterPanelWidget::HandleResetClicked);
	}
}

void UZCharacterPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindToPlayer();
	BuildRows();
	Refresh();
}

void UZCharacterPanelWidget::NativeDestruct()
{
	UnbindFromPlayer();

	Super::NativeDestruct();
}

void UZCharacterPanelWidget::BuildDefaultLayout()
{
	using namespace ZWidgetLayout;

	UScrollBox* Scroll = MakeScrollBox(*WidgetTree, TEXT("Scroll"));
	WidgetTree->RootWidget = Scroll;

	UVerticalBox* Column = MakeVerticalBox(*WidgetTree, TEXT("Column"));
	Scroll->AddChild(Column);

	UTextBlock* Title = MakeText(*WidgetTree, TEXT("CharacterTitle"), 13, MutedTextColor, true);
	Title->SetText(LOCTEXT("CharacterTitle", "CHARACTER"));
	AddToVerticalBox(Column, Title, FMargin(0.f, 0.f, 0.f, 4.f));

	LevelText = MakeText(*WidgetTree, TEXT("LevelText"), 18, AccentColor, true);
	AddToVerticalBox(Column, LevelText);

	ExperienceBar = MakeProgressBar(*WidgetTree, TEXT("ExperienceBar"), AccentColor);
	USizeBox* BarSize = MakeSizeBox(*WidgetTree, TEXT("ExperienceBarSize"), 0.f, 6.f);
	BarSize->SetContent(ExperienceBar);
	AddToVerticalBox(Column, BarSize, FMargin(0.f, 4.f));

	ExperienceText = MakeText(*WidgetTree, TEXT("ExperienceText"), 10, MutedTextColor);
	AddToVerticalBox(Column, ExperienceText, FMargin(0.f, 0.f, 0.f, 8.f));

	UHorizontalBox* PointsRow = MakeHorizontalBox(*WidgetTree, TEXT("PointsRow"));
	AddToVerticalBox(Column, PointsRow, FMargin(0.f, 0.f, 0.f, 6.f));

	TraitPointsText = MakeText(*WidgetTree, TEXT("TraitPointsText"), 12, TextColor, true);
	AddToHorizontalBox(PointsRow, TraitPointsText, FMargin(0.f), 1.f);

	ResetTraitsButton = MakeButton(*WidgetTree, TEXT("ResetTraitsButton"), LOCTEXT("ResetTraits", "Reset traits"), 10);
	AddToHorizontalBox(PointsRow, ResetTraitsButton);

	UVerticalBox* Traits = MakeVerticalBox(*WidgetTree, TEXT("TraitsPanel"));
	TraitsPanel = Traits;
	AddToVerticalBox(Column, Traits, FMargin(0.f, 0.f, 0.f, 12.f));

	UTextBlock* SkillsTitle = MakeText(*WidgetTree, TEXT("TradeskillsTitle"), 13, MutedTextColor, true);
	SkillsTitle->SetText(LOCTEXT("TradeskillsTitle", "TRADESKILLS"));
	AddToVerticalBox(Column, SkillsTitle, FMargin(0.f, 0.f, 0.f, 4.f));

	UVerticalBox* Skills = MakeVerticalBox(*WidgetTree, TEXT("TradeskillsPanel"));
	TradeskillsPanel = Skills;
	AddToVerticalBox(Column, Skills, FMargin(0.f, 0.f, 0.f, 6.f));

	DiscoveriesText = MakeText(*WidgetTree, TEXT("DiscoveriesText"), 10, MutedTextColor);
	AddToVerticalBox(Column, DiscoveriesText);
}

void UZCharacterPanelWidget::BindToPlayer()
{
	// on the PlayerState, so they're the same after the character dies and respawns
	UZProgressionComponent* NewProgression = UZProgressionComponent::FindProgressionComponent(GetOwningPlayer());
	UZCraftingComponent* NewCrafting = UZCraftingComponent::FindCraftingComponent(GetOwningPlayer());
	if (NewProgression == Progression.Get() && NewCrafting == Crafting.Get())
	{
		return;
	}

	UnbindFromPlayer();
	Progression = NewProgression;
	Crafting = NewCrafting;

	if (NewProgression)
	{
		NewProgression->OnProgressionChanged.AddUniqueDynamic(this, &UZCharacterPanelWidget::HandleProgressionChanged);
	}

	if (NewCrafting)
	{
		NewCrafting->OnTradeskillsChanged.AddUniqueDynamic(this, &UZCharacterPanelWidget::HandleTradeskillsChanged);
		NewCrafting->OnRecipeDiscovered.AddUniqueDynamic(this, &UZCharacterPanelWidget::HandleRecipeDiscovered);
	}

	// rows of an earlier player state point to the old components
	TraitRows.Reset();
	TradeskillRows.Reset();
	if (TraitsPanel)
	{
		TraitsPanel->ClearChildren();
	}

	if (TradeskillsPanel)
	{
		TradeskillsPanel->ClearChildren();
	}
}

void UZCharacterPanelWidget::UnbindFromPlayer()
{
	if (UZProgressionComponent* OldProgression = Progression.Get())
	{
		OldProgression->OnProgressionChanged.RemoveDynamic(this, &UZCharacterPanelWidget::HandleProgressionChanged);
	}

	if (UZCraftingComponent* OldCrafting = Crafting.Get())
	{
		OldCrafting->OnTradeskillsChanged.RemoveDynamic(this, &UZCharacterPanelWidget::HandleTradeskillsChanged);
		OldCrafting->OnRecipeDiscovered.RemoveDynamic(this, &UZCharacterPanelWidget::HandleRecipeDiscovered);
	}

	Progression.Reset();
	Crafting.Reset();
}

void UZCharacterPanelWidget::BuildRows()
{
	UZProgressionComponent* PlayerProgression = Progression.Get();
	if (TraitsPanel && PlayerProgression && TraitRows.IsEmpty())
	{
		for (UZTraitDefinition* Trait : PlayerProgression->GetAllTraits())
		{
			if (UZTraitRowWidget* Row = CreateWidget<UZTraitRowWidget>(this, TraitRowClass))
			{
				Row->SetTrait(PlayerProgression, Trait);
				ZWidgetLayout::AddToPanel(TraitsPanel, Row, FMargin(0.f, 0.f, 0.f, 6.f));
				TraitRows.Add(Row);
			}
		}
	}

	UZCraftingComponent* PlayerCrafting = Crafting.Get();
	if (TradeskillsPanel && PlayerCrafting && TradeskillRows.IsEmpty())
	{
		for (const FGameplayTag& Skill : UZCraftingComponent::GetAllTradeskills())
		{
			if (UZTradeskillRowWidget* Row = CreateWidget<UZTradeskillRowWidget>(this, TradeskillRowClass))
			{
				Row->SetSkill(PlayerCrafting, Skill);
				ZWidgetLayout::AddToPanel(TradeskillsPanel, Row, FMargin(0.f, 0.f, 0.f, 5.f));
				TradeskillRows.Add(Row);
			}
		}
	}
}

void UZCharacterPanelWidget::Refresh()
{
	const UZProgressionComponent* PlayerProgression = Progression.Get();
	if (PlayerProgression)
	{
		const UZProgressionSettings* Settings = UZProgressionSettings::Get();
		const int32 Level = PlayerProgression->GetLevel();
		const bool bMaxLevel = Level >= Settings->MaxCharacterLevel;

		if (LevelText)
		{
			LevelText->SetText(FText::Format(LOCTEXT("Level", "Level {0}"), Level));
		}

		if (ExperienceBar)
		{
			ExperienceBar->SetPercent(PlayerProgression->GetLevelProgress());
		}

		if (ExperienceText)
		{
			ExperienceText->SetText(bMaxLevel
				? FText::Format(LOCTEXT("ExperienceMax", "{0} experience (highest level)"), PlayerProgression->GetExperience())
				: FText::Format(LOCTEXT("Experience", "{0} / {1} experience"), PlayerProgression->GetExperience(), Settings->GetCharacterExperienceForLevel(Level + 1)));
		}

		if (TraitPointsText)
		{
			const int32 Unspent = PlayerProgression->GetUnspentTraitPoints();
			TraitPointsText->SetText(FText::Format(LOCTEXT("TraitPoints", "{0} trait {0}|plural(one=point,other=points) to spend"), Unspent));
			ZWidgetLayout::SetTextColor(TraitPointsText, Unspent > 0 ? ZWidgetLayout::AccentColor : ZWidgetLayout::MutedTextColor);
		}

		if (ResetTraitsButton)
		{
			ResetTraitsButton->SetVisibility(Settings->bAllowTraitReset ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			ResetTraitsButton->SetIsEnabled(PlayerProgression->CanResetTraits());
		}
	}

	for (UZTraitRowWidget* Row : TraitRows)
	{
		Row->Refresh();
	}

	for (UZTradeskillRowWidget* Row : TradeskillRows)
	{
		Row->Refresh();
	}

	// only how many: the recipes themselves stay secret
	if (DiscoveriesText)
	{
		const UZCraftingComponent* PlayerCrafting = Crafting.Get();
		DiscoveriesText->SetText(FText::Format(LOCTEXT("Discoveries", "Recipes found: {0}"), PlayerCrafting ? PlayerCrafting->GetNumDiscovered() : 0));
	}
}

void UZCharacterPanelWidget::HandleProgressionChanged(UZProgressionComponent* ChangedProgression)
{
	// the trait list may have arrived after the panel was built
	BuildRows();
	Refresh();
}

void UZCharacterPanelWidget::HandleTradeskillsChanged(UZCraftingComponent* ChangedCrafting)
{
	Refresh();
}

void UZCharacterPanelWidget::HandleRecipeDiscovered(UZCraftingComponent* ChangedCrafting, UZCraftingRecipe* Recipe)
{
	Refresh();
}

void UZCharacterPanelWidget::HandleResetClicked()
{
	ResetTraits();
}

void UZCharacterPanelWidget::ResetTraits()
{
	// a request to the server; the panel refreshes when the traits arrive
	if (UZProgressionComponent* PlayerProgression = Progression.Get(); PlayerProgression && PlayerProgression->CanResetTraits())
	{
		PlayerProgression->ResetTraits();
	}
}

UZTraitRowWidget* UZCharacterPanelWidget::FindTraitRow(const UZTraitDefinition* Trait) const
{
	const TObjectPtr<UZTraitRowWidget>* Row = TraitRows.FindByPredicate([Trait](const UZTraitRowWidget* Candidate) { return Candidate && Candidate->GetTrait() == Trait; });
	return Row ? Row->Get() : nullptr;
}

FString UZCharacterPanelWidget::GetDebugString() const
{
	const auto TextOf = [](const UTextBlock* Text) { return Text ? Text->GetText().ToString() : FString(TEXT("-")); };

	TArray<FString> Traits;
	for (const UZTraitRowWidget* Row : TraitRows)
	{
		const UZProgressionComponent* PlayerProgression = Progression.Get();
		const UZTraitDefinition* Trait = Row->GetTrait();
		Traits.Add(FString::Printf(TEXT("%s %d%s"), *GetNameSafe(Trait), PlayerProgression ? PlayerProgression->GetTraitRank(Trait) : 0,
			PlayerProgression && PlayerProgression->CanRaiseTrait(Trait) ? TEXT(" (+)") : TEXT("")));
	}

	TArray<FString> Skills;
	const UZCraftingComponent* PlayerCrafting = Crafting.Get();
	for (const UZTradeskillRowWidget* Row : TradeskillRows)
	{
		Skills.Add(FString::Printf(TEXT("%s %d"), *UZCraftingComponent::GetSkillName(Row->GetSkill()).ToString(), PlayerCrafting ? PlayerCrafting->GetSkillLevel(Row->GetSkill()) : 0));
	}

	return FString::Printf(TEXT("%s; %s; %s; reset %s; traits: %s; tradeskills: %s; %s"), *TextOf(LevelText), *TextOf(ExperienceText), *TextOf(TraitPointsText),
		ResetTraitsButton && ResetTraitsButton->GetIsEnabled() ? TEXT("on") : TEXT("off"), *FString::Join(Traits, TEXT(", ")), *FString::Join(Skills, TEXT(", ")), *TextOf(DiscoveriesText));
}

#undef LOCTEXT_NAMESPACE
