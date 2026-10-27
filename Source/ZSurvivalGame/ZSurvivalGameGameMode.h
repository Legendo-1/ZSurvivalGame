// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZSurvivalGameGameMode.generated.h"

class AZSurvivalGameState;
class UZPlayerSaveComponent;

/**
 *  Simple GameMode for a third person game
 *  Each player brings their character from the save on their own PC and spawns once it's in the game; the world comes from the host's save (see SAVING.md)
 *  Respawns players a few seconds after they die
 */
UCLASS(abstract)
class AZSurvivalGameGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	AZSurvivalGameGameMode();

	/** Puts the world progress from the host's save into the game state */
	virtual void InitGameState() override;

	/** Gets the player's character into the game first: the host's own from this PC's save right away, a joining player's when it arrives from theirs */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	/** Not before the player's character is in the game */
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;

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

	/** A player's character is in the game: they spawn */
	void HandlePlayerCharacterReady(UZPlayerSaveComponent* PlayerSave);

	/** Something was done in the world: the host's save (which holds the world) is written soon */
	UFUNCTION()
	void HandleWorldProgressChanged(AZSurvivalGameState* ChangedGameState);
};



