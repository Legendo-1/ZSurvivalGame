// Copyright Epic Games, Inc. All Rights Reserved.

#include "Weapons/ZWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemInstance.h"
#include "Weapons/ZWeaponData.h"
#include "Weapons/ZWeaponStats.h"

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

	// the owner works out the stats too (fire rate, spread, recoil, magazine), and those depend on the item's upgrades and mods
	DOREPLIFETIME_CONDITION(AZWeapon, ItemInstance, COND_OwnerOnly);
}

void AZWeapon::InitWeapon(UZWeaponData* InWeaponData, UZItemInstance* InItemInstance)
{
	WeaponData = InWeaponData;
	ItemInstance = InItemInstance;

	// mods can change the magazine while the weapon is in the hands
	OwnerInventory = UZInventoryComponent::FindInventoryComponent(GetOwner());
	if (OwnerInventory.IsValid())
	{
		OwnerInventory->OnInventoryChanged.AddUniqueDynamic(this, &AZWeapon::OnInventoryChanged);
	}

	// the item remembers its magazine between being put away and taken out again (a mod may have made the magazine smaller since)
	Ammo = ItemInstance ? ItemInstance->GetStatTagStackCount(ZGameplayTags::Item_Stat_MagazineAmmo) : GetMagazineSize();
	FitAmmoToMagazine();

	ApplyWeaponData();
}

void AZWeapon::SaveAmmoToItem()
{
	if (HasAuthority() && ItemInstance)
	{
		ItemInstance->SetStatTagStack(ZGameplayTags::Item_Stat_MagazineAmmo, Ammo);
	}
}

UZItemDefinition* AZWeapon::GetAmmoItem() const
{
	return WeaponData ? WeaponData->AmmoItem.Get() : nullptr;
}

int32 AZWeapon::GetMagazineSize() const
{
	return FMath::RoundToInt32(GetItemStat(EZWeaponStat::MagazineSize));
}

bool AZWeapon::IsAutomatic() const
{
	return WeaponData && WeaponData->bAutomatic;
}

float AZWeapon::GetDamage() const
{
	return GetItemStat(EZWeaponStat::Damage) * GetOwnerAttribute(UZAttributeSet::GetDamageMultiplierAttribute());
}

float AZWeapon::GetFireRate() const
{
	return WeaponData ? GetItemStat(EZWeaponStat::FireRate) : 1.f;
}

float AZWeapon::GetSpread(bool bAiming) const
{
	// mods change hip and aimed spread alike
	if (!WeaponData)
	{
		return 0.f;
	}

	return (bAiming ? WeaponData->AimSpread : WeaponData->HipSpread) * UZWeaponStatsLibrary::GetItemStatScale(WeaponData, ItemInstance, EZWeaponStat::Spread);
}

FVector2D AZWeapon::GetRecoil() const
{
	if (!WeaponData)
	{
		return FVector2D::ZeroVector;
	}

	// mods and the holder's traits change the kick up and sideways alike
	const float Scale = UZWeaponStatsLibrary::GetItemStatScale(WeaponData, ItemInstance, EZWeaponStat::Recoil) * GetOwnerAttribute(UZAttributeSet::GetRecoilMultiplierAttribute());
	return FVector2D(WeaponData->RecoilPitch, WeaponData->RecoilYaw) * Scale;
}

float AZWeapon::GetRange() const
{
	return GetItemStat(EZWeaponStat::Range);
}

float AZWeapon::GetReloadTime() const
{
	// the attribute set keeps the speed above 0
	return GetItemStat(EZWeaponStat::ReloadTime) / GetOwnerAttribute(UZAttributeSet::GetReloadSpeedMultiplierAttribute());
}

float AZWeapon::GetItemStat(EZWeaponStat Stat) const
{
	return UZWeaponStatsLibrary::GetWeaponStat(WeaponData, ItemInstance, Stat);
}

float AZWeapon::GetOwnerAttribute(const FGameplayAttribute& Attribute, float Default) const
{
	// the holding character's ability system (players: the one on their PlayerState)
	return UZAttributeSet::GetAttributeValue(GetOwner(), Attribute, Default);
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

	const double TimeBetweenShots = 1.0 / FMath::Max(GetFireRate(), 0.1f);
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

void AZWeapon::AddAmmo(int32 Rounds)
{
	// the server's count replicates to the owner; the owner's own count just shows it a moment earlier
	Ammo = FMath::Clamp(Ammo + Rounds, 0, GetMagazineSize());
}

void AZWeapon::FitAmmoToMagazine()
{
	const int32 Extra = Ammo - GetMagazineSize();
	if (!HasAuthority() || Extra <= 0)
	{
		return;
	}

	// the magazine first: giving the rounds back changes the inventory, which calls this again
	Ammo -= Extra;

	UZItemDefinition* AmmoItem = GetAmmoItem();
	if (OwnerInventory.IsValid() && AmmoItem)
	{
		const int32 NoRoom = Extra - OwnerInventory->AddItem(AmmoItem, Extra);
		if (NoRoom > 0)
		{
			OwnerInventory->DropOnGround(AmmoItem, NoRoom);
		}
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' magazine is smaller now: %d rounds back to the reserve"), *GetNameSafe(WeaponData), Extra);
}

void AZWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OwnerInventory.IsValid())
	{
		OwnerInventory->OnInventoryChanged.RemoveDynamic(this, &AZWeapon::OnInventoryChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void AZWeapon::OnInventoryChanged(UZInventoryComponent* Inventory)
{
	FitAmmoToMagazine();
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
