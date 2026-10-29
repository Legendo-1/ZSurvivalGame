// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ZAITypes.h"
#include "ZAICombatStates.generated.h"

class UZAICombatBrainComponent;

/**
 *  One state of the AI's combat state machine (see UZAICombatBrainComponent and AI.md): what the enemy does while in it, and when it leaves
 *  The brain owns one object per state. To change how an enemy type behaves in a state, make a subclass and pick it for that state
 *  (a Blueprint of AZAIBaseController → Combat Brain → States), so the other states stay as they are
 *  Runs on the server only, like the whole AI
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UZAICombatState : public UObject
{
	GENERATED_BODY()

public:

	/** The state starts: set up movement, focus, timers */
	virtual void Enter(UZAICombatBrainComponent& Brain) {}

	/** The state ends (another one starts right after) */
	virtual void Exit(UZAICombatBrainComponent& Brain) {}

	/** A decision step: do this state's work and return the state to be in next (this one to stay) */
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) PURE_VIRTUAL(UZAICombatState::Update, return EZAICombatState::Idle;);

	/** A move this state asked for finished (reached the spot, or got blocked) */
	virtual void OnMoveCompleted(UZAICombatBrainComponent& Brain, bool bSuccess) {}
};

/** No target: stands still and waits to sense an enemy */
UCLASS()
class UZAICombatState_Idle : public UZAICombatState
{
	GENERATED_BODY()

public:

	virtual void Enter(UZAICombatBrainComponent& Brain) override;
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) override;
};

/** Goes toward the target at full speed, facing it; attacks once in range and ready, starts circling (strafers) while it has to wait */
UCLASS()
class UZAICombatState_Approaching : public UZAICombatState
{
	GENERATED_BODY()

public:

	virtual void Enter(UZAICombatBrainComponent& Brain) override;
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) override;

protected:

	/** Stops this much short of the attack range, so it's surely in range when it arrives, in cm */
	UPROPERTY(EditAnywhere, Category = "Approaching", meta = (ClampMin = "0", Units = "cm"))
	float StopShortOfRange = 30.f;
};

/** Circles the target at Strafe Distance, facing it, at Strafe Speed; turns around when blocked */
UCLASS()
class UZAICombatState_Strafing : public UZAICombatState
{
	GENERATED_BODY()

public:

	virtual void Enter(UZAICombatBrainComponent& Brain) override;
	virtual void Exit(UZAICombatBrainComponent& Brain) override;
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) override;
	virtual void OnMoveCompleted(UZAICombatBrainComponent& Brain, bool bSuccess) override;

protected:

	/** How far around the target each step of the circle goes, in degrees */
	UPROPERTY(EditAnywhere, Category = "Strafing", meta = (ClampMin = "5", ClampMax = "90", Units = "deg"))
	float StepAngle = 35.f;

	/** How close to each spot on the circle counts as there, in cm */
	UPROPERTY(EditAnywhere, Category = "Strafing", meta = (ClampMin = "10", Units = "cm"))
	float AcceptanceRadius = 40.f;

	/** Chance per decision step to turn around on its own, so it doesn't circle like a clock */
	UPROPERTY(EditAnywhere, Category = "Strafing", meta = (ClampMin = "0", ClampMax = "1"))
	float TurnAroundChance = 0.03f;

private:

	/** Moves to the next spot on the circle */
	void StepAround(UZAICombatBrainComponent& Brain);

	/** +1: counterclockwise around the target, -1: clockwise */
	float Direction = 1.f;
	bool bMoving = false;
};

/** An attack ability runs: stands and faces the target (the ability does the rest) until it's over */
UCLASS()
class UZAICombatState_Attacking : public UZAICombatState
{
	GENERATED_BODY()

public:

	virtual void Enter(UZAICombatBrainComponent& Brain) override;
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) override;
};

/** After a dodge: stands still for Dodge Recovery Time (counted from the end of the dodge), then goes on */
UCLASS()
class UZAICombatState_DodgeRecovering : public UZAICombatState
{
	GENERATED_BODY()

public:

	virtual void Enter(UZAICombatBrainComponent& Brain) override;
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) override;

private:

	double RecoveredTime = 0.0;
};

/** Its poise broke: doesn't move or turn until the stagger ability is over */
UCLASS()
class UZAICombatState_Staggered : public UZAICombatState
{
	GENERATED_BODY()

public:

	virtual void Enter(UZAICombatBrainComponent& Brain) override;
	virtual EZAICombatState Update(UZAICombatBrainComponent& Brain, float DeltaTime) override;
};
