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

	// States: on the player while an ability runs (or while dead), used to block other abilities
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Sprinting);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
}
