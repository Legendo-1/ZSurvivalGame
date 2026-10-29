// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZSurvivalGameTypes.h"
#include "ZCharacterMovementComponent.generated.h"

/**
 *  What each of the player's moves tells the server beyond the engine's data: states that don't fit in the compressed flags
 *  (those four bits hold sprint, aim and the speed limit). A new state is a bit in ZFlags (see EZMoveFlag in the .cpp)
 */
struct FZCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
	typedef FCharacterNetworkMoveData Super;

	/** Our states as bits */
	uint8 ZFlags = 0;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};

/** Holds our move data for the moves sent together (new, pending and old) */
struct FZCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
	FZCharacterNetworkMoveDataContainer();

	FZCharacterNetworkMoveData ZMoveData[3];
};

/**
 *  Character movement that also syncs sprint, aim, speed limits and strafing (lock-on) with the server
 *  The owning client sets these states, predicts the speed and sends the states with every move;
 *  the server runs the move with the same states, so it agrees on the speed and facing and doesn't pull the player back
 *  The speeds themselves are set on the character (Survival|Movement)
 */
UCLASS()
class UZCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:

	UZCharacterMovementComponent();

	/** Set by the Sprint ability. On the server the client's moves overwrite it */
	void SetWantsToSprint(bool bNewWantsToSprint) { bWantsToSprint = bNewWantsToSprint; }
	bool WantsToSprint() const { return bWantsToSprint; }

	/** Set by the Aim ability. On the server the client's moves overwrite it */
	void SetWantsToAim(bool bNewWantsToAim) { bWantsToAim = bNewWantsToAim; }
	bool WantsToAim() const { return bWantsToAim; }

	/** Set on the owning client (or in a predicted ability). On the server the client's moves overwrite it */
	void SetSpeedLimit(EZSpeedLimit NewSpeedLimit) { SpeedLimit = NewSpeedLimit; }
	EZSpeedLimit GetSpeedLimit() const { return SpeedLimit; }

	/** Set by lock-on: the character faces the control rotation (the target) instead of turning toward its movement. On the server the client's moves overwrite it */
	void SetWantsToStrafe(bool bNewWantsToStrafe) { bWantsToStrafe = bNewWantsToStrafe; }
	bool WantsToStrafe() const { return bWantsToStrafe; }

	/**
	 *  Player's own machine: hit-stop, the swing almost stops for Duration seconds because it hit someone (the animation plays at Hit Stop Anim Rate).
	 *  It's a state of the moves like strafing, so the server slows the same moves and the attack stays in step on both
	 */
	void StartHitStop(float Duration);
	bool IsInHitStop() const { return bHitStop; }

	/** Our states as the bits of FZCharacterNetworkMoveData::ZFlags, and back (a saved move remembers them this way) */
	uint8 GetZFlags() const;
	void SetZFlags(uint8 ZFlags);

	/** True while moving at sprint speed: sprint held, not aiming, no speed limit and moving forward or sideways */
	bool IsSprinting() const;

	/** Speed for the current state, from the character's speeds */
	virtual float GetMaxSpeed() const override;

	/** Server: reads the states from the client's move */
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	/** Makes the client save our moves, which also hold the states */
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:

	/** Player's own machine: ends hit-stop when its time is up, before the move */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Turns the character toward the camera or toward the movement, and slows its animation during hit-stop, before each move */
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	/** The Attack Turn Rate during an attack's windup, else the normal rotation rate */
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;

	/** Replaying old moves after a server correction overwrites the states; this puts the current ones back */
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

	/** Server: reads our states from the player's move data before running the move */
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;

private:

	/** While Still (attacks), animation root motion only moves the character; its turning is dropped, so the attack goes where the camera looks */
	FTransform ProcessRootMotion(const FTransform& LocalRootMotion, UCharacterMovementComponent* Movement, float DeltaSeconds) const;

	/** True while an attack is in its windup: it turns toward the camera at the Attack Turn Rate */
	bool bAttackTurning = false;

	bool bWantsToSprint = false;
	bool bWantsToAim = false;
	bool bWantsToStrafe = false;
	EZSpeedLimit SpeedLimit = EZSpeedLimit::None;

	/** Hit-stop: on while the swing is slowed (a move state), until when (player's own machine), and whether we slowed the mesh's animation */
	bool bHitStop = false;
	double HitStopEndTime = 0.0;
	bool bAnimSlowedByHitStop = false;

	/** Where our move data lives while it's sent or received */
	FZCharacterNetworkMoveDataContainer MoveDataContainer;
};
