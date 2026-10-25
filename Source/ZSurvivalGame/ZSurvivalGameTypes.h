// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZSurvivalGameTypes.generated.h"

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
	Dodge
};
