// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ZCheatManager.generated.h"

/**
 *  Console commands for testing (open the console with the ` key; not in shipping builds)
 *  They change the game on the host, so in multiplayer type them in the host's (Server) window
 */
UCLASS()
class UZCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:

	/** Spawns zombies 8 m in front of the camera, side by side */
	UFUNCTION(Exec)
	void ZSpawnZombies(int32 Count);

	/** Kills every AI character in the level */
	UFUNCTION(Exec)
	void ZKillEnemies();

	/** Fills your health and stamina */
	UFUNCTION(Exec)
	void ZHeal();

private:

	/** True on the host (or in single player); otherwise logs that the command only works there */
	bool CheckIsHost(const TCHAR* CommandName) const;
};
