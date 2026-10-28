// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Reload.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameCharacter.h"
#include "Items/ZInventoryComponent.h"
#include "Weapons/ZWeapon.h"
#include "Weapons/ZRangedWeaponData.h"

UZGameplayAbility_Reload::UZGameplayAbility_Reload()
{
	InputID = EZAbilityInputID::Reload;
	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Reload.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Reloading);

	// not during another action (a shot's recovery, a dodge): it waits in the input buffer. It isn't an action itself, so a dodge can still cut it short
	bWaitsWhileBusy = true;
}

bool UZGameplayAbility_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// not full, and ammo in the inventory to load (or unlimited reserve)
	const AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo);
	return Character && Character->CanReloadEquippedWeapon();
}

void UZGameplayAbility_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo);
	AZWeapon* Weapon = Character ? Character->GetEquippedWeapon() : nullptr;
	const UZRangedWeaponData* WeaponData = Weapon ? Weapon->GetRangedData() : nullptr;
	if (!WeaponData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// jog at most while reloading (also cancels sprint); runs on both the player's machine and the server
	Character->SetSpeedLimit(EZSpeedLimit::Jog);

	// the player's reload speed (traits) included; the server and the player's machine have the same value
	const float ReloadTime = Weapon->GetReloadTime();
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' reloading, %.2f s"), *GetNameSafe(Character), ReloadTime);

	// character animation, stretched to the reload time (the ability system plays it for the others too)
	if (UAnimMontage* ReloadMontage = WeaponData->ReloadMontage)
	{
		const float PlayRate = ReloadTime > 0.f ? ReloadMontage->GetPlayLength() / ReloadTime : 1.f;
		ActorInfo->AbilitySystemComponent->PlayMontage(this, ActivationInfo, ReloadMontage, PlayRate);
	}

	// the weapon's reload cue: the player hears it at once, the server sends it to the others
	FGameplayCueParameters ReloadCue;
	ReloadCue.RawMagnitude = ReloadTime;
	ReloadCue.Instigator = Character;
	ReloadCue.EffectCauser = Weapon;
	ReloadCue.SourceObject = WeaponData;
	UZGameplayCueLibrary::ExecuteCue(Character, WeaponData->ReloadCue, ReloadCue);

	UAbilityTask_WaitDelay* WaitReload = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
	WaitReload->OnFinish.AddDynamic(this, &UZGameplayAbility_Reload::OnReloadFinished);
	WaitReload->ReadyForActivation();
}

void UZGameplayAbility_Reload::OnReloadFinished()
{
	const AZSurvivalGameCharacter* Character = GetZCharacter(CurrentActorInfo);
	if (AZWeapon* Weapon = Character ? Character->GetEquippedWeapon() : nullptr)
	{
		// load what's missing, as far as the reserve ammo in the inventory goes (-1 = unlimited reserve)
		const int32 Missing = Weapon->GetMagazineSize() - Weapon->GetAmmo();
		const int32 ReserveAmmo = Character->GetReserveAmmo();
		const int32 Loaded = ReserveAmmo < 0 ? Missing : FMath::Min(Missing, ReserveAmmo);

		// only the server takes the ammo from the inventory; both the player's machine and the server load the magazine,
		// so the player can keep shooting before the server's count arrives
		UZInventoryComponent* Inventory = Character->GetInventory();
		if (Loaded > 0 && ReserveAmmo >= 0 && Inventory && CurrentActorInfo->IsNetAuthority())
		{
			Inventory->RemoveItem(Weapon->GetAmmoItem(), Loaded);
		}

		Weapon->AddAmmo(Loaded);
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' reloaded %d, ammo: %d, reserve: %d"), *GetNameSafe(Character), Loaded, Weapon->GetAmmo(), Character->GetReserveAmmo());
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZGameplayAbility_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo))
	{
		Character->SetSpeedLimit(EZSpeedLimit::None);
	}

	// cancelled (dodge, weapon switch): stop the reload animation too
	if (bWasCancelled)
	{
		MontageStop();
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
