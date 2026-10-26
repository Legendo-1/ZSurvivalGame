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

	/** A free Player Start, or else any Player Start even if it's crowded (the default would fall back to the world origin) */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Spawns the player's character, moved to a free spot nearby if the start is crowded (the default gives up) */
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;

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



