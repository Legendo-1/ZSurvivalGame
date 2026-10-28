// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/ZGameplayTags.h"

FZCharacterCues::FZCharacterCues()
	: HitCue(ZGameplayTags::GameplayCue_Character_Hit)
	, DeathCue(ZGameplayTags::GameplayCue_Character_Death)
	, StaggerCue(ZGameplayTags::GameplayCue_Character_Stagger)
{
}

bool UZGameplayCueLibrary::ExecuteCue(AActor* Target, FGameplayTag CueTag, const FGameplayCueParameters& Parameters)
{
	// players' ability systems are on their PlayerState; the cue still plays on the character
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);

	// a client can only play one inside a predicted ability (the ability system ignores it anywhere else)
	if (!ASC || !CueTag.IsValid() || (!ASC->IsOwnerActorAuthoritative() && !ASC->CanPredict()))
	{
		return false;
	}

	ASC->ExecuteGameplayCue(CueTag, Parameters);
	return true;
}

bool UZGameplayCueLibrary::AddCue(AActor* Target, FGameplayTag CueTag, const FGameplayCueParameters& Parameters)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!ASC || !CueTag.IsValid() || !ASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	ASC->AddGameplayCue(CueTag, Parameters);
	return true;
}

void UZGameplayCueLibrary::RemoveCue(AActor* Target, FGameplayTag CueTag)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (ASC && CueTag.IsValid() && ASC->IsOwnerActorAuthoritative())
	{
		ASC->RemoveGameplayCue(CueTag);
	}
}

FZCharacterCues UZGameplayCueLibrary::GetCharacterCues(const AActor* Character)
{
	const IZCharacterCuesInterface* CuesInterface = Cast<IZCharacterCuesInterface>(Character);
	return CuesInterface ? CuesInterface->GetCharacterCues() : FZCharacterCues();
}
