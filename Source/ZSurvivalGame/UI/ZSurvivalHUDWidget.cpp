// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZSurvivalHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ZSurvivalGameCharacter.h"
#include "Weapons/ZWeapon.h"

void UZSurvivalHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// read the stats from whatever character the player currently controls
	const AZSurvivalGameCharacter* SurvivalCharacter = Cast<AZSurvivalGameCharacter>(GetOwningPlayerPawn());
	if (!SurvivalCharacter)
	{
		return;
	}

	HealthBar->SetPercent(SurvivalCharacter->GetHealthPercent());
	StaminaBar->SetPercent(SurvivalCharacter->GetStaminaPercent());

	// ammo and crosshair only while a weapon is in the hands
	const AZWeapon* Weapon = SurvivalCharacter->GetEquippedWeapon();
	const ESlateVisibility WeaponVisibility = Weapon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (AmmoText)
	{
		AmmoText->SetVisibility(WeaponVisibility);
		if (Weapon)
		{
			AmmoText->SetText(FText::Format(NSLOCTEXT("ZSurvivalHUD", "AmmoFormat", "{0} / {1}"), Weapon->GetAmmo(), Weapon->GetMagazineSize()));
		}
	}

	if (Crosshair)
	{
		Crosshair->SetVisibility(WeaponVisibility);
	}

	if (HitMarker)
	{
		const float ShowTime = SurvivalCharacter->WasLastHitAKill() ? 0.4f : 0.15f;
		HitMarker->SetVisibility(SurvivalCharacter->GetTimeSinceLastHit() < ShowTime ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (StatusText)
	{
		const bool bDead = SurvivalCharacter->IsDead();
		StatusText->SetVisibility(bDead ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bDead)
		{
			StatusText->SetText(NSLOCTEXT("ZSurvivalHUD", "Respawning", "You died. Respawning..."));
		}
	}
}
