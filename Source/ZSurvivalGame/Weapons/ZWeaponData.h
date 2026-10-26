// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZSurvivalGameTypes.h"
#include "ZWeaponData.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UAnimMontage;
class UZItemDefinition;

/**
 *  A weapon's stats and looks. Make one Data Asset per weapon:
 *  right-click in the Content Browser → Miscellaneous → Data Asset → ZWeaponData
 *  The player carries it as an item whose Weapon fragment points to this
 */
UCLASS(BlueprintType)
class UZWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Name shown in the UI */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FText DisplayName;

	/** Which animation set the character uses while holding it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EZWeaponType WeaponType = EZWeaponType::Rifle;

	/** --- LOOKS --- */

	/** The weapon's mesh: set this or Static Mesh (none is fine for testing; the weapon is then invisible) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UStaticMesh> StaticMesh;

	/** Socket or bone on the character's mesh that holds the weapon (falls back to hand_r if the mesh doesn't have it) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	FName AttachSocket = TEXT("weapon_r");

	/** Offset from that socket, to line the weapon up with the hand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	FTransform AttachOffset;

	/** Socket on the weapon's mesh where shots and muzzle flashes start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	FName MuzzleSocket = TEXT("Muzzle");

	/** Character animation played for each shot (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> FireMontage;

	/** Character animation played while reloading (optional) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** --- FIRING --- */

	/** Damage per bullet */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float Damage = 20.f;

	/** Damage multiplier for hits on the head */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0"))
	float HeadshotMultiplier = 2.f;

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

	/** --- UPGRADES AND MODS --- */

	/** The mod slots it has, e.g. Weapon.ModSlot.Barrel; one mod fits in each */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrades", meta = (Categories = "Weapon.ModSlot"))
	FGameplayTagContainer ModSlots;

	/** How many times it can be upgraded */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "0"))
	int32 MaxUpgradeLevel = 5;

	/** Extra damage per upgrade level: 0.1 = +10% each */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrades", meta = (ClampMin = "0"))
	float DamagePerUpgradeLevel = 0.1f;
};
