// Copyright Epic Games, Inc. All Rights Reserved.


#include "ZSurvivalGamePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "UI/ZSurvivalHUDWidget.h"
#include "ZSurvivalGame.h"
#include "ZCheatManager.h"
#include "Crafting/ZCraftingStation.h"
#include "Widgets/Input/SVirtualJoystick.h"

#define LOCTEXT_NAMESPACE "ZSurvivalPlayerController"

AZSurvivalGamePlayerController::AZSurvivalGamePlayerController()
{
	// test commands like ZSpawnZombies (see UZCheatManager)
	CheatClass = UZCheatManager::StaticClass();
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
	}
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
	UZCraftingComponent* Crafting = IsLocalPlayerController() ? UZCraftingComponent::FindCraftingComponent(this) : nullptr;
	if (Crafting == BoundCrafting.Get())
	{
		return;
	}

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

void AZSurvivalGamePlayerController::HandleCraftingResult(UZCraftingComponent* Crafting, EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message)
{
	ShowMessage(Message);
}

#undef LOCTEXT_NAMESPACE
