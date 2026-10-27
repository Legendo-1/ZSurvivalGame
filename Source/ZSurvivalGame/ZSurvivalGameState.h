// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "ZSurvivalGameState.generated.h"

class AZSurvivalGameState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnWorldProgressChanged, AZSurvivalGameState*, GameState);

/**
 *  The game's shared state, on every machine
 *  Holds the world progress: what's been done in this world (bosses beaten, shortcuts opened, ...), as WorldProgress.* tags.
 *  It's the host's: it comes from the host's save when the game starts and is saved with it (see SAVING.md)
 */
UCLASS()
class AZSurvivalGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** True if this was done in the world, e.g. WorldProgress.Boss.StoneGolem once it's beaten */
	UFUNCTION(BlueprintPure, Category = "World Progress", meta = (GameplayTagFilter = "WorldProgress"))
	bool HasWorldProgress(FGameplayTag Progress) const { return WorldProgress.HasTagExact(Progress); }

	/** Everything done in the world */
	UFUNCTION(BlueprintPure, Category = "World Progress")
	const FGameplayTagContainer& GetWorldProgress() const { return WorldProgress; }

	/** Server: something was done for good, e.g. a boss beaten or a shortcut opened (saved with the host's game) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World Progress", meta = (GameplayTagFilter = "WorldProgress"))
	void AddWorldProgress(FGameplayTag Progress);

	/** Server: takes progress back, e.g. for testing */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "World Progress", meta = (GameplayTagFilter = "WorldProgress"))
	void RemoveWorldProgress(FGameplayTag Progress);

	/** Server: the progress from a save, when the game starts (only WorldProgress tags) */
	void SetWorldProgress(const FGameplayTagContainer& Progress);

	/** World progress changed (on every machine), e.g. to open a door that was opened before */
	UPROPERTY(BlueprintAssignable, Category = "World Progress")
	FZOnWorldProgressChanged OnWorldProgressChanged;

protected:

	UFUNCTION()
	void OnRep_WorldProgress();

private:

	UPROPERTY(ReplicatedUsing = OnRep_WorldProgress)
	FGameplayTagContainer WorldProgress;
};
