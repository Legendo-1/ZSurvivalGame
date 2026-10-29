// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAICombatStates.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/ZAIBaseCharacter.h"
#include "AI/ZAIBaseController.h"
#include "AI/ZAICombatBrainComponent.h"
#include "AI/ZAITargetingComponent.h"
#include "AbilitySystem/ZGameplayTags.h"

// --- Idle ---

void UZAICombatState_Idle::Enter(UZAICombatBrainComponent& Brain)
{
	if (AZAIBaseController* Controller = Brain.GetZController())
	{
		Controller->StopMoving();
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
		Controller->SetMoveSpeed(Brain.GetCharacter() ? Brain.GetCharacter()->GetDefaultWalkSpeed() : 0.f);
	}
}

EZAICombatState UZAICombatState_Idle::Update(UZAICombatBrainComponent& Brain, float DeltaTime)
{
	const UZAITargetingComponent* Targeting = Brain.GetTargeting();
	return Targeting && Targeting->GetTarget() ? EZAICombatState::Approaching : EZAICombatState::Idle;
}

// --- Approaching ---

void UZAICombatState_Approaching::Enter(UZAICombatBrainComponent& Brain)
{
	AZAIBaseController* Controller = Brain.GetZController();
	const AZAIBaseCharacter* Character = Brain.GetCharacter();
	if (Controller && Character)
	{
		Controller->SetMoveSpeed(Character->GetDefaultWalkSpeed());
	}
}

EZAICombatState UZAICombatState_Approaching::Update(UZAICombatBrainComponent& Brain, float DeltaTime)
{
	AZAIBaseController* Controller = Brain.GetZController();
	const AZAIBaseCharacter* Character = Brain.GetCharacter();
	const UZAITargetingComponent* Targeting = Brain.GetTargeting();
	if (!Controller || !Character || !Targeting)
	{
		return EZAICombatState::Idle;
	}

	const FZAICombatSnapshot Snapshot = Brain.MakeSnapshot();
	const EZAICombatState Next = ZAICombatRules::WhileApproaching(Snapshot);
	if (Next == EZAICombatState::Attacking)
	{
		// the attack ability runs right away (server only), or something blocked it: then it waits here
		if (Brain.TryStartAttack(Snapshot.DistanceToTarget))
		{
			return EZAICombatState::Attacking;
		}
	}
	else if (Next != EZAICombatState::Approaching)
	{
		return Next;
	}

	// it faces the target while it senses it, otherwise the spot where it last did (no looking through walls)
	AActor* Target = Targeting->GetTarget();
	const bool bSensedNow = Targeting->IsTargetVisible() || Targeting->GetTimeSinceTargetSensed() < 0.5f;
	if (bSensedNow)
	{
		Controller->SetFocus(Target);
	}
	else
	{
		Controller->SetFocalPoint(Targeting->GetTargetLocation());
	}

	// in range: hold its ground facing the target (and attack once ready); otherwise go to it, or to where it was last sensed
	// (a target right above it on a ledge isn't in range: on the navmesh it walks around and up to it)
	if (ZAICombatRules::IsInAttackRange(Snapshot))
	{
		Controller->StopMoving();
	}
	else if (bSensedNow)
	{
		Controller->MoveTowardActor(Target, FMath::Max(Snapshot.AttackRange - StopShortOfRange, 10.f));
	}
	else
	{
		Controller->MoveTowardLocation(Targeting->GetTargetLocation(), 50.f);
	}

	return EZAICombatState::Approaching;
}

// --- Strafing ---

void UZAICombatState_Strafing::Enter(UZAICombatBrainComponent& Brain)
{
	AZAIBaseController* Controller = Brain.GetZController();
	const AZAIBaseCharacter* Character = Brain.GetCharacter();
	const UZAITargetingComponent* Targeting = Brain.GetTargeting();
	if (!Controller || !Character || !Targeting)
	{
		return;
	}

	Direction = FMath::RandBool() ? 1.f : -1.f;
	bMoving = false;
	Controller->SetMoveSpeed(Character->GetCombatSettings().StrafeSpeed);
	Controller->SetFocus(Targeting->GetTarget());
	StepAround(Brain);
}

void UZAICombatState_Strafing::Exit(UZAICombatBrainComponent& Brain)
{
	bMoving = false;
	if (AZAIBaseController* Controller = Brain.GetZController())
	{
		Controller->StopMoving();
	}
}

EZAICombatState UZAICombatState_Strafing::Update(UZAICombatBrainComponent& Brain, float DeltaTime)
{
	const FZAICombatSnapshot Snapshot = Brain.MakeSnapshot();
	const EZAICombatState Next = ZAICombatRules::WhileStrafing(Snapshot);
	if (Next != EZAICombatState::Strafing)
	{
		return Next;
	}

	if (AZAIBaseController* Controller = Brain.GetZController())
	{
		Controller->SetFocus(Brain.GetTargeting()->GetTarget());
	}

	// now and then it changes direction, and it keeps stepping around the circle
	if (FMath::FRand() < TurnAroundChance)
	{
		Direction = -Direction;
		bMoving = false;
	}

	if (!bMoving)
	{
		StepAround(Brain);
	}

	return EZAICombatState::Strafing;
}

void UZAICombatState_Strafing::OnMoveCompleted(UZAICombatBrainComponent& Brain, bool bSuccess)
{
	bMoving = false;

	// blocked (a wall, another enemy): go the other way round
	if (!bSuccess)
	{
		Direction = -Direction;
	}

	// keep going without waiting for the next decision step
	StepAround(Brain);
}

void UZAICombatState_Strafing::StepAround(UZAICombatBrainComponent& Brain)
{
	AZAIBaseController* Controller = Brain.GetZController();
	const AZAIBaseCharacter* Character = Brain.GetCharacter();
	const UZAITargetingComponent* Targeting = Brain.GetTargeting();
	if (!Controller || !Character || !Targeting || !Targeting->GetTarget())
	{
		return;
	}

	// the next spot on a circle around the target, a step further around from where it stands now
	const FVector Center = Targeting->GetTargetLocation();
	FVector FromCenter = (Character->GetActorLocation() - Center).GetSafeNormal2D();
	if (FromCenter.IsNearlyZero())
	{
		FromCenter = -Character->GetActorForwardVector().GetSafeNormal2D();
	}

	const FVector Spot = Center + FromCenter.RotateAngleAxis(StepAngle * Direction, FVector::UpVector) * Character->GetCombatSettings().StrafeDistance;
	const EPathFollowingRequestResult::Type Result = Controller->MoveTowardLocation(FVector(Spot.X, Spot.Y, Character->GetActorLocation().Z), AcceptanceRadius);

	// on its way, or already there (the next step comes at the next decision); no way there: the other way round next time
	bMoving = Result == EPathFollowingRequestResult::RequestSuccessful;
	if (Result == EPathFollowingRequestResult::Failed)
	{
		Direction = -Direction;
	}
}

// --- Attacking ---

void UZAICombatState_Attacking::Enter(UZAICombatBrainComponent& Brain)
{
	// it stands and keeps facing the target while it winds up (the attack ability does the rest)
	if (AZAIBaseController* Controller = Brain.GetZController())
	{
		Controller->StopMoving();
		if (const UZAITargetingComponent* Targeting = Brain.GetTargeting())
		{
			Controller->SetFocus(Targeting->GetTarget());
		}
	}
}

EZAICombatState UZAICombatState_Attacking::Update(UZAICombatBrainComponent& Brain, float DeltaTime)
{
	// committed while the attack runs and until its animation is over (an ability may end at its hit while the swing goes on)
	const AZAIBaseCharacter* Character = Brain.GetCharacter();
	if (Brain.HasStateTag(ZGameplayTags::State_Attacking) || (Character && Character->IsPlayingAttackAnimation()))
	{
		return EZAICombatState::Attacking;
	}

	// over: strafers circle for a while before the next attack (a stagger cutting it short skips that: it comes back angry)
	Brain.StartStrafeTimer();
	return ZAICombatRules::AfterAction(Brain.MakeSnapshot());
}

// --- DodgeRecovering ---

void UZAICombatState_DodgeRecovering::Enter(UZAICombatBrainComponent& Brain)
{
	const AZAIBaseCharacter* Character = Brain.GetCharacter();
	RecoveredTime = (Brain.GetWorld() ? Brain.GetWorld()->GetTimeSeconds() : 0.0) + (Character ? Character->GetCombatSettings().DodgeRecoveryTime : 0.f);
	if (AZAIBaseController* Controller = Brain.GetZController())
	{
		Controller->StopMoving();
	}
}

EZAICombatState UZAICombatState_DodgeRecovering::Update(UZAICombatBrainComponent& Brain, float DeltaTime)
{
	const double Now = Brain.GetWorld() ? Brain.GetWorld()->GetTimeSeconds() : 0.0;
	const AZAIBaseCharacter* Character = Brain.GetCharacter();

	// the recovery counts from the end of the dodge
	if (Brain.HasStateTag(ZGameplayTags::State_Dodging))
	{
		RecoveredTime = Now + (Character ? Character->GetCombatSettings().DodgeRecoveryTime : 0.f);
		return EZAICombatState::DodgeRecovering;
	}

	return Now >= RecoveredTime ? ZAICombatRules::AfterAction(Brain.MakeSnapshot()) : EZAICombatState::DodgeRecovering;
}

// --- Staggered ---

void UZAICombatState_Staggered::Enter(UZAICombatBrainComponent& Brain)
{
	// no moving and no turning toward the target until it's over
	if (AZAIBaseController* Controller = Brain.GetZController())
	{
		Controller->StopMoving();
		Controller->ClearFocus(EAIFocusPriority::Gameplay);
	}
}

EZAICombatState UZAICombatState_Staggered::Update(UZAICombatBrainComponent& Brain, float DeltaTime)
{
	return Brain.HasStateTag(ZGameplayTags::State_Staggered) ? EZAICombatState::Staggered : ZAICombatRules::AfterAction(Brain.MakeSnapshot());
}
