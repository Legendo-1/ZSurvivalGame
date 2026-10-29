// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/ZSurvivalHUDWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "ZSurvivalGameCharacter.h"
#include "ZSurvivalGamePlayerController.h"
#include "Combat/ZLockOnComponent.h"
#include "Combat/ZLockOnTargetComponent.h"
#include "Interaction/ZInteractionComponent.h"
#include "Weapons/ZWeapon.h"

void UZSurvivalHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Canvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Canvas)
	{
		return;
	}

	// no LockOnMarker in the Widget Blueprint: a small white dot with a dark outline, moved onto the target every frame
	if (!LockOnMarker)
	{
		constexpr float MarkerSize = 12.f;
		UImage* Dot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("LockOnMarker"));
		Dot->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, MarkerSize * 0.5f, FLinearColor(0.f, 0.f, 0.f, 0.85f), 2.f, FVector2f(MarkerSize, MarkerSize)));
		Dot->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* MarkerSlot = Canvas->AddChildToCanvas(Dot))
		{
			MarkerSlot->SetAnchors(FAnchors(0.f, 0.f));
			MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MarkerSlot->SetSize(FVector2D(MarkerSize, MarkerSize));
		}

		LockOnMarker = Dot;
	}

	// no MessageText in the Widget Blueprint: put one above the bottom middle of the screen
	if (MessageText)
	{
		return;
	}

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	FSlateFontInfo Font = MessageText->GetFont();
	Font.Size = 15;
	MessageText->SetFont(Font);
	MessageText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.72f)));
	MessageText->SetShadowOffset(FVector2D(1.f, 1.f));
	MessageText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetVisibility(ESlateVisibility::Collapsed);

	if (UCanvasPanelSlot* MessageSlot = Canvas->AddChildToCanvas(MessageText))
	{
		MessageSlot->SetAnchors(FAnchors(0.5f, 0.8f));
		MessageSlot->SetAlignment(FVector2D(0.5f, 1.f));
		MessageSlot->SetAutoSize(true);
	}
}

void UZSurvivalHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// messages also show while there's no character (e.g. respawning)
	if (MessageText)
	{
		const AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOwningPlayer());
		const TArray<FText> Messages = PlayerController ? PlayerController->GetRecentMessages() : TArray<FText>();
		MessageText->SetVisibility(Messages.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		MessageText->SetText(FText::Join(FText::FromString(TEXT("\n")), Messages));
	}

	// read the stats from whatever character the player currently controls
	const AZSurvivalGameCharacter* SurvivalCharacter = Cast<AZSurvivalGameCharacter>(GetOwningPlayerPawn());
	if (!SurvivalCharacter)
	{
		return;
	}

	HealthBar->SetPercent(SurvivalCharacter->GetHealthPercent());
	StaminaBar->SetPercent(SurvivalCharacter->GetStaminaPercent());

	// ammo and crosshair only while a gun is in the hands (not for a melee weapon)
	const AZWeapon* Weapon = SurvivalCharacter->GetEquippedWeapon();
	const bool bHoldingGun = Weapon && !Weapon->IsMelee();
	const ESlateVisibility WeaponVisibility = bHoldingGun ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (AmmoText)
	{
		AmmoText->SetVisibility(WeaponVisibility);
		if (bHoldingGun)
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

	UpdateLockOnMarker(SurvivalCharacter);
}

void UZSurvivalHUDWidget::UpdateLockOnMarker(const AZSurvivalGameCharacter* SurvivalCharacter)
{
	if (!LockOnMarker)
	{
		return;
	}

	// where the target's lock-on point is on screen (in the widget's units, so it fits any resolution and DPI scale)
	const UZLockOnComponent* LockOn = SurvivalCharacter ? SurvivalCharacter->GetLockOnComponent() : nullptr;
	const UZLockOnTargetComponent* Target = LockOn ? LockOn->GetTarget() : nullptr;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	const bool bShow = Target && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(GetOwningPlayer(), Target->GetComponentLocation(), ScreenPosition, false);

	LockOnMarker->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(LockOnMarker->Slot))
		{
			MarkerSlot->SetPosition(ScreenPosition);
		}
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
