// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "SaveGame/ZSaveTypes.h"
#include "ZPlayerSaveComponent.generated.h"

class APlayerController;
class UNetDriver;
class AZSurvivalGamePlayerState;
class UZCraftingComponent;
class UZCraftingRecipe;
class UZEquipmentComponent;
class UZInventoryComponent;
class UZProgressionComponent;

/**
 *  Moves a player's character between the save on their own PC and the game (Remnant style, see SAVING.md)
 *  Joining: the player's machine sends the character of its slot in use; the server checks it against the game's rules and puts it on the player
 *  (inventory, level, traits, tradeskills, recipes) before their character spawns. The host's own player takes it straight from this PC.
 *  Saving: a few seconds after something changes, the server builds the character from the game and sends it back to the player's machine,
 *  which writes it (the host also writes the world). The player can't change anything this way, only keep what the server says they have.
 *  On the PlayerController, so it exists on the server and the player's own machine only
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZPlayerSaveComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZPlayerSaveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Owning player: sends the character to the server once the game is set up */
	virtual void BeginPlay() override;

	/** Server: saves what changed when the game ends or the host leaves */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** --- JOINING (server) --- */

	/** True once the player's character is in the game (loaded, new, or given up on) */
	bool IsCharacterReady() const { return bCharacterReady; }

	/** The host's own player: the character from this PC's save, right away */
	void LoadLocalCharacter();

	/** A joining player: waits for their character. If it doesn't arrive in time, they play a new one that isn't saved */
	void WaitForCharacter();

	/** The character is in the game, e.g. so the game mode can spawn them */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCharacterReady, UZPlayerSaveComponent*);
	FOnCharacterReady OnCharacterReady;

	/** --- SAVING --- */

	/**
	 *  Saves now. On the server (the host): every player and the world; bWaitUntilWritten writes the host's save before returning.
	 *  A joined player asks the server for theirs, or with bWaitUntilWritten (e.g. switching slots) saves it right here
	 */
	void RequestSave(bool bWaitUntilWritten = false);

	/** Server: saves every player in this world (and the host's world) */
	static void SaveAllPlayers(const UWorld* World, bool bWaitUntilWritten);

	/** Server: builds this player's character from the game and writes it (the host's own) or sends it to their machine */
	void SaveCharacter(bool bWaitUntilWritten = false);

	/** Server: something about the character changed; it's saved a little later (Project Settings → Saving → Save Delay), together with whatever else changes until then */
	void MarkDirty();

	/** Server: the character as the game has it now */
	FZCharacterSaveData BuildCharacterData() const;

protected:

	/** Owning player → server: the character from the slot in use */
	UFUNCTION(Server, Reliable)
	void ServerReceiveCharacter(const FZCharacterSaveData& Character);

	/** Server → owning player: write this into your slot in use */
	UFUNCTION(Client, Reliable)
	void ClientWriteCharacter(const FZCharacterSaveData& Character);

	/** Owning player → server: save my character now */
	UFUNCTION(Server, Reliable)
	void ServerRequestSave();

	UFUNCTION()
	void HandleInventoryChanged(UZInventoryComponent* Inventory);

	UFUNCTION()
	void HandleProgressionChanged(UZProgressionComponent* Progression);

	UFUNCTION()
	void HandleTradeskillsChanged(UZCraftingComponent* Crafting);

	UFUNCTION()
	void HandleRecipeDiscovered(UZCraftingComponent* Crafting, UZCraftingRecipe* Recipe);

	UFUNCTION()
	void HandleEquipmentChanged(UZEquipmentComponent* Equipment);

private:

	/** Server: puts a saved character on the player, checking every value against the game's rules */
	void ApplyCharacter(const FZCharacterSaveData& Character);

	/** Server: the character is in the game; starts listening for changes to save */
	void FinishLoading();

	/** Server: the joining player's character didn't arrive in time */
	void HandleWaitTimedOut();

	/** The world is about to be torn down (quitting, or another level): the last save, while the PlayerState and character still exist */
	void HandleWorldTearDown(UWorld* World);

	/** Owning player: the connection to the host failed or was closed (e.g. the host ended the game): the last save, before the host's actors are removed here */
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/**
	 *  Owning player who joined someone: saves their character on this machine from what the host sent it, for when they leave
	 *  (the host can't send it any more then). Only a character the host loaded from this save, once all of it has arrived
	 */
	void SaveOnOwner();

	/** True if this asset id is something the game has (or a redirect leads to) */
	static bool IsKnownAsset(const FPrimaryAssetId& AssetId);

	FDelegateHandle WorldTearDownHandle;
	FDelegateHandle NetworkFailureHandle;

	/** Owning player: the character sent to the host, and whether it was saved here on leaving */
	FGuid SentCharacterId;
	bool bSavedOnLeaving = false;

	void BindToChanges();
	void UnbindFromChanges();

	APlayerController* GetPlayerController() const;
	AZSurvivalGamePlayerState* GetZPlayerState() const;

	/** Server: the character is in the game */
	bool bCharacterReady = false;

	/** Server: its progress is saved. False for a character that came too late: the new one played instead mustn't overwrite the player's save */
	bool bSaveProgress = false;

	/** Server: changed since it was last saved */
	bool bDirty = false;

	/** Owning player: the character was sent */
	bool bCharacterSent = false;

	/** Server: which character this is, and its name (kept as they came) */
	FGuid CharacterId;
	FString CharacterName;

	/** Server: things in the save this game doesn't know (e.g. an item that was renamed or removed), written back unchanged so they come back if it returns */
	TArray<FZSavedItem> UnknownItems;
	TArray<FPrimaryAssetId> UnknownRecipes;

	/** Server: when this player last asked to save (asking again right away is ignored) */
	double LastRequestedSaveTime = -1000.0;

	FTimerHandle SaveTimer;
	FTimerHandle WaitTimer;

	/** Server: the PlayerState whose changes are being listened to */
	TWeakObjectPtr<AZSurvivalGamePlayerState> BoundPlayerState;
};
