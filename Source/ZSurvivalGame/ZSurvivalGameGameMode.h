// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZSurvivalGameGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 *  Respawns players a few seconds after they die
 */
UCLASS(abstract)
class AZSurvivalGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	AZSurvivalGameGameMode();

	/** Server: respawns the player at a Player Start after RespawnDelay seconds */
	void RequestRespawn(AController* Controller);

protected:

	/** Seconds between dying and respawning */
	UPROPERTY(EditDefaultsOnly, Category = "Respawn", meta = (ClampMin = "0"))
	float RespawnDelay = 5.f;

	/** Seconds the dead body stays after the player respawned */
	UPROPERTY(EditDefaultsOnly, Category = "Respawn", meta = (ClampMin = "0"))
	float DeadBodyLifeSpan = 10.f;

	/** Gives the player a new character */
	void Respawn(TWeakObjectPtr<AController> Controller);
};



