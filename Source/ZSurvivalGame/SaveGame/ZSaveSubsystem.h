// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveGame/ZSaveTypes.h"
#include "ZSaveSubsystem.generated.h"

class UZPlayerSaveComponent;
class UZSaveGame;
class UZSaveStorage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnGameSaved, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FZOnSaveSlotsChanged);

/**
 *  This PC's save slots, one character each (with the world it hosts), and the one in use. Everything a save menu needs:
 *  list the slots, make a new character, choose or load one, delete one, save now (Blueprints: Get Game Instance Subsystem → ZSaveSubsystem)
 *  It only reads and writes this PC's saves. The character goes to the host and back through UZPlayerSaveComponent; see SAVING.md
 */
UCLASS()
class UZSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** The subsystem of the world's game instance, or null */
	static UZSaveSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** --- FOR MENUS --- */

	/** Every save slot on this PC, the most recently played first */
	UFUNCTION(BlueprintCallable, Category = "Save")
	TArray<FZSaveSlotInfo> GetSaveSlots();

	/** The slot in use */
	UFUNCTION(BlueprintCallable, Category = "Save")
	FZSaveSlotInfo GetActiveSlotInfo();

	/** Makes a slot with a new character and uses it from now on (the next game, hosted or joined). Returns its slot name, empty if it couldn't */
	UFUNCTION(BlueprintCallable, Category = "Save")
	FString CreateSlot(const FString& CharacterName);

	/** Uses this slot's character from the next game on (hosted or joined). The game running now is saved first and keeps going. False if it can't be loaded */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SelectSlot(const FString& SlotName);

	/** Saves the game running now, then continues this slot: opens its level as the host (players in this game are left behind). False if it can't be loaded */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadSlot(const FString& SlotName);

	/** Saves now. The host saves the world and every player; a player in someone else's game gets their character from the host. On Game Saved says when it's written */
	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveNow();

	/** Deletes a slot for good. Not the one in use: select another first. False if it couldn't */
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool DeleteSlot(const FString& SlotName);

	/** False when nothing is saved: Play in the editor with Use Saves In Editor off, or a dedicated server */
	UFUNCTION(BlueprintPure, Category = "Save")
	bool IsSavingEnabled() const;

	/** A save was written (or failed), e.g. to show a "Saved" sign */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FZOnGameSaved OnGameSaved;

	/** A slot was made, deleted or chosen */
	UPROPERTY(BlueprintAssignable, Category = "Save")
	FZOnSaveSlotsChanged OnSaveSlotsChanged;

	/** --- FOR THE GAME --- */

	/** The character of the slot in use (a new slot with a new character if there's none yet) */
	const FZCharacterSaveData& GetActiveCharacter();

	/** The world of the slot in use, for when this PC hosts */
	const FZWorldSaveData& GetActiveWorld();

	/**
	 *  Writes this player's character into the slot in use, and for the host the world too. Refused if it's not the character of the slot
	 *  (e.g. another slot was chosen since). bWaitUntilWritten: write before returning (e.g. when quitting); otherwise it doesn't hold up the game
	 */
	bool WriteActiveSlot(const FZCharacterSaveData& Character, const FZWorldSaveData* World, bool bWaitUntilWritten = false);

	/** Where saves are kept (files on this PC unless Project Settings → Saving says otherwise) */
	UZSaveStorage* GetStorage();

#if !UE_BUILD_SHIPPING
	/** For testing the host's checks: the slot in use, to change by hand, and writing it as it is (the game running now saves nothing more, so it stays like that for the next Play) */
	UZSaveGame* GetActiveSaveForTesting();
	bool WriteActiveSaveForTesting();
#endif

private:

	/** Picks the slot to use if there's none yet: -SaveSlot=Name, else the most recently played, else a new one */
	void EnsureActiveSlot();

	void ActivateSlot(const FString& SlotName, UZSaveGame* Save);

	/** Reads a slot; null if it's missing, damaged or from a newer version of the game */
	UZSaveGame* ReadSlot(const FString& SlotName);

	/** Reads a slot as it is (no conversion from older versions), null if missing or damaged */
	UZSaveGame* ReadSlotData(const FString& SlotName);

	UZSaveGame* MakeNewSave(const FString& CharacterName);

	/** "Slot1", "Slot2", ...: the first free one */
	FString MakeUniqueSlotName();

	/** The host saves everyone in the game running now and waits until it's written, e.g. before another slot is chosen */
	void SaveRunningGame();

	/** Writes the slot in use as it is in memory */
	bool WriteActiveSave(bool bWaitUntilWritten);

	void HandleAsyncWriteDone(bool bSuccess);
	void HandleWriteDone(bool bSuccess);

	/** The local player's save component in the game running now, or null (e.g. in a menu) */
	UZPlayerSaveComponent* FindLocalPlayerSave() const;

	UPROPERTY(Transient)
	TObjectPtr<UZSaveStorage> Storage;

	/** The slot in use, as last read or written */
	UPROPERTY(Transient)
	TObjectPtr<UZSaveGame> ActiveSave;

	/** Its name; empty while saving is off (e.g. an unsaved character for this game only) */
	FString ActiveSlotName;

	/** When play time was last added to the slot */
	double PlayTimeCountedUntil = 0.0;

	/** A write is going on in the background, and another one waits for it (with the latest data) */
	bool bWriteInProgress = false;
	bool bWriteWaiting = false;

	/** The player asked to save (SaveNow), so the next written save says so on screen */
	bool bSaveRequestedByPlayer = false;

#if !UE_BUILD_SHIPPING
	/** A slot written for testing as it is: nothing else is written into it until a slot is chosen again (e.g. the next Play) */
	bool bKeepTestSave = false;
#endif
};
