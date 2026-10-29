// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_MeleeAttack.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "ZSurvivalGame.h"
#include "AI/ZAIBaseCharacter.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Combat/ZAnimNotifyState_WeaponHitbox.h"
#include "Combat/ZMeleeTraceComponent.h"

UZGameplayAbility_MeleeAttack::UZGameplayAbility_MeleeAttack()
{
	// AI only runs on the server
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_MeleeAttack.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Attacking);

	// an action: the character is busy while it swings, and a stagger cuts it short
	SetupAsAction(FGameplayTag());
}

const FZEnemyAttack* UZGameplayAbility_MeleeAttack::GetAttack() const
{
	const AZAIBaseCharacter* Attacker = Cast<AZAIBaseCharacter>(GetAvatarActorFromActorInfo());
	return Attacker ? Attacker->GetAttack(AttackIndex) : nullptr;
}

void UZGameplayAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AZAIBaseCharacter* Attacker = Cast<AZAIBaseCharacter>(ActorInfo->AvatarActor.Get());
	AttackIndex = Attacker ? Attacker->GetCurrentAttackIndex() : INDEX_NONE;
	const FZEnemyAttack* Attack = GetAttack();
	if (!Attack || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// hitboxes in the montage find the hits along the weapon's swing; without any, one timed hit in front
	bUsesHitboxes = Attack->Montage && UZAnimNotifyState_WeaponHitbox::CountHitboxes(Attack->Montage) > 0;
	UZMeleeTraceComponent* Tracer = Attacker->FindComponentByClass<UZMeleeTraceComponent>();
	if (bUsesHitboxes && Tracer && Tracer->ShouldFindHits())
	{
		HitTracer = Tracer;
		MeleeHitDelegateHandle = Tracer->OnHit.AddUObject(this, &UZGameplayAbility_MeleeAttack::OnWeaponHit);
	}

	// the animation (the ability system plays it on every machine); the attack lasts as long as it does
	if (Attack->Montage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, Attack->Montage);
		MontageTask->OnBlendOut.AddDynamic(this, &UZGameplayAbility_MeleeAttack::OnMontageFinished);
		MontageTask->OnCompleted.AddDynamic(this, &UZGameplayAbility_MeleeAttack::OnMontageFinished);
		MontageTask->OnInterrupted.AddDynamic(this, &UZGameplayAbility_MeleeAttack::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UZGameplayAbility_MeleeAttack::OnMontageInterrupted);
		MontageTask->ReadyForActivation();
	}

	if (!bUsesHitboxes && IsActive())
	{
		UAbilityTask_WaitDelay* WaitForHit = UAbilityTask_WaitDelay::WaitDelay(this, Attack->HitDelay);
		WaitForHit->OnFinish.AddDynamic(this, &UZGameplayAbility_MeleeAttack::OnHitTime);
		WaitForHit->ReadyForActivation();
	}
}

void UZGameplayAbility_MeleeAttack::OnHitTime()
{
	AZAIBaseCharacter* Attacker = Cast<AZAIBaseCharacter>(GetAvatarActorFromActorInfo());
	const FZEnemyAttack* Attack = GetAttack();
	if (Attacker && Attack && !Attacker->IsDead())
	{
		// a sphere swept forward from the attacker hits the characters in front of it
		const FVector SweepStart = Attacker->GetActorLocation();
		const FVector SweepEnd = SweepStart + Attacker->GetActorForwardVector() * Attack->Reach;
		const FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZMeleeAttack), false, Attacker);

		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Attack->Radius), QueryParams);

		// each character takes the hit once (ApplyDamage skips teammates)
		TSet<AActor*> AlreadyHit;
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && !AlreadyHit.Contains(HitActor))
			{
				AlreadyHit.Add(HitActor);
				HitTarget(*Attacker, HitActor, Hit);
			}
		}
	}

	// without an animation there's nothing more to wait for
	if (!Attack || !Attack->Montage)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UZGameplayAbility_MeleeAttack::OnWeaponHit(const FHitResult& Hit)
{
	// the hitbox already skips teammates, the dead, and targets it touched before
	AZAIBaseCharacter* Attacker = Cast<AZAIBaseCharacter>(GetAvatarActorFromActorInfo());
	if (Attacker && IsActive() && !Attacker->IsDead())
	{
		HitTarget(*Attacker, Hit.GetActor(), Hit);
	}
}

void UZGameplayAbility_MeleeAttack::HitTarget(AZAIBaseCharacter& Attacker, AActor* Target, const FHitResult& Hit)
{
	const FZEnemyAttack* Attack = GetAttack();
	if (!Attack || !Target)
	{
		return;
	}

	const bool bDealtDamage = UZCombatLibrary::ApplyDamage(&Attacker, Target, Attack->Damage, Hit, Attack->PoiseDamage);
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' attack %d hit '%s' (%s, bone %s): damage %s, poise damage %.0f"), *GetNameSafe(&Attacker), AttackIndex + 1, *GetNameSafe(Target),
		bUsesHitboxes ? TEXT("hitbox") : TEXT("timed hit"), *Hit.BoneName.ToString(), bDealtDamage ? *FString::SanitizeFloat(Attack->Damage) : TEXT("none"), Attack->PoiseDamage);
}

void UZGameplayAbility_MeleeAttack::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZGameplayAbility_MeleeAttack::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UZGameplayAbility_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// only once: no more hits from this swing
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (UZMeleeTraceComponent* Tracer = HitTracer.Get())
		{
			Tracer->OnHit.Remove(MeleeHitDelegateHandle);
			Tracer->EndAllHitboxes();
		}

		HitTracer = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
