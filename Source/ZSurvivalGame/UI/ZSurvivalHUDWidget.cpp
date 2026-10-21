// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZSurvivalHUDWidget.h"
#include "Components/ProgressBar.h"
#include "ZSurvivalGameCharacter.h"

void UZSurvivalHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// read the stats from whatever character the player currently controls
	if (const AZSurvivalGameCharacter* SurvivalCharacter = Cast<AZSurvivalGameCharacter>(GetOwningPlayerPawn()))
	{
		HealthBar->SetPercent(SurvivalCharacter->GetHealthPercent());
		StaminaBar->SetPercent(SurvivalCharacter->GetStaminaPercent());
	}
}
