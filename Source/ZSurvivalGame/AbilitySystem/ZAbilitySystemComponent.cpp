// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueManager.h"
#include "GameFramework/PlayerState.h"

void UZAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	// the engine starts with the owner as the body. A PlayerState has none, so until the character is there cues have nothing
	// to play on (instead of playing at the world's origin, or starting a loop there that never stops)
	if (Cast<APlayerState>(InAvatarActor))
	{
		InAvatarActor = nullptr;
	}

	// the actor info, not the replicated AvatarActor: on clients that one already holds the new body here
	AActor* OldAvatar = AbilityActorInfo.IsValid() ? AbilityActorInfo->AvatarActor.Get() : nullptr;
	const bool bAvatarChanged = InAvatarActor != OldAvatar;

	// lasting cues move with the body (the engine only does this for gameplay effects' cues, onto a first body)
	if (bAvatarChanged)
	{
		InvokeActiveCues(OldAvatar, EGameplayCueEvent::Removed);
	}

	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (bAvatarChanged)
	{
		InvokeActiveCues(InAvatarActor, EGameplayCueEvent::WhileActive);
	}
}

void UZAbilitySystemComponent::InvokeActiveCues(AActor* Target, EGameplayCueEvent::Type EventType)
{
	if (!IsValid(Target) || bSuppressGameplayCues)
	{
		return;
	}

	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	for (const FActiveGameplayCue& Cue : ActiveGameplayCues.GameplayCues)
	{
		if (!Cue.bPredictivelyRemoved)
		{
			CueManager->HandleGameplayCue(Target, Cue.GameplayCueTag, EventType, Cue.Parameters);
		}
	}
}
