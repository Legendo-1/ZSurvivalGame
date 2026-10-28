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

	/** Spawns zombies in front of the camera, side by side, Distance cm away (8 m if left out), e.g. ZSpawnZombies 3 */
	UFUNCTION(Exec)
	void ZSpawnZombies(int32 Count = 1, float Distance = 800.f);

	/** Spawns a zombie that stands still (no AI), Distance cm in front of your character and facing you: something to practice attacks and staggers on, e.g. ZSpawnDummy 120;
	 *  ZSpawnDummy 120 all puts one in front of every player (host) */
	UFUNCTION(Exec)
	void ZSpawnDummy(float Distance = 120.f, const FString& Who = TEXT(""));

	/** Kills every AI character in the level */
	UFUNCTION(Exec)
	void ZKillEnemies();

	/** Fills your health and stamina */
	UFUNCTION(Exec)
	void ZHeal();

	/** Takes health from you, e.g. ZHurt 50 to try a bandage (ZHurt 0 just shows your health) */
	UFUNCTION(Exec)
	void ZHurt(float Amount = 0.f);

	/** Gives you items by their asset name, e.g. ZGiveItem ID_Ammo_Rifle 30; ZGiveItem ID_Ammo_Rifle 30 all gives them to every player (host) */
	UFUNCTION(Exec)
	void ZGiveItem(const FString& ItemName, int32 Count = 1, const FString& Who = TEXT(""));

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

	/** Writes the stats of the weapon in your hands to the Output Log: its upgrade level and mods, and what they and your traits make of it (as this window sees it) */
	UFUNCTION(Exec)
	void ZWeaponStats();

	/** --- COMBAT --- */

	/** Staggers you as if your poise broke; ZStagger players staggers every player, ZStagger enemies every AI character (host) */
	UFUNCTION(Exec)
	void ZStagger(const FString& Who = TEXT(""));

	/** Writes your combat state to the Output Log: the attack, the input buffer, busy or staggered, open windows, poise and stamina (as this window sees it) */
	UFUNCTION(Exec)
	void ZCombat();

	/** --- EQUIPMENT --- */

	/** Equips one of your items, e.g. ZEquip ID_Jewelry_IronRing, or in the third ring slot: ZEquip ID_Jewelry_IronRing 3 (a request, like the equipment window) */
	UFUNCTION(Exec)
	void ZEquip(const FString& ItemName, int32 SlotNumber = 0);

	/** Takes off one of your equipped items, e.g. ZUnequip ID_Rifle */
	UFUNCTION(Exec)
	void ZUnequip(const FString& ItemName);

	/** Writes what you have equipped, what's in your hands and your armor to the Output Log (as this window sees it) */
	UFUNCTION(Exec)
	void ZEquipment();

	/**
	 *  Works the inventory screen like the mouse does, and writes what it shows to the Output Log (for automated tests). ZInventoryUI on its own opens or closes it.
	 *  open, close, show, select <item>, use <item>, drop <item>, add <item> (to the combine slots), remove <item>, clear, count <n>, combine, drag <item> <onto item>, raise <trait>, reset,
	 *  equip <item> <slot kind> <number> (drag onto an equipment slot, e.g. equip ID_Rifle LongGun 1), unequip <item> (right-click in the equipment window)
	 */
	UFUNCTION(Exec)
	void ZInventoryUI(const FString& Command = TEXT(""));

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

	/** --- PROGRESSION --- */

	/** Gives you character experience, e.g. ZGiveXP 500; ZGiveXP 500 all gives it to every player (host) */
	UFUNCTION(Exec)
	void ZGiveXP(int32 Amount = 100, const FString& Who = TEXT(""));

	/** Gives you trait points, like a boss or a Tome of Knowledge; ZGiveTraitPoints 3 all gives every player some (host) */
	UFUNCTION(Exec)
	void ZGiveTraitPoints(int32 Points = 1, const FString& Who = TEXT(""));

	/** Writes your level, trait points, every trait and your stats to the Output Log */
	UFUNCTION(Exec)
	void ZTraits();

	/** Spends points on a trait, e.g. ZRaiseTrait Vigor (the asset name, with or without TR_) */
	UFUNCTION(Exec)
	void ZRaiseTrait(const FString& TraitName);

	/** Takes back all your spent trait points */
	UFUNCTION(Exec)
	void ZResetTraits();

	/** --- SAVING (see SAVING.md) --- */

	/** Saves now, like a menu's Save button (host: every player and the world; a joined player: their own character) */
	UFUNCTION(Exec)
	void ZSave();

	/** Writes the save slots on this PC to the Output Log: character, level, time played, when it was saved */
	UFUNCTION(Exec)
	void ZSaveSlots();

	/** Writes what the slot in use holds to the Output Log (as last saved on this PC): the character and the world */
	UFUNCTION(Exec)
	void ZSaveInfo();

	/** Makes a new character in a new slot and starts a game with it (the game running now is saved first), e.g. ZNewCharacter Alex */
	UFUNCTION(Exec)
	void ZNewCharacter(const FString& CharacterName = TEXT(""));

	/** Continues a save slot: opens its level as the host (the game running now is saved first), e.g. ZLoadSlot Slot2 */
	UFUNCTION(Exec)
	void ZLoadSlot(const FString& SlotName);

	/** Deletes a save slot for good (not the one in use), e.g. ZDeleteSlot Slot2 */
	UFUNCTION(Exec)
	void ZDeleteSlot(const FString& SlotName);

	/** Marks something as done in the world, saved with the host's game, e.g. ZWorldProgress WorldProgress.Boss.StoneGolem (host); on its own it lists what's done */
	UFUNCTION(Exec)
	void ZWorldProgress(const FString& Tag = TEXT(""));

	/** For testing the host's checks: breaks the game's rules in the slot in use (huge numbers, items that don't exist, ...) and writes it. The next game with it shows what the host fixed */
	UFUNCTION(Exec)
	void ZBreakSave();

	/** --- SOUND --- */

	/** Makes the floor under you sound like a surface until the game ends, e.g. ZFloorSurface Metal (in this window only); ZFloorSurface on its own says what it is now */
	UFUNCTION(Exec)
	void ZFloorSurface(const FString& SurfaceName = TEXT(""));

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
