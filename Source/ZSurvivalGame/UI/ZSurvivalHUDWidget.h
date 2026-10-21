// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ZSurvivalHUDWidget.generated.h"

class UProgressBar;

/**
 *  Player HUD showing health and stamina bars
 *  The Widget Blueprint must contain Progress Bars named HealthBar and StaminaBar
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

	/** Updates the bars from the owning player's character every frame */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
