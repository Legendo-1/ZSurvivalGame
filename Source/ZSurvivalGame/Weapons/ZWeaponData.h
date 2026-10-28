// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZGameplayTags.h"
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

	/** Which weapon slot it goes in: short gun (pistols, submachine guns, sawn-offs), long gun (rifles, shotguns, snipers) or melee. A player equips one of each */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (Categories = "Equipment.Slot"))
	FGameplayTag EquipmentSlot = ZGameplayTags::Equipment_Slot_LongGun;

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

	/** Character animation played while reloading (optional; it's sped up or slowed down to the reload time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** Character animation played when the weapon is taken out (optional; plays with the Equip Cue). It must stay in place: one that moves the character (root motion) isn't played */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> EquipMontage;

	/** --- EFFECTS: what plays is a GameplayCue Notify asset with the tag (see AUDIO.md); a child tag gives this weapon its own. Empty = nothing --- */

	/** Each shot, e.g. the bang and the muzzle flash (Location = the muzzle) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Fire"))
	FGameplayTag FireCue = ZGameplayTags::GameplayCue_Weapon_Fire;

	/** Where a shot hits something (Location = the hit point; the surface's physical material can pick e.g. a metal or a wood sound) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Impact"))
	FGameplayTag ImpactCue = ZGameplayTags::GameplayCue_Weapon_Impact;

	/** A reload starts (Raw Magnitude = the seconds it takes). With a reload animation, sounds put in the animation line up better */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Reload"))
	FGameplayTag ReloadCue = ZGameplayTags::GameplayCue_Weapon_Reload;

	/** The weapon is taken out; the Equip Montage plays with it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Equip"))
	FGameplayTag EquipCue = ZGameplayTags::GameplayCue_Weapon_Equip;

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

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
