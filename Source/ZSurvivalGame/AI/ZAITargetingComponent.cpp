// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAITargetingComponent.h"
#include "AIController.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Sight.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZCombatLibrary.h"

UZAITargetingComponent::UZAITargetingComponent()
{
	// the brain updates it on its own beat
	PrimaryComponentTick.bCanEverTick = false;
}

void UZAITargetingComponent::Initialize(UAIPerceptionComponent* Perception, const FZAICombatSettings& InSettings)
{
	Settings = InSettings;

	if (PerceptionComponent.Get() != Perception)
	{
		if (UAIPerceptionComponent* OldPerception = PerceptionComponent.Get())
		{
			OldPerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &UZAITargetingComponent::OnTargetPerceptionUpdated);
		}

		PerceptionComponent = Perception;
		if (Perception)
		{
			Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &UZAITargetingComponent::OnTargetPerceptionUpdated);
		}
	}

	LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UZAITargetingComponent::Reset()
{
	KnownEnemies.Reset();
	SetTarget(nullptr);
}

APawn* UZAITargetingComponent::GetControlledPawn() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	return Controller ? Controller->GetPawn() : nullptr;
}

bool UZAITargetingComponent::IsValidEnemy(const AActor* Actor) const
{
	// alive and on a team it fights (players for monsters; both sides for hostile humans)
	const APawn* Pawn = GetControlledPawn();
	return Actor && Pawn && Actor != Pawn && !Actor->IsActorBeingDestroyed() && !UZCombatLibrary::IsDead(Actor) && UZCombatLibrary::CanDamage(Pawn, Actor);
}

FZAIKnownEnemy* UZAITargetingComponent::FindKnown(const AActor* Actor)
{
	return KnownEnemies.FindByPredicate([Actor](const FZAIKnownEnemy& Known) { return Known.Actor.Get() == Actor; });
}

const FZAIKnownEnemy* UZAITargetingComponent::FindKnown(const AActor* Actor) const
{
	return KnownEnemies.FindByPredicate([Actor](const FZAIKnownEnemy& Known) { return Known.Actor.Get() == Actor; });
}

FZAIKnownEnemy& UZAITargetingComponent::FindOrAddKnown(AActor* Actor)
{
	if (FZAIKnownEnemy* Known = FindKnown(Actor))
	{
		return *Known;
	}

	FZAIKnownEnemy& NewEnemy = KnownEnemies.AddDefaulted_GetRef();
	NewEnemy.Actor = Actor;
	NewEnemy.LastKnownLocation = Actor->GetActorLocation();
	return NewEnemy;
}

void UZAITargetingComponent::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!IsValidEnemy(Actor) || !GetWorld())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	FZAIKnownEnemy& Known = FindOrAddKnown(Actor);

	// hit by it: it knows who and where from, and it draws its anger
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
	{
		Known.LastKnownLocation = Stimulus.StimulusLocation;
		Known.LastSensedTime = Now;
		Known.Threat += FMath::Max(Stimulus.Strength, 0.f) * Scoring.ThreatPerDamage;
		UE_LOG(LogZSurvivalGame, VeryVerbose, TEXT("AI '%s' was hit by '%s' (threat %.0f)"), *GetNameSafe(GetControlledPawn()), *GetNameSafe(Actor), Known.Threat);
		return;
	}

	// seen, or lost from sight (then it remembers where it last saw it)
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		Known.bVisible = Stimulus.WasSuccessfullySensed();
		Known.LastKnownLocation = Stimulus.StimulusLocation;
		if (Known.bVisible)
		{
			Known.LastSensedTime = Now;
		}

		UE_LOG(LogZSurvivalGame, VeryVerbose, TEXT("AI '%s' %s '%s'"), *GetNameSafe(GetControlledPawn()), Known.bVisible ? TEXT("sees") : TEXT("lost sight of"), *GetNameSafe(Actor));
	}
}

void UZAITargetingComponent::SenseNearbyPlayers(double Now)
{
	const APawn* Pawn = GetControlledPawn();
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!Pawn || !GameState || Settings.ProximityRadius <= 0.f)
	{
		return;
	}

	// a handful of players: cheaper than any query
	const float RadiusSquared = FMath::Square(Settings.ProximityRadius);
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		APawn* PlayerPawn = Player ? Player->GetPawn() : nullptr;
		if (PlayerPawn && FVector::DistSquared2D(PlayerPawn->GetActorLocation(), Pawn->GetActorLocation()) <= RadiusSquared && IsValidEnemy(PlayerPawn))
		{
			FZAIKnownEnemy& Known = FindOrAddKnown(PlayerPawn);
			Known.LastKnownLocation = PlayerPawn->GetActorLocation();
			Known.LastSensedTime = Now;
		}
	}
}

float UZAITargetingComponent::ScoreEnemy(float DistanceCm, float Threat, bool bVisible, bool bIsCurrentTarget, const FZAITargetScoring& InScoring)
{
	return Threat
		+ (bVisible ? InScoring.VisibleBonus : 0.f)
		+ (bIsCurrentTarget ? InScoring.CurrentTargetBonus : 0.f)
		- InScoring.DistancePenaltyPerMeter * DistanceCm / 100.f;
}

void UZAITargetingComponent::UpdateTarget()
{
	const APawn* Pawn = GetControlledPawn();
	const UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		Reset();
		return;
	}

	const double Now = World->GetTimeSeconds();
	const float DeltaTime = static_cast<float>(FMath::Max(Now - LastUpdateTime, 0.0));
	LastUpdateTime = Now;

	SenseNearbyPlayers(Now);

	// forget the dead, the friendly and the long unsensed; threat fades
	KnownEnemies.RemoveAll([this, Now](const FZAIKnownEnemy& Known)
	{
		const bool bSensedRecently = Known.bVisible || Now - Known.LastSensedTime <= Settings.ForgetTime;
		return !IsValidEnemy(Known.Actor.Get()) || !bSensedRecently;
	});

	AActor* BestEnemy = nullptr;
	float BestScore = -MAX_flt;
	for (FZAIKnownEnemy& Known : KnownEnemies)
	{
		Known.Threat = FMath::Max(0.f, Known.Threat - Scoring.ThreatDecayPerSecond * DeltaTime);

		// a seen enemy is where it is; an unseen one where it was last sensed
		AActor* Enemy = Known.Actor.Get();
		if (Known.bVisible)
		{
			Known.LastKnownLocation = Enemy->GetActorLocation();
			Known.LastSensedTime = Now;
		}

		const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Known.LastKnownLocation);
		const float Score = ScoreEnemy(Distance, Known.Threat, Known.bVisible, Enemy == Target.Get(), Scoring);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestEnemy = Enemy;
		}
	}

	SetTarget(BestEnemy);
}

void UZAITargetingComponent::AddThreat(AActor* Enemy, float Amount)
{
	if (IsValidEnemy(Enemy) && GetWorld())
	{
		FZAIKnownEnemy& Known = FindOrAddKnown(Enemy);
		Known.Threat += Amount;
		Known.LastSensedTime = GetWorld()->GetTimeSeconds();
	}
}

void UZAITargetingComponent::SetTarget(AActor* NewTarget)
{
	AActor* OldTarget = Target.Get();
	if (OldTarget == NewTarget)
	{
		return;
	}

	Target = NewTarget;
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("AI '%s' targets '%s' (was '%s')"), *GetNameSafe(GetControlledPawn()), *GetNameSafe(NewTarget), *GetNameSafe(OldTarget));
	OnTargetChanged.Broadcast(NewTarget, OldTarget);
}

bool UZAITargetingComponent::IsTargetVisible() const
{
	const FZAIKnownEnemy* Known = FindKnown(Target.Get());
	return Known && Known->bVisible;
}

FVector UZAITargetingComponent::GetTargetLocation() const
{
	const AActor* CurrentTarget = Target.Get();
	const FZAIKnownEnemy* Known = FindKnown(CurrentTarget);
	if (!CurrentTarget || !Known)
	{
		return FVector::ZeroVector;
	}

	// seen or sensed this beat: where it is now; otherwise only where it was
	const bool bSensedNow = Known->bVisible || (GetWorld() && GetWorld()->GetTimeSeconds() - Known->LastSensedTime < 0.5);
	return bSensedNow ? CurrentTarget->GetActorLocation() : Known->LastKnownLocation;
}

float UZAITargetingComponent::GetTimeSinceTargetSensed() const
{
	const FZAIKnownEnemy* Known = FindKnown(Target.Get());
	return Known && GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - Known->LastSensedTime) : MAX_flt;
}

FString UZAITargetingComponent::GetDebugString() const
{
	const AActor* CurrentTarget = Target.Get();
	const APawn* Pawn = GetControlledPawn();
	if (!CurrentTarget || !Pawn)
	{
		return FString::Printf(TEXT("no target, %d known"), KnownEnemies.Num());
	}

	const FZAIKnownEnemy* Known = FindKnown(CurrentTarget);
	return FString::Printf(TEXT("target '%s' %.1f m away (%s, threat %.0f), %d known"), *GetNameSafe(CurrentTarget),
		FVector::Dist2D(Pawn->GetActorLocation(), GetTargetLocation()) / 100.f, IsTargetVisible() ? TEXT("seen") : TEXT("not seen"),
		Known ? Known->Threat : 0.f, KnownEnemies.Num());
}
