// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveGame/ZSaveGame.h"
#include "ZSurvivalGame.h"
#include "Progression/ZProgressionSettings.h"

bool UZSaveGame::UpgradeToLatest()
{
	if (SaveVersion > static_cast<int32>(EZSaveVersion::Latest))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: this save is from a newer version of the game (save version %d, this game reads up to %d)"),
			SaveVersion, static_cast<int32>(EZSaveVersion::Latest));
		return false;
	}

	// one step per version, oldest first, e.g.:
	// if (SaveVersion < static_cast<int32>(EZSaveVersion::SplitAmmoStacks)) { ...convert...; }

	SaveVersion = static_cast<int32>(EZSaveVersion::Latest);
	return true;
}

FZSaveSlotInfo UZSaveGame::MakeSlotInfo(const FString& SlotName) const
{
	FZSaveSlotInfo Info;
	Info.SlotName = SlotName;
	Info.CharacterName = Character.CharacterName;
	Info.Level = UZProgressionSettings::Get()->GetCharacterLevelForExperience(Character.Experience);
	Info.PlayTime = FTimespan::FromSeconds(PlayTimeSeconds);
	Info.LastSaved = SavedAt + (FDateTime::Now() - FDateTime::UtcNow());
	Info.MapName = World.MapName;
	return Info;
}
