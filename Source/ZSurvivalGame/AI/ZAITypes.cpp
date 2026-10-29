// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAITypes.h"

const TCHAR* LexToString(EZAICombatState State)
{
	switch (State)
	{
	case EZAICombatState::Idle:				return TEXT("Idle");
	case EZAICombatState::Approaching:		return TEXT("Approaching");
	case EZAICombatState::Strafing:			return TEXT("Strafing");
	case EZAICombatState::Attacking:		return TEXT("Attacking");
	case EZAICombatState::DodgeRecovering:	return TEXT("DodgeRecovering");
	case EZAICombatState::Staggered:		return TEXT("Staggered");
	default:								return TEXT("?");
	}
}

namespace ZAICombatRules
{
	bool IsLevelWithTarget(const FZAICombatSnapshot& Snapshot)
	{
		return FMath::Abs(Snapshot.HeightToTarget) <= Snapshot.MaxAttackHeight;
	}

	bool IsInAttackRange(const FZAICombatSnapshot& Snapshot)
	{
		return Snapshot.DistanceToTarget <= Snapshot.AttackRange && IsLevelWithTarget(Snapshot);
	}

	EZAICombatState WhileApproaching(const FZAICombatSnapshot& Snapshot)
	{
		if (!Snapshot.bHasTarget)
		{
			return EZAICombatState::Idle;
		}

		// in range, seen, ready, and (for a strafer) done circling: attack
		if (Snapshot.bTargetVisible && IsInAttackRange(Snapshot) && Snapshot.bAttackReady && Snapshot.bStrafeTimeOver)
		{
			return EZAICombatState::Attacking;
		}

		// a strafer close enough that still has to wait: circle meanwhile (a target up on a ledge: find the way up first)
		if (Snapshot.bCanStrafe && !Snapshot.bStrafeTimeOver && IsLevelWithTarget(Snapshot) && Snapshot.DistanceToTarget <= Snapshot.StrafeDistance + StrafeEnterSlack)
		{
			return EZAICombatState::Strafing;
		}

		return EZAICombatState::Approaching;
	}

	EZAICombatState WhileStrafing(const FZAICombatSnapshot& Snapshot)
	{
		if (!Snapshot.bHasTarget)
		{
			return EZAICombatState::Idle;
		}

		// the target got away (also up or down a ledge), or it's time for the next attack: go in
		if (!Snapshot.bCanStrafe || !IsLevelWithTarget(Snapshot) || Snapshot.DistanceToTarget > Snapshot.StrafeDistance + StrafeLeash
			|| (Snapshot.bStrafeTimeOver && Snapshot.bAttackReady))
		{
			return EZAICombatState::Approaching;
		}

		return EZAICombatState::Strafing;
	}

	EZAICombatState AfterAction(const FZAICombatSnapshot& Snapshot)
	{
		if (!Snapshot.bHasTarget)
		{
			return EZAICombatState::Idle;
		}

		if (Snapshot.bCanStrafe && !Snapshot.bStrafeTimeOver && IsLevelWithTarget(Snapshot) && Snapshot.DistanceToTarget <= Snapshot.StrafeDistance + StrafeLeash)
		{
			return EZAICombatState::Strafing;
		}

		return EZAICombatState::Approaching;
	}
}
