// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayTags.h"

namespace ZGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability, "Ability", "Parent of every ability's tag (a stagger cancels them all)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Type_Action, "Ability.Type.Action", "Actions that commit the character: attacks, shots, dodges. One started in a window of another cancels that one");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Sprint, "Ability.Sprint", "Sprint ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Aim, "Ability.Aim", "Aim ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dodge, "Ability.Dodge", "Dodge ability");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "Ability.Fire", "Fire the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "Ability.Reload", "Reload the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_MeleeAttack, "Ability.MeleeAttack", "Melee attack (used by AI characters)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Light, "Ability.Attack.Light", "A player's light melee attack (one step of the light combo)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack_Heavy, "Ability.Attack.Heavy", "A player's heavy melee attack (one step of the heavy combo)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Stagger, "Ability.Stagger", "Staggered: the hit reaction when poise breaks");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "State.Sprinting", "Sprint key held (the speed only changes while sprinting is allowed)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Aiming down sights");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "In the middle of a dodge");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Firing, "State.Firing", "Firing a shot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Reloading the equipped weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "In the middle of a melee attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Out of health; blocks all abilities");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Busy, "State.Busy", "Committed to an action (attack, shot, dodge, stagger); other actions wait in the input buffer unless a window allows them");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Staggered, "State.Staggered", "Staggered (poise broken); no actions until it's over");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Takes no damage or poise damage, e.g. the first part of a dodge");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Window_Combo, "Window.Combo", "The next attack (or a shot) may start, cutting the current attack short");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Window_Dodge, "Window.Dodge", "A dodge may start, cutting the current action short");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Window_Turn, "Window.Turn", "An attack still turns toward the camera (its windup); after it the attack keeps its direction until the Dodge Window");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Attack_Light, "Event.Attack.Light", "Starts a light attack (Event Magnitude = the step in the light combo)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Attack_Heavy, "Event.Attack.Heavy", "Starts a heavy attack (Event Magnitude = the step in the heavy combo)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Stagger, "Event.Stagger", "Staggers the character (Instigator = who broke its poise)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Damage, "SetByCaller.Damage", "Damage amount for UZGameplayEffect_Damage");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Healing, "SetByCaller.Healing", "Healing amount for UZGameplayEffect_Heal");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_PoiseDamage, "SetByCaller.PoiseDamage", "Poise damage for UZGameplayEffect_Damage");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Weapon, "Item.Type.Weapon", "Weapons");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Ammo, "Item.Type.Ammo", "Ammunition used when reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Material, "Item.Type.Material", "Crafting materials");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Consumable, "Item.Type.Consumable", "Items used up from the inventory, e.g. bandages");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Tool, "Item.Type.Tool", "Tools that recipes need; kept when crafting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Salvage, "Item.Type.Salvage", "Things found in the world that can be taken apart or combined");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_WeaponMod, "Item.Type.WeaponMod", "Mods that are installed on weapons");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Armor, "Item.Type.Armor", "Armor and clothing, worn on the head, torso or legs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Jewelry, "Item.Type.Jewelry", "Rings and necklaces");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Tool_Screwdriver, "Item.Tool.Screwdriver", "Counts as a screwdriver for recipes");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Tool_AmmoPress, "Item.Tool.AmmoPress", "Counts as an ammo press for recipes");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat, "Item.Stat", "Parent of every number stored per item instance");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_MagazineAmmo, "Item.Stat.MagazineAmmo", "Rounds in a weapon item's magazine");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_Durability, "Item.Stat.Durability", "Durability left on an item with a Durability fragment");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Stat_UpgradeLevel, "Item.Stat.UpgradeLevel", "How many times a weapon item was upgraded");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_ModSlot_Barrel, "Weapon.ModSlot.Barrel", "Barrel mods: compensators, longer barrels");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_ModSlot_Sight, "Weapon.ModSlot.Sight", "Sights");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_ModSlot_Magazine, "Weapon.ModSlot.Magazine", "Magazines and loaders");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot, "Equipment.Slot", "Parent of every kind of equipment slot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_ShortGun, "Equipment.Slot.ShortGun", "Short guns: pistols, submachine guns, sawn-off shotguns");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_LongGun, "Equipment.Slot.LongGun", "Long guns: rifles, long shotguns, sniper rifles");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Melee, "Equipment.Slot.Melee", "Melee weapons");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Head, "Equipment.Slot.Head", "Helmets, hats and masks");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Torso, "Equipment.Slot.Torso", "Jackets, vests and body armor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Legs, "Equipment.Slot.Legs", "Trousers and leg armor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Neck, "Equipment.Slot.Neck", "Necklaces and amulets");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Ring, "Equipment.Slot.Ring", "Rings");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Crafting_Station_Workbench, "Crafting.Station.Workbench", "A workbench, e.g. in the hub");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill, "Tradeskill", "Parent of every tradeskill");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Mechanics, "Tradeskill.Mechanics", "Tools, ammo and mechanical parts");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Electronics, "Tradeskill.Electronics", "Salvaging and building electronics");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Chemistry, "Tradeskill.Chemistry", "Gunpowder, acids and other chemicals");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Medicine, "Tradeskill.Medicine", "Bandages, medkits and other healing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Tradeskill_Weaponsmithing, "Tradeskill.Weaponsmithing", "Weapon upgrades and weapon mods");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(WorldProgress, "WorldProgress", "Parent of everything done in the host's world, saved with it: bosses beaten, shortcuts opened (e.g. WorldProgress.Boss.StoneGolem)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Category_Survival, "Trait.Category.Survival", "Health, stamina and moving around");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Category_Combat, "Trait.Category.Combat", "Damage, recoil and reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trait_Category_Crafting, "Trait.Category.Crafting", "Crafting and tradeskills");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Fire, "GameplayCue.Weapon.Fire", "A weapon fired (Location = muzzle, Normal = shot direction, Source Object = the weapon data)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Impact, "GameplayCue.Weapon.Impact", "A shot hit something (Location = impact point, Normal = surface normal, Physical Material = the surface)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Reload, "GameplayCue.Weapon.Reload", "A reload started (Raw Magnitude = seconds it takes, Source Object = the weapon data)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Equip, "GameplayCue.Weapon.Equip", "A weapon was taken out (Source Object = the weapon data; its Equip Montage plays too)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_MeleeImpact, "GameplayCue.Weapon.MeleeImpact", "A melee weapon hit a character (Location = hit point, Normal = the swing's direction, Physical Material = the body's surface, Raw Magnitude = damage dealt, Source Object = the weapon data)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Character_Hit, "GameplayCue.Character.Hit", "A character took damage (Location = hit point, Raw Magnitude = damage after armor)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Character_Death, "GameplayCue.Character.Death", "A character died (Instigator = the killer)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Character_Stagger, "GameplayCue.Character.Stagger", "A character's poise broke and it staggers (Instigator = who broke it)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Character_LevelUp, "GameplayCue.Character.LevelUp", "A player reached a new level (Raw Magnitude = the level)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Pickup, "GameplayCue.Item.Pickup", "A player picked something up (Source Object = the item, Raw Magnitude = how many)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Use, "GameplayCue.Item.Use", "A player used up an item, e.g. a bandage (Source Object = the item)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Equip, "GameplayCue.Item.Equip", "A player put on armor or jewelry (Source Object = the item; weapons have their weapon data's Equip Cue)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Crafting_Working, "GameplayCue.Crafting.Working", "Lasts while a player crafts (Location = the station, if the recipe needs one; Source Object = the recipe)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Crafting_Done, "GameplayCue.Crafting.Done", "A combine made something (Source Object = the recipe, Raw Magnitude = how many crafts)");
}
