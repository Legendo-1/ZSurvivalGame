// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Reload.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameCharacter.h"
#include "Weapons/ZWeapon.h"
#include "Weapons/ZWeaponData.h"

UZGameplayAbility_Reload::UZGameplayAbility_Reload()
{
	InputID = EZAbilityInputID::Reload;
	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Reload.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Reloading);
}

bool UZGameplayAbility_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo);
	const AZWeapon* Weapon = Character ? Character->GetEquippedWeapon() : nullptr;
	return Weapon && Weapon->GetWeaponData() && Weapon->GetAmmo() < Weapon->GetMagazineSize();
}

void UZGameplayAbility_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo);
	const AZWeapon* Weapon = Character ? Character->GetEquippedWeapon() : nullptr;
	const UZWeaponData* WeaponData = Weapon ? Weapon->GetWeaponData() : nullptr;
	if (!WeaponData || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// jog at most while reloading (also cancels sprint); runs on both the player's machine and the server
	Character->SetSpeedLimit(EZSpeedLimit::Jog);

	// character animation, stretched to the reload time (the ability system plays it for the others too)
	if (UAnimMontage* ReloadMontage = WeaponData->ReloadMontage)
	{
		const float PlayRate = WeaponData->ReloadTime > 0.f ? ReloadMontage->GetPlayLength() / WeaponData->ReloadTime : 1.f;
		ActorInfo->AbilitySystemComponent->PlayMontage(this, ActivationInfo, ReloadMontage, PlayRate);
	}

	UAbilityTask_WaitDelay* WaitReload = UAbilityTask_WaitDelay::WaitDelay(this, WeaponData->ReloadTime);
	WaitReload->OnFinish.AddDynamic(this, &UZGameplayAbility_Reload::OnReloadFinished);
	WaitReload->ReadyForActivation();
}

void UZGameplayAbility_Reload::OnReloadFinished()
{
	// both the player's machine and the server fill the magazine; the server's count is the one that counts
	const AZSurvivalGameCharacter* Character = GetZCharacter(CurrentActorInfo);
	if (AZWeapon* Weapon = Character ? Character->GetEquippedWeapon() : nullptr)
	{
		Weapon->Refill();
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' reloaded, ammo: %d"), *GetNameSafe(Character), Weapon->GetAmmo());
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
