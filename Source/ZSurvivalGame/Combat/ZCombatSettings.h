// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ZCombatSettings.generated.h"

/**
 *  The numbers behind combat: the input buffer, poise and staggering, and the server's slack for melee hits (see COMBAT.md)
 *  Project Settings → Game → Combat (saved in Config/DefaultGame.ini)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Combat"))
class UZCombatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UZCombatSettings();

	static const UZCombatSettings* Get() { return GetDefault<UZCombatSettings>(); }

	/** --- INPUT BUFFER --- */

	/** How long an attack, dodge, shot, reload or weapon switch pressed while busy waits for its turn, in seconds. Pressed earlier, it's forgotten */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Input Buffer", meta = (ClampMin = "0", ClampMax = "2", Units = "s"))
	float InputBufferTime = 0.4f;

	/** --- POISE AND STAGGER --- */

	/** Seconds without taking poise damage after which poise is full again */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "0", Units = "s"))
	float PoiseRecoveryDelay = 3.f;

	/** How long a stagger lasts for a character without a Stagger Montage, in seconds */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "0.1", Units = "s"))
	float DefaultStaggerDuration = 0.6f;

	/** --- MELEE --- */

	/** Extra distance the server accepts for a melee hit reported by a player's machine (they see everyone a little out of date), in cm */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0", Units = "cm"))
	float MeleeReachTolerance = 250.f;

	/** How long after the server's copy of an attack ended it still accepts the next attack of the combo from a player's machine (network delay), in seconds */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0", Units = "s"))
	float ComboTimingTolerance = 1.f;

	/** How fast an attack turns toward the camera during its windup (its Turn Window), in degrees per second. Fast enough to look like a deliberate turn, not a snap */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "1", Units = "deg"))
	float AttackTurnRate = 900.f;

	/** For attack montages without a Turn Window: how long from the montage's start the attack turns toward the camera, in seconds of the montage */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0", Units = "s"))
	float AttackTurnTime = 0.2f;
};
