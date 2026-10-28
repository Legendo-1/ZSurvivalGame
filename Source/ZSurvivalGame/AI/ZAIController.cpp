// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAIController.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "AI/ZAICharacter.h"
#include "AbilitySystem/ZCombatLibrary.h"

AZAIController::AZAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AZAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// the controller is on the same team as its character (for AI perception later)
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(InPawn))
	{
		SetGenericTeamId(TeamAgent->GetGenericTeamId());
	}

	// start a bit randomly, so many enemies don't all think in the same frame
	GetWorldTimerManager().SetTimer(ThinkTimer, this, &AZAIController::Think, ThinkInterval, true, FMath::FRandRange(0.f, ThinkInterval));
}

void AZAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	CurrentTarget = nullptr;

	Super::OnUnPossess();
}

void AZAIController::Think()
{
	AZAICharacter* ControlledCharacter = Cast<AZAICharacter>(GetPawn());
	if (!ControlledCharacter || ControlledCharacter->IsDead())
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	// staggered: it can't chase or attack until it's over
	if (UZCombatLibrary::IsStaggered(ControlledCharacter))
	{
		StopMovement();
		return;
	}

	APawn* Target = FindTarget(ControlledCharacter);
	if (!Target)
	{
		// lost the target: stand still
		if (CurrentTarget.IsValid())
		{
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		CurrentTarget = nullptr;
		return;
	}

	SetFocus(Target);

	const float Distance = FVector::Dist2D(ControlledCharacter->GetActorLocation(), Target->GetActorLocation());
	if (Distance <= ControlledCharacter->GetAttackRange())
	{
		StopMovement();
		ControlledCharacter->TryAttack();
	}
	else if (Target != CurrentTarget.Get() || GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		// walk on the navmesh if the level has one, otherwise straight at the target
		const float AcceptanceRadius = ControlledCharacter->GetAttackRange() * 0.5f;
		if (MoveToActor(Target, AcceptanceRadius, true, true) == EPathFollowingRequestResult::Failed)
		{
			MoveToActor(Target, AcceptanceRadius, true, false);
		}
	}

	CurrentTarget = Target;
}

APawn* AZAIController::FindTarget(const AZAICharacter* ControlledCharacter) const
{
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	if (!GameState)
	{
		return nullptr;
	}

	// TODO: human NPCs and monsters fighting each other, and AI perception (sight, hearing) instead of distance
	APawn* ClosestTarget = nullptr;
	float ClosestDistanceSquared = FMath::Square(ControlledCharacter->GetAggroRange());

	for (const APlayerState* Player : GameState->PlayerArray)
	{
		APawn* PlayerPawn = Player ? Player->GetPawn() : nullptr;
		if (!PlayerPawn || UZCombatLibrary::IsDead(PlayerPawn) || !UZCombatLibrary::CanDamage(ControlledCharacter, PlayerPawn))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(ControlledCharacter->GetActorLocation(), PlayerPawn->GetActorLocation());
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestTarget = PlayerPawn;
		}
	}

	return ClosestTarget;
}
