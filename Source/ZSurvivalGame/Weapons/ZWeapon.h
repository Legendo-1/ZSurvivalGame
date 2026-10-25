// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZWeapon.generated.h"

class ACharacter;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UZWeaponData;

/**
 *  A weapon the character carries, spawned by the server from a UZWeaponData
 *  Every machine attaches it to its owner's hand; the server keeps the ammo
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

	/** Server: sets the weapon's data and fills the magazine */
	void InitWeapon(UZWeaponData* InWeaponData);

	UZWeaponData* GetWeaponData() const { return WeaponData; }

	/** Rounds left in the magazine (only up to date on the server and the owning player's machine) */
	int32 GetAmmo() const { return Ammo; }

	int32 GetMagazineSize() const;

	bool IsAutomatic() const;

	/** Where shots and muzzle flashes start: the muzzle socket, or the weapon's location if there's none */
	FVector GetMuzzleLocation() const;

	/** True if enough time has passed since the last shot for the fire rate. Tolerance below 1 accepts shots a bit early (network timing) */
	bool IsReadyToFire(float Tolerance = 1.f) const;

	/** Remembers when this machine fired, for the fire rate */
	void MarkFired();

	/** Server: uses one round */
	void ConsumeAmmo();

	/** Fills the magazine: on the server, and on the owning player's machine so it can keep shooting before the server's count arrives */
	void Refill();

	/** Shows the weapon in the hand, or hides it while holstered (server; replicates) */
	void SetEquipped(bool bEquipped);

protected:

	/** Clients: attach once the owner is known */
	virtual void OnRep_Owner() override;

	UFUNCTION()
	void OnRep_WeaponData();

	/** Sets the meshes from the data and attaches to the owner's hand */
	void ApplyWeaponData();

	/** Attaches to the owning character's mesh at the data's socket */
	void AttachToOwner();

	UPROPERTY(ReplicatedUsing = OnRep_WeaponData)
	TObjectPtr<UZWeaponData> WeaponData;

	/** Rounds left in the magazine; only the owning player needs it */
	UPROPERTY(Replicated)
	int32 Ammo = 0;

	/** When this machine last fired */
	double LastFireTime = -1000.0;
};
