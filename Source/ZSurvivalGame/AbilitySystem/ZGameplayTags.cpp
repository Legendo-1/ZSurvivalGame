// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayTags.h"

namespace ZGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "Ability.Sprint", "Sprint ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Aim, "Ability.Aim", "Aim ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ability.Dodge", "Dodge ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "Ability.Fire", "Fire the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "Ability.Reload", "Reload the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_MeleeAttack, "Ability.MeleeAttack", "Melee attack (used by AI characters)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "State.Sprinting", "Sprint key held (the speed only changes while sprinting is allowed)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Aiming down sights");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "In the middle of a dodge");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Firing, "State.Firing", "Firing a shot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Reloading the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "In the middle of a melee attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Out of health; blocks all abilities");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "Damage amount for UZGameplayEffect_Damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Healing, "SetByCaller.Healing", "Healing amount for UZGameplayEffect_Heal");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Weapon, "Item.Type.Weapon", "Weapons");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Ammo, "Item.Type.Ammo", "Ammunition used when reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Material, "Item.Type.Material", "Crafting materials");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Consumable, "Item.Type.Consumable", "Items used up from the inventory, e.g. bandages");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Tool, "Item.Type.Tool", "Tools that recipes need; kept when crafting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Salvage, "Item.Type.Salvage", "Things found in the world that can be taken apart or combined");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Tool_Screwdriver, "Item.Tool.Screwdriver", "Counts as a screwdriver for recipes");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Tool_AmmoPress, "Item.Tool.AmmoPress", "Counts as an ammo press for recipes");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_MagazineAmmo, "Item.Stat.MagazineAmmo", "Rounds in a weapon item's magazine");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Durability, "Item.Stat.Durability", "Durability left on an item with a Durability fragment");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crafting_Station_Workbench, "Crafting.Station.Workbench", "A workbench, e.g. in the hub");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill, "Tradeskill", "Parent of every tradeskill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Mechanics, "Tradeskill.Mechanics", "Tools, ammo and mechanical parts");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Electronics, "Tradeskill.Electronics", "Salvaging and building electronics");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Chemistry, "Tradeskill.Chemistry", "Gunpowder, acids and other chemicals");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Medicine, "Tradeskill.Medicine", "Bandages, medkits and other healing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Weaponsmithing, "Tradeskill.Weaponsmithing", "Weapon upgrades and weapon mods");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Category_Survival, "Trait.Category.Survival", "Health, stamina and moving around");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Category_Combat, "Trait.Category.Combat", "Damage, recoil and reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Category_Crafting, "Trait.Category.Crafting", "Crafting and tradeskills");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Fire, "GameplayCue.Weapon.Fire", "A weapon fired (Location = muzzle, Normal = shot direction)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Impact, "GameplayCue.Weapon.Impact", "A shot hit something (Location = impact point, Normal = surface normal)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Character_Hit, "GameplayCue.Character.Hit", "A character took damage (Location = hit point, RawMagnitude = damage)");
}
