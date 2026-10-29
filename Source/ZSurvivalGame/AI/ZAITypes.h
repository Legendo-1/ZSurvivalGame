// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ZAITypes.generated.h"

/**
 *  What an enemy is doing in a fight (see AI.md). Its AI brain on the server (UZAICombatBrainComponent) decides it;
 *  the character replicates it, so animations on every machine can use it (e.g. a strafing blend space while Strafing)
 *  New states go at the end (Blueprints and saved settings keep them by number)
 */
UENUM(BlueprintType)
enum class EZAICombatState : uint8
{
	/** No target: stands still (later: patrols, goes home) */
	Idle,
	/** Has a target: goes toward it until it can attack (or is close enough to circle it) */
	Approaching,
	/** Close to its target, waiting for an opening: circles it while facing it */
	Strafing,
	/** An attack is running: committed until it's over */
	Attacking,
	/** Dodged: a moment before it can act again */
	DodgeRecovering,
	/** Its poise broke: can't do anything until the stagger is over */
	Staggered
};

/** "Strafing" for logs and debug text */
ZSURVIVALGAME_API const TCHAR* LexToString(EZAICombatState State);

/** How an enemy type senses and fights: set per enemy type (Class Defaults → AI|Combat → Combat Settings) */
USTRUCT(BlueprintType)
struct FZAICombatSettings
{
	GENERATED_BODY()

	/** Sees enemies (players) this far away, in its field of view and not through walls, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "cm"))
	float SightRadius = 2500.f;

	/** Keeps seeing a target it already sees up to this far, in cm (more than Sight Radius, so it doesn't flicker at the edge) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "cm"))
	float LoseSightRadius = 3000.f;

	/** Half its field of view, in degrees: 90 sees everything in front of it, 180 all around */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
	float VisionHalfAngle = 80.f;

	/** Notices enemies this close even without seeing them (it hears or smells them), in cm; 0 = only by sight and when hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0", Units = "cm"))
	float ProximityRadius = 400.f;

	/** Gives up on a target it hasn't seen, been near or been hit by for this long, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Senses", meta = (ClampMin = "0.5", Units = "s"))
	float ForgetTime = 8.f;

	/** It only attacks (and circles) a target whose feet are at most this much higher or lower than its own, in cm;
	 *  a target up on a ledge: it looks for a way up (on the navmesh) instead of swinging at the air below it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", Units = "cm"))
	float MaxAttackHeight = 100.f;

	/** Circles its target between attacks, like a souls knight; off: walks straight in and attacks whenever it can, like a zombie */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strafing")
	bool bCanStrafe = false;

	/** How far from its target it circles, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strafing", meta = (ClampMin = "0", Units = "cm", EditCondition = "bCanStrafe"))
	float StrafeDistance = 300.f;

	/** After an attack it circles at least this long before going in again, in seconds (a random time up to Strafe Time Max) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strafing", meta = (ClampMin = "0", Units = "s", EditCondition = "bCanStrafe"))
	float StrafeTimeMin = 1.5f;

	/** ...and at most this long, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strafing", meta = (ClampMin = "0", Units = "s", EditCondition = "bCanStrafe"))
	float StrafeTimeMax = 3.5f;

	/** Walking speed while circling, in cm/s (it chases at the movement component's Max Walk Speed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Strafing", meta = (ClampMin = "0", Units = "CentimetersPerSecond", EditCondition = "bCanStrafe"))
	float StrafeSpeed = 150.f;

	/** How long after a dodge it waits before it can act again, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0", Units = "s"))
	float DodgeRecoveryTime = 0.4f;
};

/** What the AI brain knows when it decides; the rules below only use this, so they can be tested without a world */
struct FZAICombatSnapshot
{
	bool bHasTarget = false;

	/** It sees the target now (not only remembers where it was) */
	bool bTargetVisible = false;

	/** From its middle to the target's, on the ground plane, in cm */
	float DistanceToTarget = 0.f;

	/** How much higher the target's feet are than its own, in cm (negative: lower) */
	float HeightToTarget = 0.f;

	float AttackRange = 150.f;
	float MaxAttackHeight = 100.f;

	/** Its attack cooldown is over and nothing else keeps it busy */
	bool bAttackReady = false;

	bool bCanStrafe = false;
	float StrafeDistance = 300.f;

	/** It has circled long enough since its last attack (always true for enemies that don't strafe) */
	bool bStrafeTimeOver = true;
};

/** The combat state machine's decisions (the brain's state objects use them, and the tests check them) */
namespace ZAICombatRules
{
	/** How much farther than Strafe Distance a strafer may already be when it starts circling, in cm */
	inline constexpr float StrafeEnterSlack = 100.f;

	/** How far past Strafe Distance the target may get before the strafer stops circling and chases it, in cm */
	inline constexpr float StrafeLeash = 250.f;

	/** On about the same level as the target (within Max Attack Height), not below or above it on a ledge */
	ZSURVIVALGAME_API bool IsLevelWithTarget(const FZAICombatSnapshot& Snapshot);

	/** Close enough to attack: within Attack Range on the ground plane, and level with the target */
	ZSURVIVALGAME_API bool IsInAttackRange(const FZAICombatSnapshot& Snapshot);

	/** Approaching: attack when in range and ready; circle while waiting if it's a strafer; otherwise keep closing in (Attacking means "try an attack now") */
	ZSURVIVALGAME_API EZAICombatState WhileApproaching(const FZAICombatSnapshot& Snapshot);

	/** Strafing: keep circling until it may attack again (then go in), or chase a target that got away */
	ZSURVIVALGAME_API EZAICombatState WhileStrafing(const FZAICombatSnapshot& Snapshot);

	/** After an attack, a stagger or a dodge recovery: circle (strafers close by), otherwise go for the target */
	ZSURVIVALGAME_API EZAICombatState AfterAction(const FZAICombatSnapshot& Snapshot);
}
