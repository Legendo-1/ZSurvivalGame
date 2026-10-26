// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/ZCraftingStation.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"
#include "ZSurvivalGamePlayerController.h"
#include "ZSurvivalGameTypes.h"

#define LOCTEXT_NAMESPACE "ZCrafting"

AZCraftingStation::AZCraftingStation()
{
	PrimaryActorTick.bCanEverTick = false;

	// nothing about it changes while playing: placed stations stay dormant, spawned ones (e.g. built by players later) update rarely
	bReplicates = true;
	NetDormancy = DORM_Initial;
	SetNetUpdateFrequency(1.f);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// BlockAll leaves custom channels at their default (the Interaction channel ignores everything by default)
	Mesh->SetCollisionResponseToChannel(ZTraceChannel_Interaction, ECR_Block);
	SetRootComponent(Mesh);

	// placeholder look: a table-sized box (set a real mesh in the Blueprint)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(1.4f, 0.7f, 0.9f));
	}
}

bool AZCraftingStation::ProvidesStation(FGameplayTag StationTag) const
{
	return StationTag.IsValid() && StationTags.HasTag(StationTag);
}

bool AZCraftingStation::IsInCraftingRange(const AActor* Actor, float ExtraRange) const
{
	return Actor && FVector::DistSquared(Actor->GetActorLocation(), GetActorLocation()) <= FMath::Square(CraftingRadius + ExtraRange);
}

FText AZCraftingStation::GetStationName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	return StationTags.IsEmpty() ? LOCTEXT("UnnamedStation", "Crafting Station") : GetStationNameFromTag(StationTags.First());
}

FText AZCraftingStation::GetStationNameFromTag(FGameplayTag StationTag)
{
	FString TagName = StationTag.ToString();
	int32 LastDot = INDEX_NONE;
	if (TagName.FindLastChar(TEXT('.'), LastDot))
	{
		TagName.RightChopInline(LastDot + 1);
	}

	return FText::FromString(FName::NameToDisplayString(TagName, false));
}

FText AZCraftingStation::GetInteractionText_Implementation(APawn* Interactor) const
{
	return FText::Format(LOCTEXT("UseStation", "Use {0}"), GetStationName());
}

void AZCraftingStation::Interact_Implementation(APawn* Interactor)
{
	// the screen opens on that player's machine; crafting itself checks the distance on the server again
	if (AZSurvivalGamePlayerController* PlayerController = Interactor ? Interactor->GetController<AZSurvivalGamePlayerController>() : nullptr)
	{
		PlayerController->ClientOpenCraftingStation(this);
	}
}

#undef LOCTEXT_NAMESPACE
