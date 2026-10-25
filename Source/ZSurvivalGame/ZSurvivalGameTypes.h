// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ZSurvivalGameTypes.generated.h"

/** Trace channel for weapon shots (set up in DefaultEngine.ini). Characters ignore it with their capsule and block it with their mesh, so hits know the bone */
inline constexpr ECollisionChannel ZTraceChannel_Weapon = ECC_GameTraceChannel1;

/** Which weapon animation set the character uses */
UENUM(BlueprintType)
enum class EZWeaponType : uint8
{
	Unarmed,
	Pistol,
	Rifle
};

/** Temporary cap on movement speed, e.g. during a weapon swap or reload */
UENUM(BlueprintType)
enum class EZSpeedLimit : uint8
{
	None,	// no cap
	Jog,	// can't sprint, jogging allowed
	Walk	// capped at WalkSpeed
};

/** Inputs that activate abilities; each ability says which one it listens to (see UZGameplayAbility::InputID) */
UENUM(BlueprintType)
enum class EZAbilityInputID : uint8
{
	None,
	Sprint,
	Aim,
	Dodge,
	Fire,
	Reload
};

/** Which side a character is on. Same team: no damage between them. Different teams: hostile */
UENUM(BlueprintType)
enum class EZTeam : uint8
{
	Players,
	Monsters,	// zombies and other monsters
	Humans		// human NPCs that fight both (friendly NPCs use Players)
};
