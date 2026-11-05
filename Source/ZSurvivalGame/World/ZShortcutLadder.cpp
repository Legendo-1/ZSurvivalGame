// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/ZShortcutLadder.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "ZSurvivalGame.h"

#define LOCTEXT_NAMESPACE "ZShortcut"

namespace
{
	/** Ladder rail thickness and how far it stands off the wall (cm) */
	constexpr float LadderDepth = 10.f;

	/** How far from the ladder the exits are: in front of it below, behind the ledge on top (cm) */
	constexpr float BottomExitDistance = 90.f;
	constexpr float TopExitDistance = 80.f;

	/** Exits are this high above their floor: a standing player's capsule middle (cm) */
	constexpr float ExitStandHeight = 100.f;

	/** How far above the top floor players on it use it from (cm) */
	constexpr float TopEndSightHeight = 30.f;
}

AZShortcutLadder::AZShortcutLadder()
{
	LadderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LadderMesh"));
	LadderMesh->SetupAttachment(Root);

	// only seen and found by the interaction box; it never blocks anyone
	LadderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LadderMesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		LadderMesh->SetStaticMesh(CubeMesh.Object);
	}

	TopExit = CreateDefaultSubobject<USceneComponent>(TEXT("TopExit"));
	TopExit->SetupAttachment(Root);

	BottomExit = CreateDefaultSubobject<USceneComponent>(TEXT("BottomExit"));
	BottomExit->SetupAttachment(Root);

	OpenText = LOCTEXT("KickLadder", "Kick down the ladder");
	LockedText = LOCTEXT("LadderOutOfReach", "Out of reach");
	ClimbUpText = LOCTEXT("ClimbUp", "Climb up");
	ClimbDownText = LOCTEXT("ClimbDown", "Climb down");
	OpenDuration = 0.6f;

	UpdateLayout();
}

void AZShortcutLadder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateLayout();
	ApplyOpenAlpha(IsOpen() ? 1.f : 0.f);
}

void AZShortcutLadder::UpdateLayout()
{
	// the cube is 100 cm; the pivot is halfway up, the wall is behind (-X)
	LadderMesh->SetRelativeScale3D(FVector(LadderDepth, LadderWidth, LadderHeight) / 100.f);

	// covers the whole ladder, so it's found from the bottom floor and from the ledge
	InteractionBox->SetBoxExtent(FVector(60.f, LadderWidth * 0.5f + 20.f, LadderHeight * 0.5f));
	InteractionBox->SetRelativeLocation(FVector(40.f, 0.f, 0.f));

	TopExit->SetRelativeLocation(FVector(-TopExitDistance, 0.f, LadderHeight * 0.5f + ExitStandHeight));
	BottomExit->SetRelativeLocation(FVector(BottomExitDistance, 0.f, -LadderHeight * 0.5f + ExitStandHeight));
}

void AZShortcutLadder::ApplyOpenAlpha(float Alpha)
{
	// pulled up until it's kicked down: it slides down into place
	const float RaisedOffset = LadderHeight * RaisedFraction;
	LadderMesh->SetRelativeLocation(FVector(LadderDepth * 0.5f, 0.f, FMath::Lerp(RaisedOffset, 0.f, Alpha)));
}

bool AZShortcutLadder::IsAtTop(const APawn* Pawn) const
{
	// feet above the ladder's middle: standing on the top floor
	return Pawn && Pawn->GetActorLocation().Z - Pawn->GetSimpleCollisionHalfHeight() > GetActorLocation().Z;
}

bool AZShortcutLadder::IsOnOpenSide(const APawn* Pawn) const
{
	return IsAtTop(Pawn);
}

FVector AZShortcutLadder::GetInteractionLocation_Implementation(APawn* Interactor) const
{
	if (!IsAtTop(Interactor))
	{
		return Super::GetInteractionLocation_Implementation(Interactor);
	}

	// just above the ledge, at the ladder's top end: seen from the top floor without looking through the ledge
	return GetActorTransform().TransformPosition(FVector(LadderDepth, 0.f, LadderHeight * 0.5f + TopEndSightHeight));
}

FText AZShortcutLadder::GetOpenInteractionText(const APawn* Interactor) const
{
	return IsAtTop(Interactor) ? ClimbDownText : ClimbUpText;
}

void AZShortcutLadder::InteractWhileOpen(APawn* Interactor)
{
	const bool bGoingDown = IsAtTop(Interactor);
	const FVector Destination = (bGoingDown ? BottomExit : TopExit)->GetComponentLocation();

	// going up: face inland over the ledge; going down: face away from the wall
	const FRotator Facing(0.f, (bGoingDown ? GetActorForwardVector() : -GetActorForwardVector()).Rotation().Yaw, 0.f);

	if (!Interactor->TeleportTo(Destination, Facing))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Ladder '%s': no room for '%s' at the %s exit"), *GetName(), *GetNameSafe(Interactor), bGoingDown ? TEXT("bottom") : TEXT("top"));
		return;
	}

	// the camera turns with the player
	if (APlayerController* PlayerController = Cast<APlayerController>(Interactor->GetController()))
	{
		PlayerController->ClientSetRotation(Facing);
	}
}

#undef LOCTEXT_NAMESPACE
