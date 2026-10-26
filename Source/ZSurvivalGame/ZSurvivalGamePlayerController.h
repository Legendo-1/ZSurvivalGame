// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Crafting/ZCraftingComponent.h"
#include "Progression/ZProgressionComponent.h"
#include "ZSurvivalGamePlayerController.generated.h"

class AZCraftingStation;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
class UZInventoryScreenWidget;
class UZSurvivalHUDWidget;
struct FKey;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings, the HUD, the inventory screen and short messages to the player (e.g. what crafting made, level-ups)
 */
UCLASS(abstract)
class AZSurvivalGamePlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** HUD widget (health/stamina bars) to spawn */
	UPROPERTY(EditAnywhere, Category="UI")
	TSubclassOf<UZSurvivalHUDWidget> HUDWidgetClass;

	/** Pointer to the HUD widget */
	UPROPERTY()
	TObjectPtr<UZSurvivalHUDWidget> HUDWidget;

	/** Opens and closes the inventory screen (IA_Inventory: I, Tab or the gamepad's View button by default) */
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InventoryAction;

	/** The inventory screen: your own Widget Blueprint with ZInventoryScreenWidget as its parent class, or the built-in one */
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UZInventoryScreenWidget> InventoryScreenClass;

	/** Above the HUD (0) */
	UPROPERTY(EditAnywhere, Category = "UI")
	int32 InventoryZOrder = 10;

	/** The inventory screen, made the first time it's opened */
	UPROPERTY()
	TObjectPtr<UZInventoryScreenWidget> InventoryScreen;

	/** Seconds a message (e.g. "Crafted Bandage x1.") stays on screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "0", Units = "s"))
	float MessageDuration = 6.f;

	/** Most messages shown at once; older ones go first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (ClampMin = "1"))
	int32 MaxMessages = 4;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

public:

	/** Constructor */
	AZSurvivalGamePlayerController();

	/** Server → this player: they used a crafting station, so the inventory screen opens to combine items there */
	UFUNCTION(Client, Reliable)
	void ClientOpenCraftingStation(AZCraftingStation* Station);

	/** --- INVENTORY SCREEN (local player only) --- */

	/** Shows the inventory screen; while it's open the game gets no input (the character stands still) and the mouse cursor shows */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void OpenInventory();

	/** Hides the inventory screen and gives the controls back to the game */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseInventory();

	/** What the inventory key does */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleInventory();

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsInventoryOpen() const;

	/** The inventory screen, or null if it was never opened */
	UFUNCTION(BlueprintPure, Category = "UI")
	UZInventoryScreenWidget* GetInventoryScreen() const { return InventoryScreen; }

	/** True if the key is mapped to the inventory action (the screen closes on it, since the game gets no input while it's open) */
	bool IsInventoryKey(const FKey& Key) const;

	/** The keyboard key mapped to the inventory action, e.g. "I"; empty if there's none */
	FText GetInventoryKeyName() const;

	/** Shows a short message to this player, e.g. "Crafted Bandage x1." */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ShowMessage(const FText& Message);

	/** Server → this player: shows a message */
	UFUNCTION(Client, Reliable)
	void ClientShowMessage(const FText& Message);

	/** Messages from the last few seconds, oldest first */
	UFUNCTION(BlueprintPure, Category = "UI")
	TArray<FText> GetRecentMessages() const;

	/** Server → this player: runs a console command on their machine, for multiplayer test runs (see ZOnClients; does nothing in shipping builds) */
	UFUNCTION(Client, Reliable)
	void ClientRunConsoleCommand(const FString& Command);

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Local player attached: listens to the player's crafting (the host's own player) */
	virtual void ReceivedPlayer() override;

	/** Client: the PlayerState arrived, listen to the player's crafting */
	virtual void OnRep_PlayerState() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Listens to this player's crafting and progression for messages (local players only) */
	void BindToPlayerState();

	UFUNCTION()
	void HandleCraftingResult(UZCraftingComponent* Crafting, EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message);

	UFUNCTION()
	void HandleLevelUp(UZProgressionComponent* Progression, int32 NewLevel);

	UFUNCTION()
	void HandleBonusTraitPoints(UZProgressionComponent* Progression, int32 Points);

private:

	struct FPlayerMessage
	{
		FText Text;
		double Time = 0.0;
	};

	TArray<FPlayerMessage> Messages;

	TWeakObjectPtr<UZCraftingComponent> BoundCrafting;
	TWeakObjectPtr<UZProgressionComponent> BoundProgression;
};
