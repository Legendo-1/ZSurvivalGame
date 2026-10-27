// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGameState.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZGameplayTags.h"

void AZSurvivalGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// everyone sees what's been done in the world (e.g. an open shortcut)
	DOREPLIFETIME(AZSurvivalGameState, WorldProgress);
}

void AZSurvivalGameState::AddWorldProgress(FGameplayTag Progress)
{
	if (!HasAuthority() || !Progress.MatchesTag(ZGameplayTags::WorldProgress) || WorldProgress.HasTagExact(Progress))
	{
		return;
	}

	WorldProgress.AddTag(Progress);
	UE_LOG(LogZSurvivalGame, Log, TEXT("World progress: %s"), *Progress.ToString());

	// the server doesn't get the OnRep
	OnRep_WorldProgress();
}

void AZSurvivalGameState::RemoveWorldProgress(FGameplayTag Progress)
{
	if (HasAuthority() && WorldProgress.RemoveTag(Progress))
	{
		OnRep_WorldProgress();
	}
}

void AZSurvivalGameState::SetWorldProgress(const FGameplayTagContainer& Progress)
{
	if (!HasAuthority())
	{
		return;
	}

	WorldProgress = Progress.Filter(FGameplayTagContainer(ZGameplayTags::WorldProgress.GetTag()));
	OnRep_WorldProgress();
}

void AZSurvivalGameState::OnRep_WorldProgress()
{
	OnWorldProgressChanged.Broadcast(this);
}
