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
#include "Weapons/ZMeleeWeaponData.h"
#include "Weapons/ZRangedWeaponData.h"
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

	// the meshes are only for looks (shots trace from the camera); a melee weapon's hits are found along its blade sockets
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

void AZWeapon::InitWeapon(UZWeaponDataBase* InWeaponData, UZItemInstance* InItemInstance)
{
	WeaponData = InWeaponData;
	ItemInstance = InItemInstance;

	// mods can change the magazine while the weapon is in the hands
	OwnerInventory = UZInventoryComponent::FindInventoryComponent(GetOwner());
	if (OwnerInventory.IsValid())
	{
		OwnerInventory->OnInventoryChanged.AddUniqueDynamic(this, &AZWeapon::OnInventoryChanged);
	}

	// a gun's item remembers its magazine between being put away and taken out again (a mod may have made the magazine smaller since)
	if (GetRangedData())
	{
		Ammo = ItemInstance ? ItemInstance->GetStatTagStackCount(ZGameplayTags::Item_Stat_MagazineAmmo) : GetMagazineSize();
		FitAmmoToMagazine();
	}

	ApplyWeaponData();
}

const UZRangedWeaponData* AZWeapon::GetRangedData() const
{
	return Cast<UZRangedWeaponData>(WeaponData);
}

const UZMeleeWeaponData* AZWeapon::GetMeleeData() const
{
	return Cast<UZMeleeWeaponData>(WeaponData);
}

UMeshComponent* AZWeapon::GetWeaponMesh() const
{
	if (SkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		return SkeletalMeshComponent;
	}

	return StaticMeshComponent->GetStaticMesh() ? StaticMeshComponent : nullptr;
}

void AZWeapon::SaveAmmoToItem()
{
	if (HasAuthority() && ItemInstance && GetRangedData())
	{
		ItemInstance->SetStatTagStack(ZGameplayTags::Item_Stat_MagazineAmmo, Ammo);
	}
}

UZItemDefinition* AZWeapon::GetAmmoItem() const
{
	const UZRangedWeaponData* RangedData = GetRangedData();
	return RangedData ? RangedData->AmmoItem.Get() : nullptr;
}

int32 AZWeapon::GetMagazineSize() const
{
	return GetRangedData() ? FMath::RoundToInt32(GetItemStat(EZWeaponStat::MagazineSize)) : 0;
}

bool AZWeapon::IsAutomatic() const
{
	const UZRangedWeaponData* RangedData = GetRangedData();
	return RangedData && RangedData->bAutomatic;
}

float AZWeapon::GetDamage() const
{
	return GetItemStat(EZWeaponStat::Damage) * GetOwnerAttribute(UZAttributeSet::GetDamageMultiplierAttribute());
}

float AZWeapon::GetPoiseDamage() const
{
	return GetItemStat(EZWeaponStat::PoiseDamage);
}

float AZWeapon::GetAttackSpeed() const
{
	// mods can change it, but never stop the attacks
	return IsMelee() ? FMath::Max(GetItemStat(EZWeaponStat::AttackSpeed), 0.1f) : 1.f;
}

float AZWeapon::GetStaminaCostMultiplier() const
{
	return IsMelee() ? GetItemStat(EZWeaponStat::StaminaCost) : 1.f;
}

float AZWeapon::GetFireRate() const
{
	return GetRangedData() ? GetItemStat(EZWeaponStat::FireRate) : 1.f;
}

float AZWeapon::GetShotRecoveryTime() const
{
	// never longer than the time between shots, so it doesn't slow the fire rate down
	const UZRangedWeaponData* RangedData = GetRangedData();
	return RangedData ? FMath::Min(RangedData->ShotRecoveryTime, 1.f / FMath::Max(GetFireRate(), 0.1f)) : 0.f;
}

float AZWeapon::GetSpread(bool bAiming) const
{
	// mods change hip and aimed spread alike
	const UZRangedWeaponData* RangedData = GetRangedData();
	if (!RangedData)
	{
		return 0.f;
	}

	return (bAiming ? RangedData->AimSpread : RangedData->HipSpread) * UZWeaponStatsLibrary::GetItemStatScale(WeaponData, ItemInstance, EZWeaponStat::Spread);
}

FVector2D AZWeapon::GetRecoil() const
{
	const UZRangedWeaponData* RangedData = GetRangedData();
	if (!RangedData)
	{
		return FVector2D::ZeroVector;
	}

	// mods and the holder's traits change the kick up and sideways alike
	const float Scale = UZWeaponStatsLibrary::GetItemStatScale(WeaponData, ItemInstance, EZWeaponStat::Recoil) * GetOwnerAttribute(UZAttributeSet::GetRecoilMultiplierAttribute());
	return FVector2D(RangedData->RecoilPitch, RangedData->RecoilYaw) * Scale;
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
	const UZRangedWeaponData* RangedData = GetRangedData();
	const FName MuzzleSocket = RangedData ? RangedData->MuzzleSocket : NAME_None;

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
	if (!GetRangedData())
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
	if (!HasAuthority() || !GetRangedData() || Extra <= 0)
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
