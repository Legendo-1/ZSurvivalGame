// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZAIBaseController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"
#include "ZSurvivalGame.h"
#include "AI/ZAIBaseCharacter.h"
#include "AI/ZAICombatBrainComponent.h"
#include "AI/ZAITargetingComponent.h"

AZAIBaseController::AZAIBaseController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// every frame it turns the character toward its focus (the engine's AI controller does that in its tick); decisions run on the brain's timer
	PrimaryActorTick.bCanEverTick = true;

	// senses: sight (enemies only, i.e. other teams) and damage; the numbers come from the character when it's possessed
	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 2500.f;
	SightConfig->LoseSightRadius = 3000.f;
	SightConfig->PeripheralVisionAngleDegrees = 80.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	// near where it last saw a target it keeps "seeing" it without a trace: cheaper, and it doesn't lose someone who ducks behind it
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 300.f;
	SightConfig->SetMaxAge(8.f);

	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(8.f);

	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*DamageConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);

	Targeting = CreateDefaultSubobject<UZAITargetingComponent>(TEXT("Targeting"));

	// the combat state machine is this controller's brain (started in OnPossess, once the senses are set up)
	CombatBrain = CreateDefaultSubobject<UZAICombatBrainComponent>(TEXT("CombatBrain"));
	BrainComponent = CombatBrain;
	bStartAILogicOnPossess = false;
}

void AZAIBaseController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AZAIBaseCharacter* ControlledCharacter = Cast<AZAIBaseCharacter>(InPawn);
	if (!ControlledCharacter)
	{
		return;
	}

	// the controller is on its character's team: perception only reports enemies (other teams)
	SetGenericTeamId(ControlledCharacter->GetGenericTeamId());

	ConfigurePerception(ControlledCharacter->GetCombatSettings());
	Targeting->Initialize(GetPerceptionComponent(), ControlledCharacter->GetCombatSettings());
	CombatBrain->StartLogic();
}

void AZAIBaseController::OnUnPossess()
{
	// the engine cleans up the brain after this (it stops the state machine)
	Targeting->Reset();

	Super::OnUnPossess();
}

void AZAIBaseController::RefreshCombatSettings()
{
	if (const AZAIBaseCharacter* ControlledCharacter = Cast<AZAIBaseCharacter>(GetPawn()))
	{
		ConfigurePerception(ControlledCharacter->GetCombatSettings());
		Targeting->Initialize(GetPerceptionComponent(), ControlledCharacter->GetCombatSettings());
	}
}

void AZAIBaseController::ConfigurePerception(const FZAICombatSettings& Settings)
{
	UAIPerceptionComponent* Perception = GetPerceptionComponent();
	if (!Perception || !SightConfig || !DamageConfig)
	{
		return;
	}

	SightConfig->SightRadius = Settings.SightRadius;
	SightConfig->LoseSightRadius = FMath::Max(Settings.LoseSightRadius, Settings.SightRadius);
	SightConfig->PeripheralVisionAngleDegrees = Settings.VisionHalfAngle;
	SightConfig->SetMaxAge(Settings.ForgetTime);
	DamageConfig->SetMaxAge(Settings.ForgetTime);

	Perception->ConfigureSense(*SightConfig);
	Perception->ConfigureSense(*DamageConfig);

	// the perception system keeps the listener's team from when it registered: tell it the team is set now
	Perception->RequestStimuliListenerUpdate();
}

void AZAIBaseController::MoveTowardActor(AActor* Goal, float AcceptanceRadius)
{
	if (!Goal)
	{
		return;
	}

	// already on its way to it: the path follows the actor by itself
	const UPathFollowingComponent* PathFollowing = GetPathFollowingComponent();
	if (GetMoveStatus() == EPathFollowingStatus::Moving && PathFollowing && PathFollowing->GetMoveGoal() == Goal)
	{
		return;
	}

	// on the navmesh if the level has one, otherwise straight at it. The acceptance radius is from middle to middle
	// (stopping "on overlap" would add the character's own radius, and it would stop short of its attack range)
	TGuardValue<bool> RequestingMove(bRequestingMove, true);
	if (MoveToActor(Goal, AcceptanceRadius, false, true, true) == EPathFollowingRequestResult::Failed)
	{
		MoveToActor(Goal, AcceptanceRadius, false, false, true);
	}
}

EPathFollowingRequestResult::Type AZAIBaseController::MoveTowardLocation(const FVector& Location, float AcceptanceRadius)
{
	// on the navmesh if the level has one (the spot moved onto it), otherwise straight there
	TGuardValue<bool> RequestingMove(bRequestingMove, true);
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Location, AcceptanceRadius, false, true, true, true);
	return Result != EPathFollowingRequestResult::Failed ? Result : MoveToLocation(Location, AcceptanceRadius, false, false, false, true);
}

void AZAIBaseController::StopMoving()
{
	if (GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		StopMovement();
	}
}

void AZAIBaseController::SetMoveSpeed(float Speed)
{
	const ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn());
	if (ControlledCharacter && Speed > 0.f)
	{
		ControlledCharacter->GetCharacterMovement()->MaxWalkSpeed = Speed;
	}
}

void AZAIBaseController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	// aborted moves were replaced or stopped on purpose, and a move that ends during its own request is answered by the request;
	// the state only hears about real ends of a running move (arrived, blocked, lost its path)
	if (!bRequestingMove && Result.Code != EPathFollowingResult::Aborted)
	{
		CombatBrain->HandleMoveCompleted(Result.IsSuccess());
	}
}
