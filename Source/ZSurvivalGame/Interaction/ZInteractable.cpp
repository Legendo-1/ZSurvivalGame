// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/ZInteractable.h"
#include "GameFramework/Actor.h"

FText IZInteractable::GetInteractionText_Implementation(APawn* Interactor) const
{
	return NSLOCTEXT("ZInteraction", "DefaultInteractionText", "Interact");
}

FVector IZInteractable::GetInteractionLocation_Implementation(APawn* Interactor) const
{
	const AActor* Actor = Cast<AActor>(this);
	return Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
}
