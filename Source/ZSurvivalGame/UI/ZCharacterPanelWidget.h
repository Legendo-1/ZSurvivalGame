// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZCharacterPanelWidget.generated.h"

class UButton;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UZCraftingComponent;
class UZCraftingRecipe;
class UZProgressionComponent;
class UZTradeskillRowWidget;
class UZTraitDefinition;
class UZTraitRowWidget;

/**
 *  The player's character: level and experience, trait points and the traits to spend them on, tradeskills and how many recipes they've found
 *  Part of the inventory screen. Updates itself when any of it changes
 *  Your own look: a Widget Blueprint with this parent class; widgets with these names are filled in (all optional):
 *  LevelText, ExperienceBar, ExperienceText, TraitPointsText, TraitsPanel (a panel the trait rows go in), ResetTraitsButton, TradeskillsPanel, DiscoveriesText
 */
UCLASS()
class UZCharacterPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Updates everything from the player's progression and crafting */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	virtual void Refresh();

	/** Takes back every spent trait point, to spend them again (what the Reset button does) */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void ResetTraits();

	/** The row showing this trait, or null */
	UFUNCTION(BlueprintPure, Category = "Progression")
	UZTraitRowWidget* FindTraitRow(const UZTraitDefinition* Trait) const;

	/** "Level 3; 450 / 800 experience; 2 trait points to spend; Vigor 1 / 10 (+), ..." for logs and tests */
	FString GetDebugString() const;

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	UFUNCTION()
	void HandleProgressionChanged(UZProgressionComponent* ChangedProgression);

	UFUNCTION()
	void HandleTradeskillsChanged(UZCraftingComponent* ChangedCrafting);

	UFUNCTION()
	void HandleRecipeDiscovered(UZCraftingComponent* ChangedCrafting, UZCraftingRecipe* Recipe);

	UFUNCTION()
	void HandleResetClicked();

	/** One row per trait (your own Widget Blueprint with UZTraitRowWidget as its parent class, or the built-in look) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	TSubclassOf<UZTraitRowWidget> TraitRowClass;

	/** One row per tradeskill */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	TSubclassOf<UZTradeskillRowWidget> TradeskillRowClass;

	/** "Level 4" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	/** Experience toward the next level (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ExperienceBar;

	/** "450 / 800 experience" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExperienceText;

	/** "2 trait points to spend" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TraitPointsText;

	/** The trait rows go in here, e.g. a Vertical Box or Scroll Box (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TraitsPanel;

	/** Takes back every spent trait point; hidden if resetting isn't allowed (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetTraitsButton;

	/** The tradeskill rows go in here (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TradeskillsPanel;

	/** "Recipes found: 6" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DiscoveriesText;

private:

	/** Listens to the owning player's progression and crafting */
	void BindToPlayer();

	void UnbindFromPlayer();

	/** Makes a row for each trait and tradeskill (once) */
	void BuildRows();

	TWeakObjectPtr<UZProgressionComponent> Progression;
	TWeakObjectPtr<UZCraftingComponent> Crafting;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZTraitRowWidget>> TraitRows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZTradeskillRowWidget>> TradeskillRows;
};
