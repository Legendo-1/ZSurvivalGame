// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZSurvivalHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;

/**
 *  Player HUD showing health and stamina bars, and the ammo while a weapon is in the hands
 *  The Widget Blueprint must contain Progress Bars named HealthBar and StaminaBar;
 *  a Text named AmmoText and a widget named Crosshair are optional
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

	/** "Ammo / Magazine size" while a weapon is in the hands (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* AmmoText;

	/** Shown while a weapon is in the hands (optional, e.g. an Image in the middle of the screen) */
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* Crosshair;

	/** Flashes when a shot dealt damage, a bit longer for a kill (optional, e.g. an Image over the crosshair) */
	UPROPERTY(meta = (BindWidgetOptional))
	UWidget* HitMarker;

	/** "You died. Respawning..." while dead (optional) */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	/** Updates the bars from the owning player's character every frame */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
