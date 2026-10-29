// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "ZAITypes.h"
#include "ZAITargetingComponent.generated.h"

class APawn;
class UAIPerceptionComponent;

/** How the AI weighs the enemies it knows about when it picks its target (higher score wins) */
USTRUCT(BlueprintType)
struct FZAITargetScoring
{
	GENERATED_BODY()

	/** Threat per point of damage an enemy dealt to it (threat fades, see below): whoever hurts it most draws it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0"))
	float ThreatPerDamage = 1.f;

	/** How fast threat fades, per second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0"))
	float ThreatDecayPerSecond = 5.f;

	/** Score lost per meter of distance: closer enemies win when nobody hurt it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0"))
	float DistancePenaltyPerMeter = 5.f;

	/** Score for an enemy it sees right now (over one it only remembers) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0"))
	float VisibleBonus = 20.f;

	/** Score for its current target, so it doesn't switch back and forth between two players at about the same score */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0"))
	float CurrentTargetBonus = 15.f;
};

/** An enemy the AI knows about: where it last was, how much it hurt the AI, whether it sees it now */
USTRUCT()
struct FZAIKnownEnemy
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> Actor;
	FVector LastKnownLocation = FVector::ZeroVector;
	double LastSensedTime = 0.0;
	float Threat = 0.f;
	bool bVisible = false;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FZOnAITargetChanged, AActor* /*NewTarget*/, AActor* /*OldTarget*/);

/**
 *  The AI's senses and memory (server only, on AZAIBaseController): which enemies it knows about and which one it fights
 *  - Listens to the controller's AI Perception: sight (its field of view, not through walls) and damage (whoever hit it, from anywhere)
 *  - Also notices enemies within Proximity Radius without seeing them (checked against the few players only)
 *  - Keeps a threat list: damage dealt to it adds threat, which fades; picks the best target a few times a second (closest, most threatening,
 *    in sight), with a bonus for the current one so it doesn't flicker between players in co-op
 *  - Forgets enemies it hasn't sensed for Forget Time
 *  Efficient: perception is event-driven and time-sliced by the engine; this only walks its own short list, a few times a second
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class UZAITargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZAITargetingComponent();

	/** Listens to this perception, with these settings (the controlled character's) */
	void Initialize(UAIPerceptionComponent* Perception, const FZAICombatSettings& InSettings);

	/** Forgets everything (e.g. the character died or was unpossessed) */
	void Reset();

	/** Updates threat and memory and picks the target (the brain calls it on its own beat) */
	void UpdateTarget();

	/** Adds threat from an enemy, e.g. one that hit it */
	void AddThreat(AActor* Enemy, float Amount);

	/** The enemy it fights, or null */
	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	AActor* GetTarget() const { return Target.Get(); }

	/** True if it sees its target right now (not only remembers it) */
	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	bool IsTargetVisible() const;

	/** Where its target is: where it is now if it's seen or close, otherwise where it was last sensed */
	UFUNCTION(BlueprintPure, Category = "AI|Targeting")
	FVector GetTargetLocation() const;

	/** Seconds since it last sensed its target in any way */
	float GetTimeSinceTargetSensed() const;

	/** How many enemies it knows about */
	int32 GetNumKnownEnemies() const { return KnownEnemies.Num(); }

	/** The target changed (server) */
	FZOnAITargetChanged OnTargetChanged;

	/**
	 *  The score of a known enemy (higher wins): its threat, plus the bonuses for being seen and for being the current target,
	 *  minus the penalty for its distance. Static and pure, for the tests
	 */
	static float ScoreEnemy(float DistanceCm, float Threat, bool bVisible, bool bIsCurrentTarget, const FZAITargetScoring& Scoring);

	/** "target 'X' 3.1 m away (seen), 2 known" for logs and the debugger */
	FString GetDebugString() const;

protected:

	/** Weights for picking the target */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Targeting")
	FZAITargetScoring Scoring;

private:

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Notices enemies (players) within Proximity Radius, seen or not */
	void SenseNearbyPlayers(double Now);

	/** True if this actor is something it should fight now: alive and on a hostile team */
	bool IsValidEnemy(const AActor* Actor) const;

	FZAIKnownEnemy* FindKnown(const AActor* Actor);
	const FZAIKnownEnemy* FindKnown(const AActor* Actor) const;
	FZAIKnownEnemy& FindOrAddKnown(AActor* Actor);

	void SetTarget(AActor* NewTarget);

	/** The pawn the controller drives */
	APawn* GetControlledPawn() const;

	UPROPERTY()
	TArray<FZAIKnownEnemy> KnownEnemies;

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UAIPerceptionComponent> PerceptionComponent;
	FZAICombatSettings Settings;
	double LastUpdateTime = 0.0;
};
