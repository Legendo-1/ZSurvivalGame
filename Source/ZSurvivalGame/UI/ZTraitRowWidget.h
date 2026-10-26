// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZTraitRowWidget.generated.h"

class UButton;
class UTextBlock;
class UZProgressionComponent;
class UZTraitDefinition;

/**
 *  One trait in the character panel: its name, rank, what it does now and at the next rank, and a button to raise it
 *  Your own look: a Widget Blueprint with this parent class; widgets with these names are filled in (all optional): NameText, RankText, EffectText, RaiseButton
 */
UCLASS()
class UZTraitRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Shows this trait of this player (the character panel calls it) */
	void SetTrait(UZProgressionComponent* InProgression, UZTraitDefinition* InTrait);

	UFUNCTION(BlueprintPure, Category = "Progression")
	UZTraitDefinition* GetTrait() const { return Trait; }

	/** Updates the rank, the effect and whether it can be raised */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	virtual void Refresh();

	/** Spends points on one more rank (what the button does) */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void RaiseTrait();

protected:

	virtual void NativeOnInitialized() override;

	/** Builds the built-in look, used when the Widget Blueprint has no widgets of its own */
	virtual void BuildDefaultLayout();

	UFUNCTION()
	void HandleRaiseClicked();

	/** A Blueprint can add to the look here */
	UFUNCTION(BlueprintImplementableEvent, Category = "Progression")
	void OnTraitChanged(UZTraitDefinition* NewTrait, int32 Rank, bool bCanRaise);

	/** The trait's name (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** "3 / 10" (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RankText;

	/** What it does now, and at the next rank (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EffectText;

	/** Raises the trait; disabled without enough points or at the highest rank (optional) */
	UPROPERTY(BlueprintReadOnly, Category = "Progression", meta = (BindWidgetOptional))
	TObjectPtr<UButton> RaiseButton;

private:

	TWeakObjectPtr<UZProgressionComponent> Progression;

	UPROPERTY(Transient)
	TObjectPtr<UZTraitDefinition> Trait;
};
