// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZSurvivalGameTypes.h"
#include "ZCharacterMovementComponent.generated.h"

/**
 *  Character movement that also syncs sprint, aim and speed limits with the server
 *  The owning client sets these states, predicts the speed and sends the states with every move;
 *  the server runs the move with the same states, so it agrees on the speed and doesn't pull the player back
 *  The speeds themselves are set on the character (Survival|Movement)
 */
UCLASS()
class UZCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	/** Set by the Sprint ability. On the server the client's moves overwrite it */
	void SetWantsToSprint(bool bNewWantsToSprint) { bWantsToSprint = bNewWantsToSprint; }
	bool WantsToSprint() const { return bWantsToSprint; }

	/** Set by the Aim ability. On the server the client's moves overwrite it */
	void SetWantsToAim(bool bNewWantsToAim) { bWantsToAim = bNewWantsToAim; }
	bool WantsToAim() const { return bWantsToAim; }

	/** Set on the owning client (or in a predicted ability). On the server the client's moves overwrite it */
	void SetSpeedLimit(EZSpeedLimit NewSpeedLimit) { SpeedLimit = NewSpeedLimit; }
	EZSpeedLimit GetSpeedLimit() const { return SpeedLimit; }

	/** True while moving at sprint speed: sprint held, not aiming, no speed limit and moving forward or sideways */
	bool IsSprinting() const;

	/** Speed for the current state, from the character's speeds */
	virtual float GetMaxSpeed() const override;

	/** Server: reads the states from the client's move */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/** Makes the client save our moves, which also hold the states */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:

	/** Turns the character toward the camera or toward the movement, before each move */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/** Replaying old moves after a server correction overwrites the states; this puts the current ones back */
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

private:

	bool bWantsToSprint = false;
	bool bWantsToAim = false;
	EZSpeedLimit SpeedLimit = EZSpeedLimit::None;
};
