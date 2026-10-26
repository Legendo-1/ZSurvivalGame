// Copyright Epic Games, Inc. All Rights Reserved.


#include "ZSurvivalGamePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/ZInventoryScreenWidget.h"
#include "UI/ZSurvivalHUDWidget.h"
#include "ZSurvivalGame.h"
#include "ZCheatManager.h"
#include "Crafting/ZCraftingStation.h"
#include "Progression/ZProgressionSettings.h"
#include "Widgets/Input/SVirtualJoystick.h"

#define LOCTEXT_NAMESPACE "ZSurvivalPlayerController"

AZSurvivalGamePlayerController::AZSurvivalGamePlayerController()
{
	// test commands like ZSpawnZombies (see UZCheatManager)
	CheatClass = UZCheatManager::StaticClass();

	// the built-in look until a Widget Blueprint is set
	InventoryScreenClass = UZInventoryScreenWidget::StaticClass();
}

void AZSurvivalGamePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogZSurvivalGame, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	// only spawn the HUD on local player controllers
	if (IsLocalPlayerController() && HUDWidgetClass)
	{
		// spawn the HUD widget and add it to the player screen
		HUDWidget = CreateWidget<UZSurvivalHUDWidget>(this, HUDWidgetClass);

		if (HUDWidget)
		{
			HUDWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogZSurvivalGame, Error, TEXT("Could not spawn HUD widget."));
		}
	}
}

void AZSurvivalGamePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		// on the controller, not the character: the inventory opens while dead or respawning too
		UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
		if (EnhancedInput && InventoryAction)
		{
			EnhancedInput->BindAction(InventoryAction, ETriggerEvent::Started, this, &AZSurvivalGamePlayerController::ToggleInventory);
		}
	}
}

void AZSurvivalGamePlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	BindToPlayerState();
}

void AZSurvivalGamePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	BindToPlayerState();
}

bool AZSurvivalGamePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AZSurvivalGamePlayerController::ClientOpenCraftingStation_Implementation(AZCraftingStation* Station)
{
	// stations only need to be nearby: combining items in the inventory uses them
	if (Station)
	{
		ShowMessage(FText::Format(LOCTEXT("AtStation", "At the {0}: combine items here to use it."), Station->GetStationName()));
		OpenInventory();
	}
}

void AZSurvivalGamePlayerController::OpenInventory()
{
	if (!IsLocalPlayerController() || IsInventoryOpen())
	{
		return;
	}

	if (!InventoryScreen)
	{
		InventoryScreen = CreateWidget<UZInventoryScreenWidget>(this, InventoryScreenClass ? InventoryScreenClass.Get() : UZInventoryScreenWidget::StaticClass());
		if (!InventoryScreen)
		{
			UE_LOG(LogZSurvivalGame, Error, TEXT("Could not create the inventory screen (Inventory Screen Class '%s')."), *GetNameSafe(InventoryScreenClass));
			return;
		}
	}

	// the game gets no input while it's open (no walking or shooting by accident), and keys held down count as let go
	InventoryScreen->AddToPlayerScreen(InventoryZOrder);
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, InventoryScreen, EMouseLockMode::DoNotLock, true);
	SetShowMouseCursor(true);

	// a gamepad needs something focused to move around with; the mouse doesn't
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().IsGamepadAttached())
	{
		InventoryScreen->FocusFirstItem();
	}
}

void AZSurvivalGamePlayerController::CloseInventory()
{
	if (!IsInventoryOpen())
	{
		return;
	}

	InventoryScreen->RemoveFromParent();
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(this, true);
	SetShowMouseCursor(false);
}

void AZSurvivalGamePlayerController::ToggleInventory()
{
	if (IsInventoryOpen())
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

bool AZSurvivalGamePlayerController::IsInventoryOpen() const
{
	return InventoryScreen && InventoryScreen->IsInViewport();
}

bool AZSurvivalGamePlayerController::IsInventoryKey(const FKey& Key) const
{
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	return InventoryAction && Subsystem && Subsystem->QueryKeysMappedToAction(InventoryAction).Contains(Key);
}

FText AZSurvivalGamePlayerController::GetInventoryKeyName() const
{
	const UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (InventoryAction && Subsystem)
	{
		for (const FKey& Key : Subsystem->QueryKeysMappedToAction(InventoryAction))
		{
			if (!Key.IsGamepadKey())
			{
				return Key.GetDisplayName(false);
			}
		}
	}

	return FText::GetEmpty();
}

void AZSurvivalGamePlayerController::ShowMessage(const FText& Message)
{
	if (Message.IsEmpty())
	{
		return;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Message to '%s': %s"), *GetNameSafe(this), *Message.ToString());

	Messages.Add({ Message, GetWorld()->GetRealTimeSeconds() });
	if (Messages.Num() > MaxMessages)
	{
		Messages.RemoveAt(0, Messages.Num() - MaxMessages);
	}
}

void AZSurvivalGamePlayerController::ClientShowMessage_Implementation(const FText& Message)
{
	ShowMessage(Message);
}

void AZSurvivalGamePlayerController::ClientRunConsoleCommand_Implementation(const FString& Command)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogZSurvivalGame, Log, TEXT("Running '%s' for the host"), *Command);
	ConsoleCommand(Command);
#endif
}

TArray<FText> AZSurvivalGamePlayerController::GetRecentMessages() const
{
	TArray<FText> Recent;
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	for (const FPlayerMessage& Message : Messages)
	{
		if (Now - Message.Time <= MessageDuration)
		{
			Recent.Add(Message.Text);
		}
	}

	return Recent;
}

void AZSurvivalGamePlayerController::BindToPlayerState()
{
	// only the local player's own messages; the PlayerState can change (e.g. after travelling), so swap over
	UZCraftingComponent* Crafting = IsLocalPlayerController() ? UZCraftingComponent::FindCraftingComponent(this) : nullptr;
	if (Crafting != BoundCrafting.Get())
	{
		if (UZCraftingComponent* OldCrafting = BoundCrafting.Get())
		{
			OldCrafting->OnCraftingResult.RemoveDynamic(this, &AZSurvivalGamePlayerController::HandleCraftingResult);
		}

		BoundCrafting = Crafting;

		if (Crafting)
		{
			Crafting->OnCraftingResult.AddDynamic(this, &AZSurvivalGamePlayerController::HandleCraftingResult);
		}
	}

	UZProgressionComponent* Progression = IsLocalPlayerController() ? UZProgressionComponent::FindProgressionComponent(this) : nullptr;
	if (Progression != BoundProgression.Get())
	{
		if (UZProgressionComponent* OldProgression = BoundProgression.Get())
		{
			OldProgression->OnLevelUp.RemoveDynamic(this, &AZSurvivalGamePlayerController::HandleLevelUp);
			OldProgression->OnBonusTraitPoints.RemoveDynamic(this, &AZSurvivalGamePlayerController::HandleBonusTraitPoints);
		}

		BoundProgression = Progression;

		if (Progression)
		{
			Progression->OnLevelUp.AddDynamic(this, &AZSurvivalGamePlayerController::HandleLevelUp);
			Progression->OnBonusTraitPoints.AddDynamic(this, &AZSurvivalGamePlayerController::HandleBonusTraitPoints);
		}
	}
}

void AZSurvivalGamePlayerController::HandleCraftingResult(UZCraftingComponent* Crafting, EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message)
{
	ShowMessage(Message);
}

void AZSurvivalGamePlayerController::HandleLevelUp(UZProgressionComponent* Progression, int32 NewLevel)
{
	const int32 Points = UZProgressionSettings::Get()->TraitPointsPerLevel;
	ShowMessage(Points > 0
		? FText::Format(LOCTEXT("LevelUpPoints", "Level {0}! +{1} trait {1}|plural(one=point,other=points)."), NewLevel, Points)
		: FText::Format(LOCTEXT("LevelUp", "Level {0}!"), NewLevel));
}

void AZSurvivalGamePlayerController::HandleBonusTraitPoints(UZProgressionComponent* Progression, int32 Points)
{
	ShowMessage(FText::Format(LOCTEXT("BonusTraitPoints", "+{0} trait {0}|plural(one=point,other=points)."), Points));
}

#undef LOCTEXT_NAMESPACE
