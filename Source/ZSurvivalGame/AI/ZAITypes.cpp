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

	float GetApproachRange(TConstArrayView<FZAttackOption> Options, float Distance)
	{
		// the attacks it goes by: the ready ones, or all of them while none is ready
		const bool bAnyReady = Options.ContainsByPredicate([](const FZAttackOption& Option) { return Option.bReady && Option.Weight > 0.f; });
		bool bAnyCounted = false;
		float FarthestBelow = -1.f;
		for (const FZAttackOption& Option : Options)
		{
			if (Option.Weight <= 0.f || (bAnyReady && !Option.bReady))
			{
				continue;
			}

			bAnyCounted = true;

			// one fits: it's close enough already
			if (Distance >= Option.MinRange && Distance <= Option.MaxRange)
			{
				return Distance;
			}

			// one it could walk into
			if (Option.MaxRange < Distance)
			{
				FarthestBelow = FMath::Max(FarthestBelow, Option.MaxRange);
			}
		}

		// no attacks, or the target is too close for all of them: it stays where it is
		return bAnyCounted && FarthestBelow >= 0.f ? FarthestBelow : Distance;
	}

	/** One of the options that pass the filter, by weight (Roll: 0 to 1); INDEX_NONE if none passes */
	template <typename FilterType>
	int32 PickByWeight(TConstArrayView<FZAttackOption> Options, float Roll, FilterType Filter)
	{
		float TotalWeight = 0.f;
		for (const FZAttackOption& Option : Options)
		{
			TotalWeight += Option.Weight > 0.f && Filter(Option) ? Option.Weight : 0.f;
		}

		if (TotalWeight <= 0.f)
		{
			return INDEX_NONE;
		}

		// walk the options that pass until the roll's share of the total weight is used up
		float Remaining = FMath::Clamp(Roll, 0.f, 1.f) * TotalWeight;
		int32 LastPassed = INDEX_NONE;
		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			if (Options[Index].Weight <= 0.f || !Filter(Options[Index]))
			{
				continue;
			}

			LastPassed = Index;
			Remaining -= Options[Index].Weight;
			if (Remaining < 0.f)
			{
				return Index;
			}
		}

		// a roll of exactly 1
		return LastPassed;
	}

	int32 ChooseAttack(TConstArrayView<FZAttackOption> Options, float Distance, float Roll)
	{
		return PickByWeight(Options, Roll, [Distance](const FZAttackOption& Option)
		{
			return Option.bReady && Distance >= Option.MinRange && Distance <= Option.MaxRange;
		});
	}

	int32 PlanAttack(TConstArrayView<FZAttackOption> Options, float Distance, float Roll)
	{
		// the target isn't too close for it: it fits now, or by walking closer
		return PickByWeight(Options, Roll, [Distance](const FZAttackOption& Option)
		{
			return Option.bReady && Distance >= Option.MinRange;
		});
	}

	float GetApproachRangeFor(const FZAttackOption& Option, float Distance)
	{
		return Distance > Option.MaxRange ? Option.MaxRange : Distance;
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
