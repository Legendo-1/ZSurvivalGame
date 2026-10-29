// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "ZAIBaseController.generated.h"

class UAISenseConfig_Damage;
class UAISenseConfig_Sight;
class UZAICombatBrainComponent;
class UZAITargetingComponent;
struct FZAICombatSettings;

/**
 *  The AI of every AZAIBaseCharacter (zombies, monsters, human NPCs), on the server only (see AI.md)
 *  - AI Perception: sight (the character's field of view, not through walls) and damage (it knows who hit it)
 *  - Targeting (UZAITargetingComponent): which enemies it knows about and which one it fights (threat, distance, sight)
 *  - Combat brain (UZAICombatBrainComponent): the combat state machine (Idle, Approaching, Strafing, Attacking, DodgeRecovering, Staggered)
 *  The numbers come from the controlled character (its Combat Settings), so one controller class serves every enemy type.
 *  For an enemy that behaves differently in a state, make a Blueprint of this controller, give that state another class (Combat Brain → States),
 *  and pick the Blueprint as the enemy's AI Controller Class
 *  Nothing here replicates: the character replicates its movement, combat state and target, and its abilities their animations
 */
UCLASS()
class AZAIBaseController : public AAIController
{
	GENERATED_BODY()

public:

	AZAIBaseController(const FObjectInitializer& ObjectInitializer);

	/** Which enemies it knows about and which one it fights */
	UZAITargetingComponent* GetTargeting() const { return Targeting; }

	/** The combat state machine */
	UZAICombatBrainComponent* GetCombatBrain() const { return CombatBrain; }

	/** --- Movement, for the brain's states --- */

	/** Goes toward an actor until its middle is AcceptanceRadius from the actor's (on the navmesh, or straight if the level has none); leaves a move toward the same actor running */
	void MoveTowardActor(AActor* Goal, float AcceptanceRadius);

	/** Goes to a spot (on the navmesh, or straight). RequestSuccessful: on its way; AlreadyAtGoal: there; Failed: it can't */
	EPathFollowingRequestResult::Type MoveTowardLocation(const FVector& Location, float AcceptanceRadius);

	/** Stops the current move */
	void StopMoving();

	/** Reads the character's Combat Settings again (senses and targeting), e.g. after they changed during play */
	void RefreshCombatSettings();

	/** Sets the character's walking speed (server: AI movement isn't predicted, everyone gets the result) */
	void SetMoveSpeed(float Speed);

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:

	/** Takes the character's team and senses, then starts the brain */
	virtual void OnPossess(APawn* InPawn) override;

	/** Forgets its targets (the engine stops the brain) */
	virtual void OnUnPossess() override;

	/** Sets sight from the character's Combat Settings */
	void ConfigurePerception(const FZAICombatSettings& Settings);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UZAITargetingComponent> Targeting;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UZAICombatBrainComponent> CombatBrain;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

private:

	/** True while it asks for a move: a move that ends during the request (already there, or no way) is answered by the request's result, not as an ended move */
	bool bRequestingMove = false;
};
