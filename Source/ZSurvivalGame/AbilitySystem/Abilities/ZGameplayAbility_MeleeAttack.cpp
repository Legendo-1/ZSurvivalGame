// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_MeleeAttack.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"
#include "AI/ZAIBaseCharacter.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"

UZGameplayAbility_MeleeAttack::UZGameplayAbility_MeleeAttack()
{
	// AI only runs on the server
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_MeleeAttack.GetTag()));
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Attacking);

	// an action: the character is busy while it swings, and a stagger cuts it short
	SetupAsAction(FGameplayTag());
}

void UZGameplayAbility_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const AZAIBaseCharacter* Attacker = Cast<AZAIBaseCharacter>(ActorInfo->AvatarActor.Get());
	if (!Attacker || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// attack animation (the ability system plays it on every machine)
	if (UAnimMontage* AttackMontage = Attacker->GetAttackMontage())
	{
		ActorInfo->AbilitySystemComponent->PlayMontage(this, ActivationInfo, AttackMontage, 1.f);
	}

	UAbilityTask_WaitDelay* WaitForHit = UAbilityTask_WaitDelay::WaitDelay(this, Attacker->GetMeleeHitDelay());
	WaitForHit->OnFinish.AddDynamic(this, &UZGameplayAbility_MeleeAttack::OnHitTime);
	WaitForHit->ReadyForActivation();
}

void UZGameplayAbility_MeleeAttack::OnHitTime()
{
	AZAIBaseCharacter* Attacker = Cast<AZAIBaseCharacter>(GetAvatarActorFromActorInfo());
	if (Attacker && !Attacker->IsDead())
	{
		// a sphere swept forward from the attacker hits the characters in front of it
		const FVector SweepStart = Attacker->GetActorLocation();
		const FVector SweepEnd = SweepStart + Attacker->GetActorForwardVector() * Attacker->GetMeleeReach();
		const FCollisionObjectQueryParams ObjectParams(ECC_Pawn);
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZMeleeAttack), false, Attacker);

		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Attacker->GetMeleeRadius()), QueryParams);

		// each character takes the hit once (ApplyDamage skips teammates)
		TSet<AActor*> AlreadyHit;
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && !AlreadyHit.Contains(HitActor))
			{
				AlreadyHit.Add(HitActor);
				UZCombatLibrary::ApplyDamage(Attacker, HitActor, Attacker->GetMeleeDamage(), Hit, Attacker->GetMeleePoiseDamage());
			}
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
