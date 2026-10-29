// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ZCombatSettings.generated.h"

class UMaterialInterface;

/**
 *  The numbers behind combat: the input buffer, poise and staggering, the server's slack for melee hits, lock-on and hit feedback (see COMBAT.md)
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

	/** --- LOCK-ON --- */

	/** Lock-on finds enemies up to this far from your character, in cm */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0", Units = "cm"))
	float LockOnDistance = 250.f;

	/** ...and at most this far from the middle of the screen, in degrees */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "1", ClampMax = "90", Units = "deg"))
	float LockOnAngle = 20.f;

	/** When two enemies are near the middle, how much being farther away counts against one (degrees from the middle per meter). Higher prefers the closer one */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0"))
	float LockOnDistanceWeight = 2.f;

	/** The lock lets go when the target gets farther than this from your character, in cm (more than Lock On Distance, so a step back doesn't end it) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0", Units = "cm"))
	float LoseLockDistance = 600.f;

	/** Turning the camera at least this far from the target toward another enemy switches the lock to it (once that enemy is near the middle of the screen), in degrees */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
	float SwitchTargetAngle = 10.f;

	/** Turning the camera this far away from the target with the mouse or stick, with no other enemy there, lets go of it, in degrees */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "1", ClampMax = "180", Units = "deg"))
	float BreakLockAngle = 35.f;

	/** The lock lets go when walls hide the target this long, in seconds (other characters in between don't count) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0", Units = "s"))
	float LoseSightTime = 1.f;

	/** How quickly the camera follows the target (higher = tighter) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0.1"))
	float LockOnCameraSpeed = 10.f;

	/** How quickly the camera drifts back onto the target after you turned it a little with the mouse or stick (higher = faster; turning past Break Lock Angle before it's back lets go) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On", meta = (ClampMin = "0"))
	float LockOnCameraReturnSpeed = 3.f;

	/** When the target dies, lock on to the next enemy near the middle of the screen (if there's one) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On")
	bool bSwitchTargetOnKill = true;

	/** Aiming a gun lets go of the lock (you aim yourself) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Lock-On")
	bool bReleaseLockOnWhenAiming = true;

	/** --- HIT FEEDBACK --- */

	/** How fast the attacker's animation plays during hit-stop (each attack's Hit Stop Time in its moveset): 0 = frozen, 1 = no slowdown */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hit Feedback", meta = (ClampMin = "0", ClampMax = "1"))
	float HitStopAnimRate = 0.05f;

	/** Laid over a character's mesh when it's hit, fading out (empty = no flash). It may use the parameters FlashAmount (1 → 0) and FlashColor */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hit Feedback")
	TSoftObjectPtr<UMaterialInterface> HitFlashMaterial;

	/** How long the hit flash takes to fade out, in seconds */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hit Feedback", meta = (ClampMin = "0", ClampMax = "1", Units = "s"))
	float HitFlashTime = 0.12f;

	/** The flash's color (the material's FlashColor); above 1 is brighter */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hit Feedback", meta = (HideAlphaChannel))
	FLinearColor HitFlashColor = FLinearColor(2.f, 2.f, 2.f);
};
