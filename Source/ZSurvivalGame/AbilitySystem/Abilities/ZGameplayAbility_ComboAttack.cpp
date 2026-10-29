// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_ComboAttack.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZSurvivalGame.h"
#include "ZCharacterMovementComponent.h"
#include "ZSurvivalGameCharacter.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Combat/ZAnimNotifyState_WeaponHitbox.h"
#include "Combat/ZCombatComponent.h"
#include "Combat/ZCombatSettings.h"
#include "Combat/ZMeleeTraceComponent.h"
#include "Weapons/ZMeleeMoveset.h"
#include "Weapons/ZMeleeWeaponData.h"
#include "Weapons/ZWeapon.h"

namespace
{
	const TCHAR* GetAttackTypeName(EZMeleeAttackType Type)
	{
		return Type == EZMeleeAttackType::Light ? TEXT("light") : TEXT("heavy");
	}
}

UZGameplayAbility_ComboAttack::UZGameplayAbility_ComboAttack()
{
	// one instance per attack: the next attack of the combo starts while this one is being cut short
	// (predicted abilities with an instance per activation must not replicate; nothing of theirs needs to)
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;

	ActivationOwnedTags.AddTag(ZGameplayTags::State_Attacking);

	// an action: commits the attacker; in its Combo Window the next attack cuts in, in its Dodge Window a dodge
	SetupAsAction(ZGameplayTags::Window_Combo);

	// attacking stops sprinting and aiming
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability_Sprint);
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability_Aim);
}

void UZGameplayAbility_ComboAttack::InitAttackType(EZMeleeAttackType InAttackType)
{
	AttackType = InAttackType;
	AddAssetTag(InAttackType == EZMeleeAttackType::Light ? ZGameplayTags::Ability_Attack_Light : ZGameplayTags::Ability_Attack_Heavy);

	// started by the combat component's event, which carries the attack's number in the combo
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = InAttackType == EZMeleeAttackType::Light ? ZGameplayTags::Event_Attack_Light : ZGameplayTags::Event_Attack_Heavy;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

UZGameplayAbility_LightAttack::UZGameplayAbility_LightAttack()
{
	InitAttackType(EZMeleeAttackType::Light);
}

UZGameplayAbility_HeavyAttack::UZGameplayAbility_HeavyAttack()
{
	InitAttackType(EZMeleeAttackType::Heavy);
}

AZWeapon* UZGameplayAbility_ComboAttack::GetMeleeWeapon(const FGameplayAbilityActorInfo* ActorInfo)
{
	const IZCombatantInterface* Combatant = ActorInfo ? Cast<IZCombatantInterface>(ActorInfo->AvatarActor.Get()) : nullptr;
	AZWeapon* Weapon = Combatant ? Combatant->GetHeldWeapon() : nullptr;
	return Weapon && Weapon->IsMelee() ? Weapon : nullptr;
}

UZCombatComponent* UZGameplayAbility_ComboAttack::GetCombatComponent(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar ? Avatar->FindComponentByClass<UZCombatComponent>() : nullptr;
}

const FZMeleeAttack* UZGameplayAbility_ComboAttack::FindAttack(const FGameplayAbilityActorInfo* ActorInfo, int32 InStep) const
{
	const AZWeapon* Weapon = GetMeleeWeapon(ActorInfo);
	const UZMeleeWeaponData* MeleeData = Weapon ? Weapon->GetMeleeData() : nullptr;
	return MeleeData && MeleeData->Moveset ? MeleeData->Moveset->GetAttack(AttackType, InStep) : nullptr;
}

bool UZGameplayAbility_ComboAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// a melee weapon whose moveset has attacks of this kind
	if (!FindAttack(ActorInfo, 0))
	{
		return false;
	}

	// no attacks in the air (yet); the server follows the player's machine here, which knows the moment of landing better
	const ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	return !ActorInfo->IsLocallyControlled() || !Movement || !Movement->IsFalling();
}

bool UZGameplayAbility_ComboAttack::ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const
{
	if (!Payload || !Super::ShouldAbilityRespondToEvent(ActorInfo, Payload))
	{
		return false;
	}

	const int32 RequestedStep = FMath::RoundToInt32(Payload->EventMagnitude);
	if (!FindAttack(ActorInfo, RequestedStep))
	{
		return false;
	}

	// a player's machine picks the attack; the server only takes a first attack, or the follow-up of the attack before (no skipping to the strongest)
	if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
	{
		const AZWeapon* Weapon = GetMeleeWeapon(ActorInfo);
		const UZCombatComponent* Combat = GetCombatComponent(ActorInfo);
		const bool bValid = Combat ? Combat->IsValidFollowUp(Weapon->GetMeleeData()->Moveset, AttackType, RequestedStep) : RequestedStep == 0;
		if (!bValid)
		{
			UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' %s attack %d refused: it can't follow the attack before"), *GetNameSafe(ActorInfo->AvatarActor.Get()), GetAttackTypeName(AttackType), RequestedStep + 1);
			return false;
		}
	}

	return true;
}

bool UZGameplayAbility_ComboAttack::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	// souls-like: any stamina left lets an attack start (it may take the last of it)
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || ASC->GetNumericAttribute(UZAttributeSet::GetStaminaAttribute()) <= 0.f)
	{
		return false;
	}

	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UZGameplayAbility_ComboAttack::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	// stamina only changes on the server; the new value replicates to the player
	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const AZWeapon* Weapon = GetMeleeWeapon(ActorInfo);
	const FZMeleeAttack* Attack = FindAttack(ActorInfo, Step);
	if (ASC && Weapon && Attack && HasAuthority(&ActivationInfo))
	{
		const FGameplayAttribute StaminaAttribute = UZAttributeSet::GetStaminaAttribute();
		const float Cost = Attack->StaminaCost * Weapon->GetStaminaCostMultiplier();
		ASC->SetNumericAttributeBase(StaminaAttribute, FMath::Max(ASC->GetNumericAttributeBase(StaminaAttribute) - Cost, 0.f));
	}
}

void UZGameplayAbility_ComboAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Step = TriggerEventData ? FMath::RoundToInt32(TriggerEventData->EventMagnitude) : 0;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const AZWeapon* Weapon = GetMeleeWeapon(ActorInfo);
	const FZMeleeAttack* Attack = FindAttack(ActorInfo, Step);
	if (!ASC || !Weapon || !Attack || !Attack->Montage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// remembered for the next attack of the combo
	if (UZCombatComponent* Combat = GetCombatComponent(ActorInfo))
	{
		Combat->NotifyAttackStarted(this, AttackType, Step);
	}

	// no walking during the swing; runs on the player's machine and the server (it reaches the server with the player's moves)
	if (AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo))
	{
		Character->SetSpeedLimit(EZSpeedLimit::Still);
	}

	// each target can take one hit per hitbox of the animation
	MaxHitsPerTarget = FMath::Max(1, UZAnimNotifyState_WeaponHitbox::CountHitboxes(Attack->Montage, Attack->Section));

	// the server waits for the hits a player's machine finds; the machine that finds the hits listens to the hitboxes
	if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
	{
		TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).AddUObject(this, &UZGameplayAbility_ComboAttack::OnServerTargetData);
	}

	AActor* Avatar = ActorInfo->AvatarActor.Get();
	UZMeleeTraceComponent* Tracer = Avatar ? Avatar->FindComponentByClass<UZMeleeTraceComponent>() : nullptr;
	if (Tracer && Tracer->ShouldFindHits())
	{
		HitTracer = Tracer;
		MeleeHitDelegateHandle = Tracer->OnHit.AddUObject(this, &UZGameplayAbility_ComboAttack::OnMeleeHit);
	}

	StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' %s attack %d: '%s'%s"), *GetNameSafe(Avatar), GetAttackTypeName(AttackType), Step + 1, *GetNameSafe(Attack->Montage),
		Attack->Section.IsNone() ? TEXT("") : *FString::Printf(TEXT(" section %s"), *Attack->Section.ToString()));

	// the animation, at the weapon's attack speed (the ability system plays it for the others too)
	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Attack->Montage, Weapon->GetAttackSpeed(), Attack->Section);
	MontageTask->OnBlendOut.AddDynamic(this, &UZGameplayAbility_ComboAttack::OnMontageFinished);
	MontageTask->OnCompleted.AddDynamic(this, &UZGameplayAbility_ComboAttack::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UZGameplayAbility_ComboAttack::OnMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(this, &UZGameplayAbility_ComboAttack::OnMontageInterrupted);
	MontageTask->ReadyForActivation();

	// one section of a montage that holds several attacks: it ends with its section instead of going on into the next one
	// (each machine that runs the attack sets it; the others follow the server's montage)
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (IsActive() && !Attack->Section.IsNone() && AnimInstance)
	{
		AnimInstance->Montage_SetNextSection(Attack->Section, NAME_None, Attack->Montage);
	}
}

void UZGameplayAbility_ComboAttack::OnMeleeHit(const FHitResult& Hit)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC || !IsActive())
	{
		return;
	}

	// hit-stop: the swing almost stops for a moment on the attacker's own machine; it rides in the moves, so the server slows the same moves
	if (CurrentActorInfo->IsLocallyControlled())
	{
		const FZMeleeAttack* HitAttack = FindAttack(CurrentActorInfo, Step);
		if (UZCharacterMovementComponent* Movement = Cast<UZCharacterMovementComponent>(CurrentActorInfo->MovementComponent.Get()))
		{
			Movement->StartHitStop(HitAttack ? HitAttack->HitStopTime : 0.f);
		}
	}

	// the server found it itself (the host's own attacks, AI)
	if (CurrentActorInfo->IsNetAuthority())
	{
		ApplyHit(Hit);
		return;
	}

	// a player's machine: the hit's sound and effect right away, and the hit to the server, which checks it and decides the damage
	FScopedPredictionWindow ScopedPrediction(ASC, true);

	const AZWeapon* Weapon = GetMeleeWeapon(CurrentActorInfo);
	const FZMeleeAttack* Attack = FindAttack(CurrentActorInfo, Step);
	ExecuteImpactCue(Hit, Weapon && Attack ? Weapon->GetDamage() * Attack->DamageMultiplier : 0.f);

	const FGameplayAbilityTargetDataHandle TargetData(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(), TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
}

void UZGameplayAbility_ComboAttack::OnServerTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return;
	}

	// keep our own copy: consuming the data frees the original
	const FGameplayAbilityTargetDataHandle TargetData = Data;
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	for (int32 Index = 0; Index < TargetData.Num(); ++Index)
	{
		const FGameplayAbilityTargetData* Entry = TargetData.Get(Index);
		if (const FHitResult* Hit = Entry ? Entry->GetHitResult() : nullptr)
		{
			ApplyHit(*Hit);
		}
	}
}

void UZGameplayAbility_ComboAttack::ApplyHit(const FHitResult& Hit)
{
	AActor* Attacker = GetAvatarActorFromActorInfo();
	const AZWeapon* Weapon = GetMeleeWeapon(CurrentActorInfo);
	const UZMeleeWeaponData* MeleeData = Weapon ? Weapon->GetMeleeData() : nullptr;
	const FZMeleeAttack* Attack = FindAttack(CurrentActorInfo, Step);
	AActor* Target = Hit.GetActor();
	if (!Attacker || !MeleeData || !Attack || !Target || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}

	// within the weapon's reach (a player's machine sees everyone a little out of date: some slack)
	const bool bFoundByPlayerMachine = !CurrentActorInfo->IsLocallyControlled();
	const float MaxDistance = MeleeData->MaxReach + (bFoundByPlayerMachine ? UZCombatSettings::Get()->MeleeReachTolerance : 0.f);
	const float Distance = FVector::Dist(Attacker->GetActorLocation(), Hit.ImpactPoint);
	if (Distance > MaxDistance)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Melee hit by '%s' on '%s' refused: too far (%.0f cm, at most %.0f)"), *GetNameSafe(Attacker), *GetNameSafe(Target), Distance, MaxDistance);
		return;
	}

	// each target once per hitbox of the attack
	int32& HitCount = HitCounts.FindOrAdd(Target);
	if (HitCount >= MaxHitsPerTarget)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Melee hit by '%s' on '%s' refused: this attack already hit it %d time(s)"), *GetNameSafe(Attacker), *GetNameSafe(Target), HitCount);
		return;
	}

	++HitCount;

	// the weapon's numbers (upgrades, mods and the attacker's traits included) times the attack's
	const float Damage = Weapon->GetDamage() * Attack->DamageMultiplier;
	const float PoiseDamage = Weapon->GetPoiseDamage() * Attack->PoiseDamageMultiplier;
	const bool bDealtDamage = UZCombatLibrary::ApplyDamage(Attacker, Target, Damage, Hit, PoiseDamage);

	// the hit's sound and effect for everyone (a player's own machine already played it)
	ExecuteImpactCue(Hit, Damage);

	// hit marker on the attacker's screen
	AZSurvivalGameCharacter* Character = Cast<AZSurvivalGameCharacter>(Attacker);
	if (bDealtDamage && Character)
	{
		Character->ClientConfirmHit(UZCombatLibrary::IsDead(Target));
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("Melee hit by '%s' (%s attack %d) on '%s' (bone %s): damage %s, poise damage %.1f"),
		*GetNameSafe(Attacker), GetAttackTypeName(AttackType), Step + 1, *GetNameSafe(Target), *Hit.BoneName.ToString(),
		bDealtDamage ? *FString::SanitizeFloat(Damage) : TEXT("none"), PoiseDamage);
}

void UZGameplayAbility_ComboAttack::ExecuteImpactCue(const FHitResult& Hit, float Damage) const
{
	AActor* Attacker = GetAvatarActorFromActorInfo();
	AZWeapon* Weapon = GetMeleeWeapon(CurrentActorInfo);
	const UZMeleeWeaponData* MeleeData = Weapon ? Weapon->GetMeleeData() : nullptr;
	if (!Attacker || !MeleeData)
	{
		return;
	}

	FGameplayCueParameters ImpactCue;
	ImpactCue.Location = Hit.ImpactPoint;
	ImpactCue.Normal = Hit.ImpactNormal;
	ImpactCue.RawMagnitude = Damage;
	ImpactCue.Instigator = Attacker;
	ImpactCue.EffectCauser = Weapon;
	ImpactCue.SourceObject = MeleeData;
	ImpactCue.PhysicalMaterial = Hit.PhysMaterial.Get();
	UZGameplayCueLibrary::ExecuteCue(Attacker, MeleeData->ImpactCue, ImpactCue);
}

void UZGameplayAbility_ComboAttack::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZGameplayAbility_ComboAttack::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UZGameplayAbility_ComboAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// only once (a late callback must not undo what the next attack set up)
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		// no more hits from this swing
		if (UZMeleeTraceComponent* Tracer = HitTracer.Get())
		{
			Tracer->OnHit.Remove(MeleeHitDelegateHandle);
			Tracer->EndAllHitboxes();
		}

		HitTracer = nullptr;

		if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
		}

		if (UZCombatComponent* Combat = GetCombatComponent(ActorInfo))
		{
			Combat->NotifyAttackEnded(this);
		}

		// walking again (an attack that cut this one short sets its own limit right after)
		if (AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo))
		{
			Character->SetSpeedLimit(EZSpeedLimit::None);
		}

		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' %s attack %d %s after %.2f s"), ActorInfo ? *GetNameSafe(ActorInfo->AvatarActor.Get()) : TEXT("?"), GetAttackTypeName(AttackType), Step + 1,
			bWasCancelled ? TEXT("cut short") : TEXT("ended"), GetWorld() ? GetWorld()->GetTimeSeconds() - StartTime : 0.0);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
