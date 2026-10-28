// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZWeapon.generated.h"

class ACharacter;
struct FGameplayAttribute;
enum class EZWeaponStat : uint8;
class UMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UZInventoryComponent;
class UZItemDefinition;
class UZItemInstance;
class UZMeleeWeaponData;
class UZRangedWeaponData;
class UZWeaponDataBase;

/**
 *  The weapon in a character's hands (a gun or a melee weapon), spawned by the server when a weapon item is taken out and removed when it's put away
 *  Every machine attaches it to its owner's hand; for a gun the server keeps the ammo, and writes it back to the item when the weapon is put away
 */
UCLASS()
class AZWeapon : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

public:

	AZWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: sets the weapon's data and, for a gun, loads the magazine from its item (full if there's no item) */
	void InitWeapon(UZWeaponDataBase* InWeaponData, UZItemInstance* InItemInstance);

	/** Server: stores the magazine ammo on the item, so it's kept when the weapon is put away or dropped */
	void SaveAmmoToItem();

	UZWeaponDataBase* GetWeaponData() const { return WeaponData; }

	/** The gun's data, or null for a melee weapon */
	const UZRangedWeaponData* GetRangedData() const;

	/** The melee weapon's data, or null for a gun */
	const UZMeleeWeaponData* GetMeleeData() const;

	/** True for knives, swords, axes, ...: the attack buttons swing it instead of shooting */
	bool IsMelee() const { return GetMeleeData() != nullptr; }

	/** The inventory item this weapon comes from (the server, and the owner's machine) */
	UZItemInstance* GetItemInstance() const { return ItemInstance; }

	/** The mesh it shows (skeletal or static), or null if its data has none */
	UMeshComponent* GetWeaponMesh() const;

	/** Item used as reserve ammo, or null for unlimited reserve (or a melee weapon) */
	UZItemDefinition* GetAmmoItem() const;

	/** Rounds left in the magazine (only up to date on the server and the owning player's machine; 0 for a melee weapon) */
	int32 GetAmmo() const { return Ammo; }

	/** Rounds the magazine holds (0 for a melee weapon) */
	int32 GetMagazineSize() const;

	bool IsAutomatic() const;

	/** --- STATS: the weapon data, changed by the item's upgrade level and mods and by the traits of the character holding it --- */

	/** Damage per bullet, or per hit of a melee weapon (before an attack's own multiplier) */
	float GetDamage() const;

	/** Poise damage per bullet, or per hit of a melee weapon (before an attack's own multiplier) */
	float GetPoiseDamage() const;

	/** How fast a melee weapon's attack animations play (1 = as made); 1 for a gun */
	float GetAttackSpeed() const;

	/** What a melee weapon's attacks' stamina costs are multiplied by; 1 for a gun */
	float GetStaminaCostMultiplier() const;

	/** Shots per second */
	float GetFireRate() const;

	/** Seconds after a shot in which the shooter is committed (never longer than the time between shots) */
	float GetShotRecoveryTime() const;

	/** Spread cone in degrees */
	float GetSpread(bool bAiming) const;

	/** Camera kick per shot in degrees: X up, Y the most it goes sideways */
	FVector2D GetRecoil() const;

	/** How far shots reach, in cm */
	float GetRange() const;

	/** Seconds a reload takes */
	float GetReloadTime() const;

	/** An attribute of the character holding it, e.g. their damage multiplier; Default if there's none */
	float GetOwnerAttribute(const FGameplayAttribute& Attribute, float Default = 1.f) const;

	/** A stat of the weapon item: the data with its upgrade level and mods (no traits) */
	float GetItemStat(EZWeaponStat Stat) const;

	/** Where shots and muzzle flashes start: the muzzle socket, or the weapon's location if there's none */
	FVector GetMuzzleLocation() const;

	/** True if enough time has passed since the last shot for the fire rate. Tolerance below 1 accepts shots a bit early (network timing) */
	bool IsReadyToFire(float Tolerance = 1.f) const;

	/** Remembers when this machine fired, for the fire rate */
	void MarkFired();

	/** Server: uses one round */
	void ConsumeAmmo();

	/** Loads rounds into the magazine (up to its size): on the server, and on the owning player's machine so it can keep shooting before the server's count arrives */
	void AddAmmo(int32 Rounds);

	/** Server: if the magazine holds more than it can now (e.g. an extended magazine came off), the extra rounds go back to the reserve */
	void FitAmmoToMagazine();

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Clients: attach once the owner is known */
	virtual void OnRep_Owner() override;

	/** Server: the owner's inventory changed, maybe this weapon's item (e.g. a mod came off) */
	UFUNCTION()
	void OnInventoryChanged(UZInventoryComponent* Inventory);

	UFUNCTION()
	void OnRep_WeaponData();

	/** Sets the meshes from the data and attaches to the owner's hand */
	void ApplyWeaponData();

	/** Attaches to the owning character's mesh at the data's socket */
	void AttachToOwner();

	UPROPERTY(ReplicatedUsing = OnRep_WeaponData)
	TObjectPtr<UZWeaponDataBase> WeaponData;

	/** The item this weapon comes from; only the owner needs it (its upgrades and mods change the stats) */
	UPROPERTY(Replicated)
	TObjectPtr<UZItemInstance> ItemInstance;

	/** Rounds left in the magazine; only the owning player needs it */
	UPROPERTY(Replicated)
	int32 Ammo = 0;

	/** When this machine last fired */
	double LastFireTime = -1000.0;

	/** Server: the inventory holding the weapon's item, whose changes it listens to */
	TWeakObjectPtr<UZInventoryComponent> OwnerInventory;
};
