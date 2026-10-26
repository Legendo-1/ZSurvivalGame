// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/** Gameplay tags defined in C++, so code can use them without looking them up by name */
namespace ZGameplayTags
{
	// Abilities: set on each ability, used to cancel or block abilities by type
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Sprint);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Aim);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Reload);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_MeleeAttack);

	// States: on the character while an ability runs (or while dead), used to block other abilities
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Firing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// Set by caller: values filled in by code when a gameplay effect is applied
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Healing);

	// Item types: on item definitions (ItemTags), for sorting, filtering and rules
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Weapon);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Ammo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Material);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Consumable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Tool);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_Salvage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Type_WeaponMod);

	// Tools: what kind of tool an item is, so a recipe can ask for any tool of a kind (its quality is on the Tool fragment)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Tool_Screwdriver);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Tool_AmmoPress);

	// Item stats: numbers stored per item instance
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_MagazineAmmo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_Durability);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Stat_UpgradeLevel);

	// Weapon mod slots: which slots a weapon has (its weapon data) and which one a mod goes into (its Weapon Mod fragment)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_ModSlot_Barrel);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_ModSlot_Sight);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_ModSlot_Magazine);

	// Crafting stations: what a station is (on AZCraftingStation) and what a recipe needs nearby
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Crafting_Station_Workbench);

	// Tradeskills: every tag under Tradeskill is a skill players level up by crafting (more can be added in Project Settings → Gameplay Tags)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tradeskill);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tradeskill_Mechanics);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tradeskill_Electronics);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tradeskill_Chemistry);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tradeskill_Medicine);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tradeskill_Weaponsmithing);

	// Trait categories: group traits on the character screen
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_Category_Survival);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_Category_Combat);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trait_Category_Crafting);

	// Gameplay cues: visuals and sounds everyone sees (made as GameplayCue Notify assets with the same tag)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Character_Hit);
}
