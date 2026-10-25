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

	// Gameplay cues: visuals and sounds everyone sees (made as GameplayCue Notify assets with the same tag)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Impact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Character_Hit);
}
