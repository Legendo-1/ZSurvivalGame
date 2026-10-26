// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/ZInteractionComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "Interaction/ZInteractable.h"

namespace
{
	// the server allows a bit more range, since the player has moved on by the time the request arrives
	constexpr float ServerRangeSlack = 100.f;

	// radius of the sweep along the crosshair, so small things are easier to hit
	constexpr float FocusSweepRadius = 20.f;
}

UZInteractionComponent::UZInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;

	// needed for the server requests
	SetIsReplicatedByDefault(true);
}

void UZInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = GetPawn();
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	UpdateFocus();

	// the server finishes it on its own timer
	if (bHolding && GetHoldProgress() >= 1.f)
	{
		bHolding = false;
	}
}

void UZInteractionComponent::StartInteraction()
{
	APawn* Pawn = GetPawn();
	if (!Pawn || !Pawn->IsLocallyControlled() || bHolding)
	{
		return;
	}

	UpdateFocus();

	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return;
	}

	const float Duration = IZInteractable::Execute_GetInteractionDuration(Target, Pawn);
	if (Duration <= 0.f)
	{
		ServerInteract(Target);
		return;
	}

	bHolding = true;
	HoldStartTime = GetWorld()->GetTimeSeconds();
	HoldDuration = Duration;
	ServerStartHold(Target);
}

void UZInteractionComponent::StopInteraction()
{
	if (!bHolding)
	{
		return;
	}

	bHolding = false;
	ServerStopHold();
}

float UZInteractionComponent::GetHoldProgress() const
{
	if (!bHolding || HoldDuration <= 0.f)
	{
		return 0.f;
	}

	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - HoldStartTime) / HoldDuration), 0.f, 1.f);
}

void UZInteractionComponent::UpdateFocus()
{
	AActor* NewFocus = FindBestInteractable();

	// looking at something else cancels a hold
	if (bHolding && NewFocus != FocusedActor.Get())
	{
		StopInteraction();
	}

	FocusedActor = NewFocus;
	FocusedText = NewFocus ? IZInteractable::Execute_GetInteractionText(NewFocus, GetPawn()) : FText::GetEmpty();
}

AActor* UZInteractionComponent::FindBestInteractable() const
{
	APawn* Pawn = GetPawn();
	const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		return nullptr;
	}

	// along the crosshair, starting level with the character so nothing behind it is picked
	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector TraceStart = ViewLocation + ViewDirection * FVector::DotProduct(Pawn->GetActorLocation() - ViewLocation, ViewDirection);
	const FVector TraceEnd = TraceStart + ViewDirection * InteractionRange;

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZInteractionTrace), false, Pawn);

	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity, ZTraceChannel_Interaction, FCollisionShape::MakeSphere(FocusSweepRadius), QueryParams)
		&& CanInteractWith(Hit.GetActor()))
	{
		return Hit.GetActor();
	}

	// nothing under the crosshair: the closest one nearby that isn't behind the character
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, Pawn->GetActorLocation(), FQuat::Identity, ZTraceChannel_Interaction, FCollisionShape::MakeSphere(NearbyRadius), QueryParams);

	const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
	AActor* ClosestActor = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || Candidate == ClosestActor)
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - Pawn->GetActorLocation();
		const float DistanceSquared = ToCandidate.SizeSquared2D();
		if (DistanceSquared < ClosestDistanceSquared && FVector::DotProduct(Forward, ToCandidate.GetSafeNormal2D()) > -0.25f && CanInteractWith(Candidate))
		{
			ClosestActor = Candidate;
			ClosestDistanceSquared = DistanceSquared;
		}
	}

	return ClosestActor;
}

bool UZInteractionComponent::CanInteractWith(AActor* Target, float ExtraRange) const
{
	APawn* Pawn = GetPawn();
	if (!Pawn || !IsValid(Target) || !Target->Implements<UZInteractable>() || UZCombatLibrary::IsDead(Pawn))
	{
		return false;
	}

	// in range
	const float MaxDistance = InteractionRange + NearbyRadius + ExtraRange;
	if (FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxDistance))
	{
		return false;
	}

	// nothing in between (no grabbing things through walls)
	FCollisionQueryParams SightParams(SCENE_QUERY_STAT(ZInteractionSight), false, Pawn);
	SightParams.AddIgnoredActor(Target);
	FHitResult Blocker;
	if (GetWorld()->LineTraceSingleByChannel(Blocker, Pawn->GetPawnViewLocation(), Target->GetActorLocation(), ECC_Visibility, SightParams))
	{
		return false;
	}

	return IZInteractable::Execute_CanInteract(Target, Pawn);
}

APawn* UZInteractionComponent::GetPawn() const
{
	return Cast<APawn>(GetOwner());
}

void UZInteractionComponent::ServerInteract_Implementation(AActor* Target)
{
	// the player's machine picked it; check it really can be used from here (and doesn't need holding)
	if (CanInteractWith(Target, ServerRangeSlack) && IZInteractable::Execute_GetInteractionDuration(Target, GetPawn()) <= 0.f)
	{
		IZInteractable::Execute_Interact(Target, GetPawn());
	}
}

void UZInteractionComponent::ServerStartHold_Implementation(AActor* Target)
{
	const float Duration = CanInteractWith(Target, ServerRangeSlack) ? IZInteractable::Execute_GetInteractionDuration(Target, GetPawn()) : 0.f;
	if (Duration <= 0.f)
	{
		return;
	}

	ServerHoldTarget = Target;
	GetWorld()->GetTimerManager().SetTimer(HoldTimer, this, &UZInteractionComponent::CompleteHold, Duration, false);
}

void UZInteractionComponent::ServerStopHold_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(HoldTimer);
	ServerHoldTarget = nullptr;
}

void UZInteractionComponent::CompleteHold()
{
	AActor* Target = ServerHoldTarget.Get();
	ServerHoldTarget = nullptr;

	// still in range and still possible after the whole hold
	if (CanInteractWith(Target, ServerRangeSlack))
	{
		IZInteractable::Execute_Interact(Target, GetPawn());
	}
}
