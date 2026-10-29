// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAICombatBrainComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "ZSurvivalGame.h"
#include "AI/ZAIBaseCharacter.h"
#include "AI/ZAIBaseController.h"
#include "AI/ZAICombatStates.h"
#include "AI/ZAITargetingComponent.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDrawAIDebug(
		TEXT("Z.AI.Debug"),
		false,
		TEXT("Draws each enemy's combat state and target over its head (host / server only)."),
		ECVF_Cheat);

	/** The height of an actor's feet (a character's capsule bottom) if it stands at Location */
	float GetFeetHeight(const AActor* Actor, const FVector& Location)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		return static_cast<float>(Location.Z) - (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f);
	}
}

UZAICombatBrainComponent::UZAICombatBrainComponent()
{
	// it thinks on a timer, not every frame
	PrimaryComponentTick.bCanEverTick = false;

	IdleState = CreateDefaultSubobject<UZAICombatState_Idle>(TEXT("IdleState"));
	ApproachingState = CreateDefaultSubobject<UZAICombatState_Approaching>(TEXT("ApproachingState"));
	StrafingState = CreateDefaultSubobject<UZAICombatState_Strafing>(TEXT("StrafingState"));
	AttackingState = CreateDefaultSubobject<UZAICombatState_Attacking>(TEXT("AttackingState"));
	DodgeRecoveringState = CreateDefaultSubobject<UZAICombatState_DodgeRecovering>(TEXT("DodgeRecoveringState"));
	StaggeredState = CreateDefaultSubobject<UZAICombatState_Staggered>(TEXT("StaggeredState"));
}

AZAIBaseController* UZAICombatBrainComponent::GetZController() const
{
	return Cast<AZAIBaseController>(GetOwner());
}

AZAIBaseCharacter* UZAICombatBrainComponent::GetCharacter() const
{
	const AZAIBaseController* Controller = GetZController();
	return Controller ? Cast<AZAIBaseCharacter>(Controller->GetPawn()) : nullptr;
}

UZAITargetingComponent* UZAICombatBrainComponent::GetTargeting() const
{
	const AZAIBaseController* Controller = GetZController();
	return Controller ? Controller->GetTargeting() : nullptr;
}

UZAICombatState* UZAICombatBrainComponent::GetStateObject(EZAICombatState State) const
{
	switch (State)
	{
	case EZAICombatState::Approaching:		return ApproachingState;
	case EZAICombatState::Strafing:			return StrafingState;
	case EZAICombatState::Attacking:		return AttackingState;
	case EZAICombatState::DodgeRecovering:	return DodgeRecoveringState;
	case EZAICombatState::Staggered:		return StaggeredState;
	default:								return IdleState;
	}
}

float UZAICombatBrainComponent::GetTimeInState() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - StateStartTime) : 0.f;
}

bool UZAICombatBrainComponent::HasStateTag(const FGameplayTag& Tag) const
{
	const AZAIBaseCharacter* Character = GetCharacter();
	const UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	return ASC && ASC->HasMatchingGameplayTag(Tag);
}

void UZAICombatBrainComponent::StartLogic()
{
	AZAIBaseCharacter* Character = GetCharacter();
	if (bRunning || !Character || !GetWorld())
	{
		return;
	}

	bRunning = true;
	bPaused = false;
	PendingState.Reset();
	NextAttackAllowedTime = 0.0;
	BindToCharacter();

	CurrentState = EZAICombatState::Idle;
	StateStartTime = GetWorld()->GetTimeSeconds();
	LastThinkTime = StateStartTime;
	IdleState->Enter(*this);
	Character->SetCombatState(CurrentState);

	// start a bit randomly, so many enemies don't all think in the same frame
	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &UZAICombatBrainComponent::Think, ThinkInterval, true, FMath::FRandRange(0.f, ThinkInterval));
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("AI '%s': combat brain started"), *GetNameSafe(Character));
}

void UZAICombatBrainComponent::RestartLogic()
{
	StopLogic(TEXT("restart"));
	StartLogic();
}

void UZAICombatBrainComponent::StopLogic(const FString& Reason)
{
	if (!bRunning)
	{
		return;
	}

	GetStateObject(CurrentState)->Exit(*this);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}

	if (AZAIBaseCharacter* Character = GetCharacter())
	{
		Character->SetCombatState(EZAICombatState::Idle);
		Character->SetCombatTarget(nullptr);
	}

	UnbindFromCharacter();
	bRunning = false;
	bPaused = false;
	PendingState.Reset();
	CurrentState = EZAICombatState::Idle;
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("AI '%s': combat brain stopped (%s)"), *GetNameSafe(GetOwner()), *Reason);
}

void UZAICombatBrainComponent::Cleanup()
{
	StopLogic(TEXT("cleanup"));
}

void UZAICombatBrainComponent::PauseLogic(const FString& Reason)
{
	bPaused = true;
}

EAILogicResuming::Type UZAICombatBrainComponent::ResumeLogic(const FString& Reason)
{
	const EAILogicResuming::Type Result = Super::ResumeLogic(Reason);
	bPaused = false;
	return Result;
}

void UZAICombatBrainComponent::BindToCharacter()
{
	AZAIBaseCharacter* Character = GetCharacter();
	UAbilitySystemComponent* ASC = Character ? Character->GetAbilitySystemComponent() : nullptr;
	if (ASC)
	{
		// a stagger or a dodge interrupts whatever it was doing, right away (not at the next decision step)
		StaggerTagHandle = ASC->RegisterGameplayTagEvent(ZGameplayTags::State_Staggered, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UZAICombatBrainComponent::OnStateTagChanged);
		DodgeTagHandle = ASC->RegisterGameplayTagEvent(ZGameplayTags::State_Dodging, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UZAICombatBrainComponent::OnStateTagChanged);
		BoundAbilitySystem = ASC;
	}

	if (UZAITargetingComponent* Targeting = GetTargeting())
	{
		TargetChangedHandle = Targeting->OnTargetChanged.AddUObject(this, &UZAICombatBrainComponent::OnTargetChanged);
	}
}

void UZAICombatBrainComponent::UnbindFromCharacter()
{
	if (UAbilitySystemComponent* ASC = BoundAbilitySystem.Get())
	{
		ASC->RegisterGameplayTagEvent(ZGameplayTags::State_Staggered, EGameplayTagEventType::NewOrRemoved).Remove(StaggerTagHandle);
		ASC->RegisterGameplayTagEvent(ZGameplayTags::State_Dodging, EGameplayTagEventType::NewOrRemoved).Remove(DodgeTagHandle);
	}

	if (UZAITargetingComponent* Targeting = GetTargeting())
	{
		Targeting->OnTargetChanged.Remove(TargetChangedHandle);
	}

	BoundAbilitySystem = nullptr;
	StaggerTagHandle.Reset();
	DodgeTagHandle.Reset();
	TargetChangedHandle.Reset();
}

void UZAICombatBrainComponent::OnStateTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!bRunning)
	{
		return;
	}

	if (NewCount > 0)
	{
		if (Tag == ZGameplayTags::State_Staggered)
		{
			ChangeState(EZAICombatState::Staggered, TEXT("poise broke"));
		}
		else if (Tag == ZGameplayTags::State_Dodging)
		{
			ChangeState(EZAICombatState::DodgeRecovering, TEXT("dodged"));
		}
	}
	else if (!bThinking)
	{
		// the stagger or dodge is over: decide what's next now
		Think();
	}
}

void UZAICombatBrainComponent::OnTargetChanged(AActor* NewTarget, AActor* OldTarget)
{
	// everyone sees whom it fights (e.g. for its head to follow the target)
	if (AZAIBaseCharacter* Character = GetCharacter())
	{
		Character->SetCombatTarget(NewTarget);
	}
}

void UZAICombatBrainComponent::ChangeState(EZAICombatState NewState, const TCHAR* Reason)
{
	if (!bRunning)
	{
		return;
	}

	if (bThinking)
	{
		PendingState = NewState;
		PendingReason = Reason;
		return;
	}

	SwitchState(NewState, Reason);
}

void UZAICombatBrainComponent::SwitchState(EZAICombatState NewState, const TCHAR* Reason)
{
	if (NewState == CurrentState)
	{
		return;
	}

	const EZAICombatState OldState = CurrentState;
	GetStateObject(OldState)->Exit(*this);

	CurrentState = NewState;
	StateStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	GetStateObject(NewState)->Enter(*this);

	AZAIBaseCharacter* Character = GetCharacter();
	if (Character)
	{
		Character->SetCombatState(NewState);
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("AI '%s': %s -> %s (%s)"), *GetNameSafe(Character), LexToString(OldState), LexToString(NewState), Reason);
}

void UZAICombatBrainComponent::Think()
{
	AZAIBaseCharacter* Character = GetCharacter();
	UZAITargetingComponent* Targeting = GetTargeting();
	UWorld* World = GetWorld();
	if (!bRunning || bPaused || !World)
	{
		return;
	}

	if (!Character || !Targeting || Character->IsDead())
	{
		StopLogic(TEXT("no living character"));
		return;
	}

	const double Now = World->GetTimeSeconds();
	const float DeltaTime = static_cast<float>(FMath::Max(Now - LastThinkTime, 0.0));
	LastThinkTime = Now;

	bThinking = true;

	Targeting->UpdateTarget();

	// what abilities do wins: staggered or dodging (also when it was missed as an event), or an attack that runs
	EZAICombatState NextState;
	const TCHAR* Reason = TEXT("decision");
	if (HasStateTag(ZGameplayTags::State_Staggered) && CurrentState != EZAICombatState::Staggered)
	{
		NextState = EZAICombatState::Staggered;
		Reason = TEXT("staggered");
	}
	else if (HasStateTag(ZGameplayTags::State_Dodging) && CurrentState != EZAICombatState::DodgeRecovering)
	{
		NextState = EZAICombatState::DodgeRecovering;
		Reason = TEXT("dodging");
	}
	else if (HasStateTag(ZGameplayTags::State_Attacking) && CurrentState != EZAICombatState::Attacking && CurrentState != EZAICombatState::Staggered)
	{
		NextState = EZAICombatState::Attacking;
		Reason = TEXT("attack running");
	}
	else
	{
		NextState = GetStateObject(CurrentState)->Update(*this, DeltaTime);
	}

	bThinking = false;

	// an event during the step (e.g. a stagger) wins over the step's own decision
	if (PendingState.IsSet())
	{
		NextState = PendingState.GetValue();
		Reason = PendingReason;
		PendingState.Reset();
	}

	SwitchState(NextState, Reason);

	if (CVarDrawAIDebug.GetValueOnGameThread())
	{
		DrawDebug();
	}
}

void UZAICombatBrainComponent::HandleMoveCompleted(bool bSuccess)
{
	if (bRunning && !bPaused)
	{
		GetStateObject(CurrentState)->OnMoveCompleted(*this, bSuccess);
	}
}

FZAICombatSnapshot UZAICombatBrainComponent::MakeSnapshot() const
{
	FZAICombatSnapshot Snapshot;
	const AZAIBaseCharacter* Character = GetCharacter();
	const UZAITargetingComponent* Targeting = GetTargeting();
	if (!Character || !Targeting)
	{
		return Snapshot;
	}

	const FZAICombatSettings& Settings = Character->GetCombatSettings();
	Snapshot.bHasTarget = Targeting->GetTarget() != nullptr;
	Snapshot.bTargetVisible = Targeting->IsTargetVisible();
	if (Snapshot.bHasTarget)
	{
		const FVector TargetLocation = Targeting->GetTargetLocation();
		Snapshot.DistanceToTarget = FVector::Dist2D(Character->GetActorLocation(), TargetLocation);
		Snapshot.HeightToTarget = GetFeetHeight(Targeting->GetTarget(), TargetLocation) - GetFeetHeight(Character, Character->GetActorLocation());
	}
	Snapshot.AttackRange = Character->GetAttackRange();
	Snapshot.MaxAttackHeight = Settings.MaxAttackHeight;
	Snapshot.bAttackReady = Character->IsAttackReady() && !UZCombatLibrary::IsBusy(Character) && !UZCombatLibrary::IsStaggered(Character);
	Snapshot.bCanStrafe = Settings.bCanStrafe;
	Snapshot.StrafeDistance = Settings.StrafeDistance;
	Snapshot.bStrafeTimeOver = !Settings.bCanStrafe || (GetWorld() && GetWorld()->GetTimeSeconds() >= NextAttackAllowedTime);
	return Snapshot;
}

void UZAICombatBrainComponent::StartStrafeTimer()
{
	const AZAIBaseCharacter* Character = GetCharacter();
	if (Character && GetWorld() && Character->GetCombatSettings().bCanStrafe)
	{
		const FZAICombatSettings& Settings = Character->GetCombatSettings();
		NextAttackAllowedTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(Settings.StrafeTimeMin, FMath::Max(Settings.StrafeTimeMin, Settings.StrafeTimeMax));
	}
}

bool UZAICombatBrainComponent::TryStartAttack()
{
	AZAIBaseCharacter* Character = GetCharacter();
	return Character && Character->TryAttack();
}

FString UZAICombatBrainComponent::GetDebugInfoString() const
{
	const UZAITargetingComponent* Targeting = GetTargeting();
	return FString::Printf(TEXT("Combat: %s for %.1f s%s; %s"), LexToString(CurrentState), GetTimeInState(), bRunning ? (bPaused ? TEXT(" (paused)") : TEXT("")) : TEXT(" (stopped)"),
		Targeting ? *Targeting->GetDebugString() : TEXT("no targeting"));
}

void UZAICombatBrainComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const AZAIBaseCharacter* Character = GetCharacter();
	const UZAITargetingComponent* Targeting = GetTargeting();
	if (!Character || !Targeting)
	{
		return;
	}

	const FColor Color = CurrentState == EZAICombatState::Attacking ? FColor::Red
		: CurrentState == EZAICombatState::Staggered ? FColor::Yellow
		: CurrentState == EZAICombatState::Idle ? FColor::White : FColor::Cyan;

	DrawDebugString(GetWorld(), Character->GetActorLocation() + FVector(0.f, 0.f, 120.f), FString::Printf(TEXT("%s\n%s"), LexToString(CurrentState), *GetNameSafe(Targeting->GetTarget())),
		nullptr, Color, ThinkInterval * 1.1f, true);

	if (Targeting->GetTarget())
	{
		DrawDebugLine(GetWorld(), Character->GetActorLocation(), Targeting->GetTargetLocation(), Targeting->IsTargetVisible() ? FColor::Green : FColor::Orange, false, ThinkInterval * 1.1f);
	}
#endif
}
