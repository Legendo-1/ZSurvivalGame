// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZEnemySpawner.generated.h"

class AZAIBaseCharacter;
class UBillboardComponent;

/**
 *  Place in a level to spawn enemies around it, on the server
 *  Keeps up to MaxAlive living enemies, one every SpawnInterval seconds
 */
UCLASS()
class AZEnemySpawner : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

#if WITH_EDITORONLY_DATA
	/** Icon in the editor */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Sprite;
#endif

public:

	AZEnemySpawner();

	/** Starts spawning (server only; already running if Spawn On Begin Play is set) */
	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void StartSpawning();

	UFUNCTION(BlueprintCallable, Category = "Spawning")
	void StopSpawning();

protected:

	virtual void BeginPlay() override;

	/** Removes dead enemies from the count and spawns one if there's room */
	void SpawnTick();

	/** A spot on the ground within SpawnRadius; false if there's no ground */
	bool FindSpawnLocation(FVector& OutLocation) const;

	/** What to spawn (a zombie by default) */
	UPROPERTY(EditAnywhere, Category = "Spawning")
	TSubclassOf<AZAIBaseCharacter> EnemyClass;

	/** Most living enemies from this spawner at once */
	UPROPERTY(EditAnywhere, Category = "Spawning", meta = (ClampMin = "0"))
	int32 MaxAlive = 5;

	/** Stops after spawning this many; 0 = no limit */
	UPROPERTY(EditAnywhere, Category = "Spawning", meta = (ClampMin = "0"))
	int32 TotalToSpawn = 0;

	/** Seconds between spawns */
	UPROPERTY(EditAnywhere, Category = "Spawning", meta = (ClampMin = "0.1"))
	float SpawnInterval = 4.f;

	/** Enemies appear within this distance of the spawner, in cm */
	UPROPERTY(EditAnywhere, Category = "Spawning", meta = (ClampMin = "0"))
	float SpawnRadius = 600.f;

	/** Start spawning when the game starts */
	UPROPERTY(EditAnywhere, Category = "Spawning")
	bool bSpawnOnBeginPlay = true;

	TArray<TWeakObjectPtr<AZAIBaseCharacter>> AliveEnemies;

	int32 SpawnedCount = 0;

	FTimerHandle SpawnTimer;
};
