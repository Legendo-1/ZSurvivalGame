// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZWeapon.generated.h"

class ACharacter;
struct FGameplayAttribute;
enum class EZWeaponStat : uint8;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UZInventoryComponent;
class UZItemDefinition;
class UZItemInstance;
class UZWeaponData;

/**
 *  The weapon in a character's hands, spawned by the server when a weapon item is equipped and removed when it's put away
 *  Every machine attaches it to its owner's hand; the server keeps the ammo, and writes it back to the item when the weapon is put away
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

	/** Server: sets the weapon's data and loads the magazine from its item (full if there's no item) */
	void InitWeapon(UZWeaponData* InWeaponData, UZItemInstance* InItemInstance);

	/** Server: stores the magazine ammo on the item, so it's kept when the weapon is put away or dropped */
	void SaveAmmoToItem();

	UZWeaponData* GetWeaponData() const { return WeaponData; }

	/** The inventory item this weapon comes from (the server, and the owner's machine) */
	UZItemInstance* GetItemInstance() const { return ItemInstance; }

	/** Item used as reserve ammo, or null for unlimited reserve */
	UZItemDefinition* GetAmmoItem() const;

	/** Rounds left in the magazine (only up to date on the server and the owning player's machine) */
	int32 GetAmmo() const { return Ammo; }

	int32 GetMagazineSize() const;

	bool IsAutomatic() const;

	/** --- STATS: the weapon data, changed by the item's upgrade level and mods and by the traits of the character holding it --- */

	/** Damage per bullet */
	float GetDamage() const;

	/** Shots per second */
	float GetFireRate() const;

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
	TObjectPtr<UZWeaponData> WeaponData;

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
