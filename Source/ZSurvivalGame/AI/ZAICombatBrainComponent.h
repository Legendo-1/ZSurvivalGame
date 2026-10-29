// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BrainComponent.h"
#include "GameplayTagContainer.h"
#include "ZAITypes.h"
#include "ZAICombatBrainComponent.generated.h"

class AZAIBaseCharacter;
class AZAIBaseController;
class UAbilitySystemComponent;
class UZAICombatState;
class UZAITargetingComponent;

/**
 *  The AI's combat state machine (see AI.md), on the server on AZAIBaseController: the engine's brain component slot, like a behavior tree would be,
 *  so the AI controller starts and stops it and the gameplay debugger shows it
 *
 *  Every Think Interval (spread out randomly, so many enemies don't think in the same frame) it updates the target (UZAITargetingComponent)
 *  and runs the current state's object, which returns the next state. States that abilities drive win over that:
 *  a stagger (State.Staggered) or a dodge (State.Dodging) switch right away, and an attack (State.Attacking) as soon as one runs.
 *  The state is mirrored on the character (replicated), so animations on every machine can use it
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class UZAICombatBrainComponent : public UBrainComponent
{
	GENERATED_BODY()

public:

	UZAICombatBrainComponent();

	// UBrainComponent: the controller starts it on possess and cleans it up on unpossess
	virtual void StartLogic() override;
	virtual void RestartLogic() override;
	virtual void StopLogic(const FString& Reason) override;
	virtual void Cleanup() override;
	virtual void PauseLogic(const FString& Reason) override;
	virtual EAILogicResuming::Type ResumeLogic(const FString& Reason) override;
	virtual bool IsRunning() const override { return bRunning; }
	virtual bool IsPaused() const override { return bPaused; }
	virtual FString GetDebugInfoString() const override;

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	EZAICombatState GetCombatState() const { return CurrentState; }

	/** Seconds since the current state started */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	float GetTimeInState() const;

	/** Switches state now, for events from outside (the state objects return their next state instead). Waits for the end of a decision step if one runs */
	void ChangeState(EZAICombatState NewState, const TCHAR* Reason);

	/** A move finished (the controller passes it on) */
	void HandleMoveCompleted(bool bSuccess);

	/** --- For the state objects --- */

	AZAIBaseController* GetZController() const;
	AZAIBaseCharacter* GetCharacter() const;
	UZAITargetingComponent* GetTargeting() const;

	/** True if the character's ability system has this tag (e.g. State.Attacking) */
	bool HasStateTag(const FGameplayTag& Tag) const;

	/** What it knows right now, for the decision rules (ZAICombatRules) */
	FZAICombatSnapshot MakeSnapshot() const;

	/** Strafers: after an attack, circle for a random Strafe Time before the next one */
	void StartStrafeTimer();

	/** Tries to start an attack on the target; true if one started */
	bool TryStartAttack();

protected:

	/** Seconds between decision steps */
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.05", Units = "s"))
	float ThinkInterval = 0.2f;

	/** The states; give one a subclass in a Blueprint of the controller to change how an enemy behaves in it */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI|States")
	TObjectPtr<UZAICombatState> IdleState;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI|States")
	TObjectPtr<UZAICombatState> ApproachingState;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI|States")
	TObjectPtr<UZAICombatState> StrafingState;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI|States")
	TObjectPtr<UZAICombatState> AttackingState;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI|States")
	TObjectPtr<UZAICombatState> DodgeRecoveringState;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "AI|States")
	TObjectPtr<UZAICombatState> StaggeredState;

private:

	/** One decision step */
	void Think();

	/** Leaves the current state and enters the new one, and tells the character */
	void SwitchState(EZAICombatState NewState, const TCHAR* Reason);

	UZAICombatState* GetStateObject(EZAICombatState State) const;

	/** A stagger or dodge started or ended on the character */
	void OnStateTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** The targeting component picked a new target */
	void OnTargetChanged(AActor* NewTarget, AActor* OldTarget);

	void BindToCharacter();
	void UnbindFromCharacter();

	/** Draws the state over the enemy (console: Z.AI.Debug 1) */
	void DrawDebug() const;

	EZAICombatState CurrentState = EZAICombatState::Idle;
	double StateStartTime = 0.0;
	double LastThinkTime = 0.0;

	/** Strafers: when it may go in for the next attack */
	double NextAttackAllowedTime = 0.0;

	bool bRunning = false;
	bool bPaused = false;
	bool bThinking = false;

	/** A state change asked for during a decision step, applied at its end */
	TOptional<EZAICombatState> PendingState;
	const TCHAR* PendingReason = TEXT("");

	FTimerHandle ThinkTimer;
	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
	FDelegateHandle StaggerTagHandle;
	FDelegateHandle DodgeTagHandle;
	FDelegateHandle TargetChangedHandle;
};
