// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGameGameMode.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGamePlayerState.h"

AZSurvivalGameGameMode::AZSurvivalGameGameMode()
{
	// the PlayerState holds each player's ability system, health and stamina
	PlayerStateClass = AZSurvivalGamePlayerState::StaticClass();
}

void AZSurvivalGameGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	FTimerHandle RespawnTimer;
	GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateUObject(this, &AZSurvivalGameGameMode::Respawn, TWeakObjectPtr<AController>(Controller)), FMath::Max(RespawnDelay, 0.01f), false);
}

void AZSurvivalGameGameMode::Respawn(TWeakObjectPtr<AController> Controller)
{
	AController* PlayerController = Controller.Get();
	if (!PlayerController)
	{
		return;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Respawning '%s'"), *GetNameSafe(PlayerController));

	// leave the dead body for a while, then spawn a new character at a Player Start
	if (APawn* DeadPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		DeadPawn->SetLifeSpan(DeadBodyLifeSpan);
	}

	RestartPlayer(PlayerController);
}
