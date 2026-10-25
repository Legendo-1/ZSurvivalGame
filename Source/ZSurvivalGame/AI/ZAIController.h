// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ZAIController.generated.h"

class AZAICharacter;

/**
 *  Simple AI for AZAICharacter, on the server only: goes for the closest living player in aggro range,
 *  walks there (on the navmesh if the level has one, otherwise straight) and attacks when in range
 *  More complex behavior (patrols, fleeing, ranged attacks) can move to a StateTree later
 */
UCLASS()
class AZAIController : public AAIController
{
	GENERATED_BODY()

public:

	AZAIController();

protected:

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	/** Picks a target, chases it and attacks when close; runs every ThinkInterval seconds */
	void Think();

	/** The closest living player the character may attack, within its aggro range */
	APawn* FindTarget(const AZAICharacter* ControlledCharacter) const;

	/** Seconds between decisions */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.25f;

	FTimerHandle ThinkTimer;

	TWeakObjectPtr<APawn> CurrentTarget;
};
