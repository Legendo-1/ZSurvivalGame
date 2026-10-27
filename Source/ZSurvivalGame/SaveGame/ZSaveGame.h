// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveGame/ZSaveTypes.h"
#include "ZSaveGame.generated.h"

/**
 *  One save slot: a character, and the world it hosts (Remnant style: a player who joins someone takes the character along, their world stays as it was)
 *  Written and read by UZSaveSubsystem; the gameplay side is UZPlayerSaveComponent
 */
UCLASS()
class UZSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	/** The data's version (EZSaveVersion), to convert saves made by older versions of the game */
	UPROPERTY()
	int32 SaveVersion = static_cast<int32>(EZSaveVersion::Latest);

	/** When the slot was made and last saved (UTC) */
	UPROPERTY()
	FDateTime CreatedAt;

	UPROPERTY()
	FDateTime SavedAt;

	/** Time played with this slot */
	UPROPERTY()
	double PlayTimeSeconds = 0.0;

	UPROPERTY()
	FZCharacterSaveData Character;

	UPROPERTY()
	FZWorldSaveData World;

	/** Converts data from an older version of the game. False if it's from a newer version, which this one can't read safely */
	bool UpgradeToLatest();

	/** What a menu shows about this slot */
	FZSaveSlotInfo MakeSlotInfo(const FString& SlotName) const;
};
