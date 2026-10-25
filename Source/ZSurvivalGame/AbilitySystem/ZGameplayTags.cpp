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

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Fire, "GameplayCue.Weapon.Fire", "A weapon fired (Location = muzzle, Normal = shot direction)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Weapon_Impact, "GameplayCue.Weapon.Impact", "A shot hit something (Location = impact point, Normal = surface normal)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Character_Hit, "GameplayCue.Character.Hit", "A character took damage (Location = hit point, RawMagnitude = damage)");
}
