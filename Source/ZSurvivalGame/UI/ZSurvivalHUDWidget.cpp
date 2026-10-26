// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZSurvivalHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "ZSurvivalGameCharacter.h"
#include "Interaction/ZInteractionComponent.h"
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
			// magazine / reserve in the inventory (just the magazine if the reserve is unlimited)
			const int32 ReserveAmmo = SurvivalCharacter->GetReserveAmmo();
			AmmoText->SetText(ReserveAmmo >= 0
				? FText::Format(NSLOCTEXT("ZSurvivalHUD", "AmmoFormat", "{0} / {1}"), Weapon->GetAmmo(), ReserveAmmo)
				: FText::AsNumber(Weapon->GetAmmo()));
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

	// what the interact key would do, e.g. "[E] Pick up Rifle Ammo x15"
	const UZInteractionComponent* Interaction = SurvivalCharacter->GetInteractionComponent();
	const bool bHasInteraction = Interaction && Interaction->GetFocusedActor() && !SurvivalCharacter->IsDead();

	if (InteractionText)
	{
		InteractionText->SetVisibility(bHasInteraction ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bHasInteraction)
		{
			InteractionText->SetText(FText::Format(NSLOCTEXT("ZSurvivalHUD", "InteractionFormat", "[{0}] {1}"), GetInteractKeyName(), Interaction->GetFocusedText()));
		}
	}

	if (InteractionProgress)
	{
		const float HoldProgress = Interaction ? Interaction->GetHoldProgress() : 0.f;
		InteractionProgress->SetVisibility(HoldProgress > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		InteractionProgress->SetPercent(HoldProgress);
	}
}

FText UZSurvivalHUDWidget::GetInteractKeyName() const
{
	// the key the player has mapped to Interact, so the prompt stays right if it's changed
	const AZSurvivalGameCharacter* SurvivalCharacter = Cast<AZSurvivalGameCharacter>(GetOwningPlayerPawn());
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetOwningLocalPlayer());
	if (SurvivalCharacter && SurvivalCharacter->GetInteractAction() && InputSubsystem)
	{
		const TArray<FKey> Keys = InputSubsystem->QueryKeysMappedToAction(SurvivalCharacter->GetInteractAction());
		if (Keys.Num() > 0)
		{
			return Keys[0].GetDisplayName(false);
		}
	}

	return NSLOCTEXT("ZSurvivalHUD", "InteractKeyFallback", "Interact");
}
