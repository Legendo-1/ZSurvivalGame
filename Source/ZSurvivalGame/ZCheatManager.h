// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ZCheatManager.generated.h"

/**
 *  Console commands for testing (open the console with the ` key; not in shipping builds)
 *  They change the game on the host, so in multiplayer type them in the host's (Server) window
 */
UCLASS()
class UZCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	// numbers left out of a command use the defaults below (the engine only reads them in editor builds)

	/** Spawns zombies 8 m in front of the camera, side by side */
	UFUNCTION(Exec)
	void ZSpawnZombies(int32 Count = 1);

	/** Kills every AI character in the level */
	UFUNCTION(Exec)
	void ZKillEnemies();

	/** Fills your health and stamina */
	UFUNCTION(Exec)
	void ZHeal();

	/** Takes health from you, e.g. ZHurt 50 to try a bandage (ZHurt 0 just shows your health) */
	UFUNCTION(Exec)
	void ZHurt(float Amount = 0.f);

	/** Gives you items by their asset name, e.g. ZGiveItem ID_Ammo_Rifle 30 */
	UFUNCTION(Exec)
	void ZGiveItem(const FString& ItemName, int32 Count = 1);

	/** Writes your inventory to the Output Log (works in every window, shows that player's copy) */
	UFUNCTION(Exec)
	void ZShowInventory();

	/** Drops some of your items in front of you, e.g. ZDropItem ID_Ammo_Rifle 10 */
	UFUNCTION(Exec)
	void ZDropItem(const FString& ItemName, int32 Count = 1);

	/** Uses one of your items, e.g. ZUseItem ID_Bandage */
	UFUNCTION(Exec)
	void ZUseItem(const FString& ItemName);

	/** Spawns a pickup like enemy loot, Distance cm in front of you (0 = at your feet), e.g. ZSpawnPickup ID_Cloth 3 200 */
	UFUNCTION(Exec)
	void ZSpawnPickup(const FString& ItemName, int32 Count = 1, float Distance = 200.f);

	/** --- CRAFTING --- */

	/** Combines items from your inventory, like the inventory screen does, e.g. ZCombine ID_BrokenRadio ID_Screwdriver (a number at the end crafts that many) */
	UFUNCTION(Exec)
	void ZCombine(const FString& ItemNames);

	/** Writes your tradeskills, discoveries and what you're crafting to the Output Log */
	UFUNCTION(Exec)
	void ZSkills();

	/** Sets one of your tradeskills to a level, e.g. ZSetSkill Electronics 20 (host) */
	UFUNCTION(Exec)
	void ZSetSkill(const FString& SkillName, int32 Level = 0);

	/** For developers: writes every recipe with its inputs and requirements to the Output Log (players never see recipes) */
	UFUNCTION(Exec)
	void ZListRecipes();

	/** Forgets your discoveries, so the first-time bonus happens again (host) */
	UFUNCTION(Exec)
	void ZForgetDiscoveries();

	/** Gives you a set of crafting materials and a screwdriver to try recipes with; ZGiveCraftingKit all gives one to every player (host) */
	UFUNCTION(Exec)
	void ZGiveCraftingKit(const FString& Who = TEXT(""));

	/** Spawns a workbench 2 m in front of you; ZSpawnWorkbench all puts one in front of every player (host) */
	UFUNCTION(Exec)
	void ZSpawnWorkbench(const FString& Who = TEXT(""));

	/** Sets the durability of your first item of this kind, e.g. ZSetDurability ID_Screwdriver 1 to see it break (host) */
	UFUNCTION(Exec)
	void ZSetDurability(const FString& ItemName, int32 Durability = 1);

	/** --- TESTING --- */

	/** Runs a console command after some seconds, e.g. ZDelay 3 ZSkills (for test runs with -ExecCmds, which runs everything at once) */
	UFUNCTION(Exec)
	void ZDelay(float Seconds, const FString& Command);

	/** Runs a console command on every client's machine, e.g. ZOnClients ZSkills (host; for multiplayer test runs) */
	UFUNCTION(Exec)
	void ZOnClients(const FString& Command);

private:

	/** True on the host (or in single player); otherwise logs that the command only works there */
	bool CheckIsHost(const TCHAR* CommandName) const;

	/** An item by its asset name (e.g. ID_Rifle), found through the Asset Manager; logs the known items if there's none */
	class UZItemDefinition* FindItem(const FString& ItemName) const;

	/** Your inventory */
	class UZInventoryComponent* GetMyInventory() const;

	/** Your PlayerState, or with "all" every player's */
	TArray<class APlayerState*> GetTargetPlayers(const FString& Who) const;
};
