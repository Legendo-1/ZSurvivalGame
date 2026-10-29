// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ZSurvivalGameTypes.generated.h"

/** Trace channel for weapon shots (set up in DefaultEngine.ini). Characters ignore it with their capsule and block it with their mesh, so hits know the bone */
inline constexpr ECollisionChannel ZTraceChannel_Weapon = ECC_GameTraceChannel1;

/** Trace channel for finding interactables (set up in DefaultEngine.ini). Ignored by everything except collisions of interactable things, which block it */
inline constexpr ECollisionChannel ZTraceChannel_Interaction = ECC_GameTraceChannel2;

/** Object channels of the spacing capsules around players and around enemies (UZCharacterSpacingComponent; set up in DefaultEngine.ini). Everything ignores them except the other side's capsules */
inline constexpr ECollisionChannel ZObjectChannel_PlayerSpacing = ECC_GameTraceChannel3;
inline constexpr ECollisionChannel ZObjectChannel_EnemySpacing = ECC_GameTraceChannel4;

/** Which weapon animation set the character uses (add new ones at the end: the animation blueprint's Blend Poses nodes keep their pins by position) */
UENUM(BlueprintType)
enum class EZWeaponType : uint8
{
	Unarmed,
	Pistol,
	Rifle,
	/** Knives, machetes, swords: one hand */
	MeleeOneHanded,
	/** Axes, sledgehammers, big swords: both hands */
	MeleeTwoHanded
};

/** True for the melee weapon types (their attack buttons swing instead of shooting and aiming) */
inline bool IsMeleeWeaponType(EZWeaponType WeaponType)
{
	return WeaponType == EZWeaponType::MeleeOneHanded || WeaponType == EZWeaponType::MeleeTwoHanded;
}

/** Temporary cap on movement speed, e.g. during a weapon swap or reload */
UENUM(BlueprintType)
enum class EZSpeedLimit : uint8
{
	None,	// no cap
	Jog,	// can't sprint, jogging allowed
	Walk,	// capped at WalkSpeed
	Still	// no walking at all (root motion still moves the character), e.g. during a melee attack or a stagger
};

/** The two kinds of melee attack: light (left mouse button) and heavy (right mouse button) */
UENUM(BlueprintType)
enum class EZMeleeAttackType : uint8
{
	Light,
	Heavy
};

/** Things a player does that commit them (they can't do something else until it's done) and that wait in the input buffer while they're busy (see COMBAT.md) */
UENUM(BlueprintType)
enum class EZCombatAction : uint8
{
	None,
	LightAttack,
	HeavyAttack,
	Dodge,
	Fire,
	Reload,
	/** Taking out the weapon of a weapon slot (keys 1, 2, 3) */
	SwitchWeapon
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
