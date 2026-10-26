// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZSurvivalHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;

/**
 *  Player HUD showing health and stamina bars, the ammo while a weapon is in the hands, and what the interact key would do
 *  The Widget Blueprint must contain Progress Bars named HealthBar and StaminaBar; the other widgets are optional (see MULTIPLAYER.md)
 */
UCLASS(abstract)
class UZSurvivalHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Health bar, bound by name from the Widget Blueprint */
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;

	/** Stamina bar, bound by name from the Widget Blueprint */
	UPROPERTY(meta = (BindWidget))
	UProgressBar* StaminaBar;

	/** "Magazine / reserve ammo" while a weapon is in the hands (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* AmmoText;

	/** "[E] Pick up Rifle Ammo x15" while looking at something to interact with (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* InteractionText;

	/** Fills while holding the interact key on an interaction that takes time (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* InteractionProgress;

	/** Shown while a weapon is in the hands (optional, e.g. an Image in the middle of the screen) */
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* Crosshair;

	/** Flashes when a shot dealt damage, a bit longer for a kill (optional, e.g. an Image over the crosshair) */
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* HitMarker;

	/** "You died. Respawning..." while dead (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	/** The last few messages to the player, one per line, e.g. "Crafted Bandage x1." (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* MessageText;

	/** Adds a MessageText near the bottom of the screen if the Widget Blueprint has none, so messages always show */
	virtual void NativeOnInitialized() override;

	/** Updates the bars from the owning player's character every frame */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Name of the key mapped to Interact, e.g. "E" */
	FText GetInteractKeyName() const;
};
