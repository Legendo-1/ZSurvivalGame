// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "ZSurvivalGameCharacter.h"
#include "ZCharacterMovementComponent.h"

UZGameplayAbility::UZGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// no abilities while dead or staggered (the stagger itself takes the staggered tag off its own list)
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(ZGameplayTags::State_Staggered);
}

void UZGameplayAbility::SetupAsAction(const FGameplayTag& Window)
{
	bWaitsWhileBusy = true;
	InterruptWindow = Window;
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Busy);
	AddAssetTag(ZGameplayTags::Ability_Type_Action);

	// started in another action's window, it takes over from that action
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability_Type_Action);
}

void UZGameplayAbility::AddAssetTag(const FGameplayTag& Tag)
{
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(Tag);
	SetAssetTags(Tags);
}

bool UZGameplayAbility::IsBlockedByCommitment(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}

	if (ASC->HasMatchingGameplayTag(ZGameplayTags::State_Staggered))
	{
		return true;
	}

	if (!ASC->HasMatchingGameplayTag(ZGameplayTags::State_Busy))
	{
		return false;
	}

	// the server only enforces staggers on a player's actions: when to act is up to the player's own machine,
	// whose timing differs a little from the server's (the server's animation runs a moment later)
	if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled())
	{
		return false;
	}

	return !UZCombatLibrary::IsActionWindowOpen(ASC, InterruptWindow);
}

bool UZGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (bWaitsWhileBusy && IsBlockedByCommitment(ActorInfo))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(ZGameplayTags::State_Busy);
		}

		return false;
	}

	return true;
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
