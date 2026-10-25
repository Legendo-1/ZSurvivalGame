// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayAbility.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZSurvivalGameCharacter.h"
#include "ZCharacterMovementComponent.h"

UZGameplayAbility::UZGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// no abilities while dead
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Dead);
}

AZSurvivalGameCharacter* UZGameplayAbility::GetZCharacter(const FGameplayAbilityActorInfo* ActorInfo)
{
	return ActorInfo ? Cast<AZSurvivalGameCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
}

UZCharacterMovementComponent* UZGameplayAbility::GetZMovement(const FGameplayAbilityActorInfo* ActorInfo)
{
	const AZSurvivalGameCharacter* Character = GetZCharacter(ActorInfo);
	return Character ? Character->GetZMovement() : nullptr;
}
