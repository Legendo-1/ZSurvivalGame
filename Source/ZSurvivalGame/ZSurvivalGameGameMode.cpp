// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGameGameMode.h"
#include "ZSurvivalGamePlayerState.h"

AZSurvivalGameGameMode::AZSurvivalGameGameMode()
{
	// the PlayerState holds each player's ability system, health and stamina
	PlayerStateClass = AZSurvivalGamePlayerState::StaticClass();
}
