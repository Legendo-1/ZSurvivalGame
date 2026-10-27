// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "ZSaveTypes.generated.h"

/**
 *  Versions of the save data. A change that old saves need converting for gets a new entry above LatestPlusOne,
 *  and UZSaveGame::UpgradeToLatest converts saves from the version before it
 *  (new fields need no version: old saves just don't have them and get the field's default)
 */
enum class EZSaveVersion : int32
{
	Initial = 1,

	// add new versions above this line
	LatestPlusOne,
	Latest = LatestPlusOne - 1
};

/** One number stored on an item, e.g. Item.Stat.Durability = 12 */
USTRUCT(BlueprintType)
struct FZSavedItemStat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Value = 0;
};

/** One inventory stack */
USTRUCT(BlueprintType)
struct FZSavedItem
{
	GENERATED_BODY()

	/** The item by its asset name (Item:ID_Rifle), so moving the asset to another folder doesn't break saves */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FPrimaryAssetId Item;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Count = 1;

	/** Its stats: magazine ammo, durability, upgrade level, ... */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FZSavedItemStat> Stats;

	/** The mods installed on it */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FPrimaryAssetId> Mods;

	/** It was in the character's hands */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bEquipped = false;
};

/** Ranks put into one trait */
USTRUCT(BlueprintType)
struct FZSavedTraitRank
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FPrimaryAssetId Trait;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Rank = 0;
};

/** Experience in one tradeskill */
USTRUCT(BlueprintType)
struct FZSavedTradeskill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FGameplayTag Skill;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Experience = 0;
};

/**
 *  A character: everything that goes with its player into any game (their own or a friend's).
 *  Each player keeps their characters on their own PC; when they join a game, the host checks the character and plays it there.
 *  Also sent over the network (to the host when joining, back to the player to save), so: arrays, no maps
 */
USTRUCT(BlueprintType)
struct FZCharacterSaveData
{
	GENERATED_BODY()

	/** Tells characters apart: a game only saves into the character it loaded */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FGuid CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString CharacterName;

	/** Character experience; the level comes from it */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Experience = 0;

	/** Trait points from bosses and items (points from levels come from the experience) */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 BonusTraitPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FZSavedTraitRank> Traits;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FZSavedTradeskill> Tradeskills;

	/** Recipes found by combining items */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FPrimaryAssetId> DiscoveredRecipes;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TArray<FZSavedItem> Inventory;

	/** False until the character first spawns and gets the starting items */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bReceivedStartingItems = false;
};

/** The host's world: what's been done in it. Only the host's save is used for the world; a player who joins someone keeps theirs as it was */
USTRUCT(BlueprintType)
struct FZWorldSaveData
{
	GENERATED_BODY()

	/** Progress tags, e.g. WorldProgress.Boss.StoneGolem once that boss is beaten */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FGameplayTagContainer Progress;

	/** The level last played, e.g. /Game/ThirdPerson/Lvl_ThirdPerson (empty: the game's default level) */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString MapName;
};

/** What a menu shows about a save slot */
USTRUCT(BlueprintType)
struct FZSaveSlotInfo
{
	GENERATED_BODY()

	/** The slot's name, to load or delete it (its file name) */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString CharacterName;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FTimespan PlayTime;

	/** When it was last saved (local time) */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime LastSaved;

	/** The level it was last played in (empty: the game's default level) */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FString MapName;

	/** False if it can't be loaded: damaged, or made by a newer version of the game */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bCanLoad = true;

	/** The slot this game uses right now */
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	bool bIsActive = false;
};
