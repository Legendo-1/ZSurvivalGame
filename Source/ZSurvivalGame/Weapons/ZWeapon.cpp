// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/ZWeaponData.h"

AZWeapon::AZWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	// replicate with the character that carries it
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// the meshes are only for looks; shots trace from the camera
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMeshComponent->SetupAttachment(Root);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMeshComponent->SetupAttachment(Root);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AZWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZWeapon, WeaponData);
	DOREPLIFETIME_CONDITION(AZWeapon, Ammo, COND_OwnerOnly);
}

void AZWeapon::InitWeapon(UZWeaponData* InWeaponData)
{
	WeaponData = InWeaponData;
	Refill();
	ApplyWeaponData();
}

int32 AZWeapon::GetMagazineSize() const
{
	return WeaponData ? WeaponData->MagazineSize : 0;
}

bool AZWeapon::IsAutomatic() const
{
	return WeaponData && WeaponData->bAutomatic;
}

FVector AZWeapon::GetMuzzleLocation() const
{
	const FName MuzzleSocket = WeaponData ? WeaponData->MuzzleSocket : NAME_None;

	if (SkeletalMeshComponent->DoesSocketExist(MuzzleSocket))
	{
		return SkeletalMeshComponent->GetSocketLocation(MuzzleSocket);
	}

	if (StaticMeshComponent->DoesSocketExist(MuzzleSocket))
	{
		return StaticMeshComponent->GetSocketLocation(MuzzleSocket);
	}

	return GetActorLocation();
}

bool AZWeapon::IsReadyToFire(float Tolerance) const
{
	if (!WeaponData)
	{
		return false;
	}

	const double TimeBetweenShots = 1.0 / WeaponData->FireRate;
	return GetWorld()->GetTimeSeconds() - LastFireTime >= TimeBetweenShots * Tolerance;
}

void AZWeapon::MarkFired()
{
	LastFireTime = GetWorld()->GetTimeSeconds();
}

void AZWeapon::ConsumeAmmo()
{
	if (HasAuthority())
	{
		Ammo = FMath::Max(0, Ammo - 1);
	}
}

void AZWeapon::Refill()
{
	// the server's count replicates to the owner; the owner's own refill just shows it a moment earlier
	Ammo = GetMagazineSize();
}

void AZWeapon::SetEquipped(bool bEquipped)
{
	SetActorHiddenInGame(!bEquipped);
}

void AZWeapon::OnRep_Owner()
{
	Super::OnRep_Owner();

	AttachToOwner();
}

void AZWeapon::OnRep_WeaponData()
{
	ApplyWeaponData();
}

void AZWeapon::ApplyWeaponData()
{
	SkeletalMeshComponent->SetSkeletalMeshAsset(WeaponData ? WeaponData->SkeletalMesh.Get() : nullptr);
	StaticMeshComponent->SetStaticMesh(WeaponData ? WeaponData->StaticMesh.Get() : nullptr);

	AttachToOwner();
}

void AZWeapon::AttachToOwner()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !WeaponData)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();
	const FName Socket = CharacterMesh->DoesSocketExist(WeaponData->AttachSocket) ? WeaponData->AttachSocket : FName(TEXT("hand_r"));

	AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	Root->SetRelativeTransform(WeaponData->AttachOffset);
}
