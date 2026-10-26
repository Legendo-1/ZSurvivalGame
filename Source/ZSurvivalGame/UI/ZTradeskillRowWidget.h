// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ZTradeskillRowWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UZCraftingComponent;

/**
 *  One tradeskill in the character panel: its name, level and how far it is to the next level
 *  Your own look: a Widget Blueprint with this parent class; widgets with these names are filled in (all optional): NameText, LevelText, LevelProgressBar
 */
UCLASS()
class UZTradeskillRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Shows this tradeskill of this player (the character panel calls it) */
	void SetSkill(UZCraftingComponent* InCrafting, FGameplayTag InSkill);

	UFUNCTION(BlueprintPure, Category = "Crafting")
	FGameplayTag GetSkill() const { return Skill; }

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	virtual void Refresh();

protected:

	virtual void NativeOnInitialized() override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	/** A Blueprint can add to the look here */
	UFUNCTION(BlueprintImplementableEvent, Category = "Crafting")
	void OnSkillChanged(FGameplayTag ChangedSkill, int32 Level);

	/** "Electronics" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** The level (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	/** Experience toward the next level (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> LevelProgressBar;

private:

	TWeakObjectPtr<UZCraftingComponent> Crafting;

	FGameplayTag Skill;
};
