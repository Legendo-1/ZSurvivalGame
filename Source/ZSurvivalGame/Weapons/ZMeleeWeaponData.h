// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ZWeaponDataBase.h"
#include "ZMeleeWeaponData.generated.h"

class UZMeleeMoveset;

/**
 *  A melee weapon's stats and looks: knives, machetes, swords, axes, sledgehammers. Make one Data Asset per weapon:
 *  right-click in the Content Browser → Miscellaneous → Data Asset → ZMeleeWeaponData
 *  Its attacks (light and heavy combos with their animations) are in its Moveset, which weapons of the same kind share (see COMBAT.md)
 */
UCLASS(BlueprintType)
class UZMeleeWeaponData : public UZWeaponDataBase
{
	GENERATED_BODY()

public:

	UZMeleeWeaponData();

	/** --- MELEE --- */

	/** The attacks: a light combo (left mouse button) and a heavy combo (right mouse button), with their animations */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	TObjectPtr<UZMeleeMoveset> Moveset;

	/** Damage per hit (each attack in the moveset multiplies it; upgrades and the holder's traits add to it) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0"))
	float Damage = 25.f;

	/** Poise damage per hit: how much each hit wears down the target's poise toward a stagger (each attack multiplies it) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0"))
	float PoiseDamage = 15.f;

	/** Multiplies the stamina every attack costs, e.g. 1.5 for a heavy weapon */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0"))
	float StaminaCostMultiplier = 1.f;

	/** How fast its attack animations play: 1 = as made, 1.2 = 20% faster */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.1", ClampMax = "5"))
	float AttackSpeed = 1.f;

	/** --- HITBOX: where the weapon hits during an attack's Weapon Hitbox notify --- */

	/** Socket on the weapon's mesh at one end of the blade (or the head of a hammer); hits are found along the line to Blade End Socket */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox")
	FName BladeStartSocket = TEXT("BladeStart");

	/** Socket on the weapon's mesh at the other end of the blade (the tip) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox")
	FName BladeEndSocket = TEXT("BladeEnd");

	/** How far around that line hits count, in cm (the blade's thickness) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox", meta = (ClampMin = "1", Units = "cm"))
	float BladeRadius = 10.f;

	/** Without a mesh that has both blade sockets: a ball around this bone or socket of the character's mesh hits instead (e.g. the hand) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox")
	FName FallbackHitboxSocket = TEXT("hand_r");

	/** Size of that ball, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox", meta = (ClampMin = "1", Units = "cm"))
	float FallbackHitboxRadius = 45.f;

	/** The farthest from the attacker a hit can be, in cm. The server ignores hits farther away (it adds some slack for network delay: Project Settings → Game → Combat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hitbox", meta = (ClampMin = "50", Units = "cm"))
	float MaxReach = 300.f;

	/** --- EFFECTS --- */

	/** A hit on a character, e.g. a slash sound and blood (Location = the hit point, Physical Material = the body's surface, Raw Magnitude = the damage). The target's own Hit Cue plays too */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.MeleeImpact"))
	FGameplayTag ImpactCue = ZGameplayTags::GameplayCue_Weapon_MeleeImpact;

	virtual float GetBaseStat(EZWeaponStat Stat) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
