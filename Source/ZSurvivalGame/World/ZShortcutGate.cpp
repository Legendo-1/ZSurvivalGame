// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/ZShortcutGate.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "ZShortcut"

AZShortcutGate::AZShortcutGate()
{
	GatePivot = CreateDefaultSubobject<USceneComponent>(TEXT("GatePivot"));
	GatePivot->SetupAttachment(Root);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(GatePivot);
	GateMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GateMesh->SetStaticMesh(CubeMesh.Object);
	}

	OpenText = LOCTEXT("OpenGate", "Open the gate");
	LockedText = LOCTEXT("GateLocked", "Locked from the other side");
	OpenDuration = 2.f;

	UpdateLayout();
}

void AZShortcutGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateLayout();
	ApplyOpenAlpha(IsOpen() ? 1.f : 0.f);
}

void AZShortcutGate::UpdateLayout()
{
	// the cube is 100 cm: scale it to the opening; it hangs from the pivot (the left edge for a swing)
	const bool bSwing = Motion == EZShortcutGateMotion::Swing;
	GateMesh->SetRelativeScale3D(FVector(GateThickness, GateWidth, GateHeight) / 100.f);
	GateMesh->SetRelativeLocation(FVector(0.f, bSwing ? GateWidth * 0.5f : 0.f, GateHeight * 0.5f));

	// found from both sides, up to head height
	const float BoxHalfHeight = FMath::Min(GateHeight * 0.5f, 150.f);
	InteractionBox->SetBoxExtent(FVector(GateThickness * 0.5f + 100.f, GateWidth * 0.5f, BoxHalfHeight));
	InteractionBox->SetRelativeLocation(FVector(0.f, 0.f, BoxHalfHeight));
}

void AZShortcutGate::ApplyOpenAlpha(float Alpha)
{
	if (Motion == EZShortcutGateMotion::Swing)
	{
		// turns around its left edge, toward its back (away from the open side)
		GatePivot->SetRelativeLocationAndRotation(FVector(0.f, -GateWidth * 0.5f, 0.f), FRotator(0.f, SwingAngle * Alpha, 0.f));
	}
	else
	{
		const float Rise = SlideHeight > 0.f ? SlideHeight : GateHeight + 20.f;
		GatePivot->SetRelativeLocationAndRotation(FVector(0.f, 0.f, Rise * Alpha), FRotator::ZeroRotator);
	}
}

#undef LOCTEXT_NAMESPACE
