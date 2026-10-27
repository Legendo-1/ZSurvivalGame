// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ZSaveSettings.generated.h"

class UZSaveStorage;

/**
 *  Project-wide save settings
 *  Project Settings → Game → Saving (saved in Config/DefaultGame.ini)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Saving"))
class UZSaveSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UZSaveSettings* Get() { return GetDefault<UZSaveSettings>(); }

	/** Where saves are kept. Empty: files on this PC (Saved/SaveGames). Another class can keep them elsewhere later, e.g. on a server */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	TSoftClassPtr<UZSaveStorage> StorageClass;

	/** Seconds from a change (an item picked up, experience, a trait) until the character is saved; more changes in that time are saved together */
	UPROPERTY(Config, EditAnywhere, Category = "Saving", meta = (ClampMin = "1", Units = "s"))
	float SaveDelay = 10.f;

	/** Name of new characters */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	FString DefaultCharacterName = TEXT("Survivor");

	/**
	 *  Seconds the host waits for a joining player's character before giving up.
	 *  Then they play a new character for this game, which isn't saved (their own save stays as it was)
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Multiplayer", meta = (ClampMin = "1", Units = "s"))
	float CharacterArrivalTimeout = 15.f;

	/** Play in the editor with the saved character. Off: every Play starts with a new character, and nothing is saved */
	UPROPERTY(Config, EditAnywhere, Category = "Editor")
	bool bUseSavesInEditor = true;
};
