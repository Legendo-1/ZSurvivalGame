// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Fire.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZSurvivalGameCharacter.h"
#include "Weapons/ZWeapon.h"
#include "Weapons/ZRangedWeaponData.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDrawWeaponDebug(
		TEXT("Z.Weapon.DrawDebug"),
		true,
		TEXT("Draws shots (line) and hits (red point = damage dealt, yellow = something else) on the shooter's screen. 0 = off"));

	// the server accepts shots a bit faster than the fire rate, since they arrive with network timing
	constexpr float ServerFireRateTolerance = 0.5f;
}

UZGameplayAbility_Fire::UZGameplayAbility_Fire()
{
	InputID = EZAbilityInputID::Fire;
	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Fire.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Firing);

	// an action: commits the shooter for a moment, and may follow an attack in its combo window
	SetupAsAction(ZGameplayTags::Window_Combo);

	// no shooting while reloading or dodging
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Reloading);
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Dodging);

	// Firing cancels sprint
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability_Sprint);
}

AZWeapon* UZGameplayAbility_Fire::GetWeapon(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo);
	return Character ? Character->GetEquippedWeapon() : nullptr;
}

bool UZGameplayAbility_Fire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// only guns shoot (a melee weapon's attacks are its own abilities)
	const AZWeapon* Weapon = GetWeapon(ActorInfo);
	if (!Weapon || !Weapon->GetRangedData())
	{
		return false;
	}

	// keep the fire rate (with some slack on the server for shots from a client)
	const bool bShotFromClient = ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled();
	return Weapon->IsReadyToFire(bShotFromClient ? ServerFireRateTolerance : 1.f);
}

bool UZGameplayAbility_Fire::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const AZWeapon* Weapon = GetWeapon(ActorInfo);
	if (!Weapon || Weapon->GetAmmo() <= 0)
	{
		return false;
	}

	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UZGameplayAbility_Fire::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	// ammo only changes on the server; the new count replicates to the player
	if (AZWeapon* Weapon = GetWeapon(ActorInfo))
	{
		Weapon->ConsumeAmmo();
	}
}

void UZGameplayAbility_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// the server waits for the client's hits; the shooter's own machine makes them
	TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).AddUObject(this, &UZGameplayAbility_Fire::OnTargetDataReady);

	if (ActorInfo->IsLocallyControlled())
	{
		FireLocalShot();
	}
	else
	{
		// the hits may have arrived before the activation
		ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationInfo.GetActivationPredictionKey());
	}
}

void UZGameplayAbility_Fire::FireLocalShot()
{
	AZSurvivalGameCharacter* Character = GetZCharacter(CurrentActorInfo);
	AZWeapon* Weapon = GetWeapon(CurrentActorInfo);
	const UZRangedWeaponData* WeaponData = Weapon ? Weapon->GetRangedData() : nullptr;
	APlayerController* PlayerController = CurrentActorInfo->PlayerController.Get();
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!Character || !WeaponData || !PlayerController || !ASC)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	FScopedPredictionWindow ScopedPrediction(ASC, CurrentActivationInfo.GetActivationPredictionKey());

	// aim from the camera, but start level with the character so nothing between the camera and the character is hit
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector AimDirection = ViewRotation.Vector();
	const FVector TraceStart = ViewLocation + AimDirection * FVector::DotProduct(Character->GetActorLocation() - ViewLocation, AimDirection);

	const float HalfConeRadians = FMath::DegreesToRadians(Weapon->GetSpread(Character->IsAiming()) * 0.5f);
	const float Range = Weapon->GetRange();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZWeaponTrace), false, Character);
	QueryParams.AddIgnoredActor(Weapon);
	QueryParams.bReturnPhysicalMaterial = true;

	// one trace per bullet; misses are sent too, for the shot effects
	FGameplayAbilityTargetDataHandle TargetData;
	for (int32 BulletIndex = 0; BulletIndex < WeaponData->BulletsPerShot; ++BulletIndex)
	{
		const FVector ShotDirection = FMath::VRandCone(AimDirection, HalfConeRadians);
		const FVector TraceEnd = TraceStart + ShotDirection * Range;

		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ZTraceChannel_Weapon, QueryParams))
		{
			Hit = FHitResult(TraceStart, TraceEnd);
			Hit.Location = TraceEnd;
			Hit.ImpactPoint = TraceEnd;
		}

		TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	}

	// recoil: kick the camera up (and a little sideways)
	const FVector2D Recoil = Weapon->GetRecoil();
	FRotator ControlRotation = PlayerController->GetControlRotation();
	ControlRotation.Pitch += Recoil.X;
	ControlRotation.Yaw += FMath::FRandRange(-Recoil.Y, Recoil.Y);
	PlayerController->SetControlRotation(ControlRotation);

	// clients send the hits to the server
	if (!CurrentActorInfo->IsNetAuthority())
	{
		ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
	}

	OnTargetDataReady(TargetData, FGameplayTag());
}

void UZGameplayAbility_Fire::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& InTargetData, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	AZSurvivalGameCharacter* Character = GetZCharacter(CurrentActorInfo);
	AZWeapon* Weapon = GetWeapon(CurrentActorInfo);
	const UZRangedWeaponData* WeaponData = Weapon ? Weapon->GetRangedData() : nullptr;
	if (!ASC || !Character || !WeaponData)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// keep our own copy: consuming the data below frees the original
	const FGameplayAbilityTargetDataHandle TargetData = InTargetData;
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	// pays the round; fails if the magazine is empty (e.g. the client was a shot ahead of the server)
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	Weapon->MarkFired();

	const bool bIsServer = CurrentActorInfo->IsNetAuthority();
	const bool bDrawDebug = CVarDrawWeaponDebug.GetValueOnGameThread() && CurrentActorInfo->IsLocallyControlled();
	const FVector MuzzleLocation = Weapon->GetMuzzleLocation();

	// character animation (the ability system plays it for the others too)
	if (WeaponData->FireMontage)
	{
		ASC->PlayMontage(this, CurrentActivationInfo, WeaponData->FireMontage, 1.f);
	}

	for (int32 Index = 0; Index < TargetData.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Entry = TargetData.Get(Index);
		const FHitResult* Hit = Entry ? Entry->GetHitResult() : nullptr;
		if (!Hit)
		{
			continue;
		}

		const FVector ShotDirection = (Hit->TraceEnd - Hit->TraceStart).GetSafeNormal();

		// shot and impact effects (the weapon's own cues): the shooter plays them right away, the server sends them to everyone else
		if (Index == 0)
		{
			FGameplayCueParameters FireCue;
			FireCue.Location = MuzzleLocation;
			FireCue.Normal = ShotDirection;
			FireCue.Instigator = Character;
			FireCue.EffectCauser = Weapon;
			FireCue.SourceObject = WeaponData;
			UZGameplayCueLibrary::ExecuteCue(Character, WeaponData->FireCue, FireCue);
		}

		if (Hit->bBlockingHit)
		{
			FGameplayCueParameters ImpactCue;
			ImpactCue.Location = Hit->ImpactPoint;
			ImpactCue.Normal = Hit->ImpactNormal;
			ImpactCue.Instigator = Character;
			ImpactCue.EffectCauser = Weapon;
			ImpactCue.SourceObject = WeaponData;
			ImpactCue.PhysicalMaterial = Hit->PhysMaterial.Get();
			UZGameplayCueLibrary::ExecuteCue(Character, WeaponData->ImpactCue, ImpactCue);
		}

		// damage only on the server
		bool bDealtDamage = false;
		float Damage = 0.f;
		if (bIsServer && Hit->bBlockingHit && Hit->GetActor() && IsHitValid(*Hit, Weapon->GetRange()))
		{
			const bool bHeadshot = Hit->BoneName.ToString().Contains(TEXT("head"));
			Damage = Weapon->GetDamage() * (bHeadshot ? WeaponData->HeadshotMultiplier : 1.f);
			bDealtDamage = UZCombatLibrary::ApplyDamage(Character, Hit->GetActor(), Damage, *Hit, Weapon->GetPoiseDamage());

			// hit marker on the shooter's screen (runs right away on the host)
			if (bDealtDamage)
			{
				Character->ClientConfirmHit(UZCombatLibrary::IsDead(Hit->GetActor()));
			}
		}

		UE_LOG(LogZSurvivalGame, Verbose, TEXT("Shot by '%s' hit '%s' (bone %s), damage dealt: %s, ammo left: %d"),
			*GetNameSafe(Character), *GetNameSafe(Hit->GetActor()), *Hit->BoneName.ToString(), bDealtDamage ? *FString::SanitizeFloat(Damage) : TEXT("no"), Weapon->GetAmmo());

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			const FVector ShotEnd = Hit->bBlockingHit ? FVector(Hit->ImpactPoint) : FVector(Hit->TraceEnd);
			DrawDebugLine(GetWorld(), MuzzleLocation, ShotEnd, FColor::White, false, 1.f, 0, 0.5f);
			if (Hit->bBlockingHit)
			{
				// red: damage dealt (only known on the host); yellow: hit something
				DrawDebugPoint(GetWorld(), Hit->ImpactPoint, 8.f, bDealtDamage ? FColor::Red : FColor::Yellow, false, 1.f);
			}
		}
#endif
	}

	// the shooter's machine keeps them committed for the shot's recovery (a dodge pressed meanwhile waits for it);
	// the server lets go right away, so its own timing never holds back the player's next action
	const float RecoveryTime = Weapon->GetShotRecoveryTime();
	if (CurrentActorInfo->IsLocallyControlled() && RecoveryTime > 0.f)
	{
		UAbilityTask_WaitDelay* WaitRecovery = UAbilityTask_WaitDelay::WaitDelay(this, RecoveryTime);
		WaitRecovery->OnFinish.AddDynamic(this, &UZGameplayAbility_Fire::OnRecoveryFinished);
		WaitRecovery->ReadyForActivation();
		return;
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZGameplayAbility_Fire::OnRecoveryFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UZGameplayAbility_Fire::IsHitValid(const FHitResult& Hit, float Range) const
{
	// the hit has to be within the weapon's range of the shooter (with slack for movement between client and server)
	const AActor* Shooter = GetAvatarActorFromActorInfo();
	const float MaxDistance = Range + 500.f;
	return Shooter && FVector::DistSquared(Shooter->GetActorLocation(), Hit.ImpactPoint) <= FMath::Square(MaxDistance);
}

void UZGameplayAbility_Fire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegateHandle);
		ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
