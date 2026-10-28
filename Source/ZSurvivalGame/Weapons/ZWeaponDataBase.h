// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZWeaponDataBase.generated.h"

class USkeletalMesh;
class UStaticMesh;
class UAnimMontage;
enum class EZWeaponStat : uint8;

/**
 *  What every weapon has, ranged or melee: its name, its animation set and weapon slot, how it looks in the hands, taking it out, and upgrades
 *  Make a Data Asset of one of its children: ZRangedWeaponData (guns) or ZMeleeWeaponData (knives, swords, heavy weapons).
 *  The player carries the weapon as an item whose Weapon fragment points to its data
 */
UCLASS(Abstract, BlueprintType)
class UZWeaponDataBase : public UPrimaryDataAsset
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

	/** Character animation played when the weapon is taken out (optional; plays with the Equip Cue). It must stay in place: one that moves the character (root motion) isn't played */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UAnimMontage> EquipMontage;

	/** --- EFFECTS: what plays is a GameplayCue Notify asset with the tag (see AUDIO.md); a child tag gives this weapon its own. Empty = nothing --- */

	/** The weapon is taken out; the Equip Montage plays with it */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Weapon.Equip"))
	FGameplayTag EquipCue = ZGameplayTags::GameplayCue_Weapon_Equip;

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

	/** A stat before upgrades and mods (0 for a stat this kind of weapon doesn't have, e.g. a sword's magazine) */
	virtual float GetBaseStat(EZWeaponStat Stat) const { return 0.f; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
