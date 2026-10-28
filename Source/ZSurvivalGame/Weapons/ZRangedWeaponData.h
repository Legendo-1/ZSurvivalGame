// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/ZWeaponDataBase.h"
#include "ZRangedWeaponData.generated.h"

class UAnimMontage;
class UZItemDefinition;

/**
 *  A gun's stats and looks. Make one Data Asset per gun:
 *  right-click in the Content Browser → Miscellaneous → Data Asset → ZRangedWeaponData
 *  (Data assets made before the melee weapons were "ZWeaponData"; they load as this.)
 */
UCLASS(BlueprintType)
class UZRangedWeaponData : public UZWeaponDataBase
{
	GENERATED_BODY()

public:

	/** --- LOOKS --- */

	/** Socket on the weapon's mesh where shots and muzzle flashes start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	FName MuzzleSocket = TEXT("Muzzle");

	/** Character animation played for each shot (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> FireMontage;

	/** Character animation played while reloading (optional; it's sped up or slowed down to the reload time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** --- EFFECTS --- */

	/** Each shot, e.g. the bang and the muzzle flash (Location = the muzzle) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Fire"))
	FGameplayTag FireCue = ZGameplayTags::GameplayCue_Weapon_Fire;

	/** Where a shot hits something (Location = the hit point; the surface's physical material can pick e.g. a metal or a wood sound) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Impact"))
	FGameplayTag ImpactCue = ZGameplayTags::GameplayCue_Weapon_Impact;

	/** A reload starts (Raw Magnitude = the seconds it takes). With a reload animation, sounds put in the animation line up better */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Reload"))
	FGameplayTag ReloadCue = ZGameplayTags::GameplayCue_Weapon_Reload;

	/** --- FIRING --- */

	/** Damage per bullet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float Damage = 20.f;

	/** Damage multiplier for hits on the head */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float HeadshotMultiplier = 2.f;

	/** Poise damage per bullet: how much each hit wears down the target's poise toward a stagger (0 = bullets never stagger) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float PoiseDamage = 0.f;

	/** Shots per second */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0.1"))
	float FireRate = 8.f;

	/** Keeps firing while the button is held; otherwise one shot per press */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing")
	bool bAutomatic = true;

	/** Bullets per shot (more than 1 for shotguns) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "1"))
	int32 BulletsPerShot = 1;

	/** Spread cone in degrees when firing from the hip */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float HipSpread = 3.f;

	/** Spread cone in degrees while aiming */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float AimSpread = 0.5f;

	/** How far shots reach, in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "100"))
	float Range = 10000.f;

	/** Camera kick up per shot, in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float RecoilPitch = 0.4f;

	/** Random camera kick sideways per shot, in degrees */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float RecoilYaw = 0.15f;

	/**
	 *  Seconds after each shot in which the shooter is committed: no dodging or switching weapons yet (pressed meanwhile, they happen right after).
	 *  At most the time between two shots, so it never slows the fire rate (see COMBAT.md)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0", ClampMax = "2", Units = "s"))
	float ShotRecoveryTime = 0.15f;

	/** --- AMMO --- */

	/** Shots per magazine */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "1"))
	int32 MagazineSize = 30;

	/** Seconds to reload */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo", meta = (ClampMin = "0"))
	float ReloadTime = 2.f;

	/** Item taken from the inventory when reloading; empty = unlimited reserve ammo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	TObjectPtr<UZItemDefinition> AmmoItem;

	virtual float GetBaseStat(EZWeaponStat Stat) const override;
};
