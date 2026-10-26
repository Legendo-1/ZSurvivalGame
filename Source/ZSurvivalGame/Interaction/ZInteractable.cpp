// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/ZInteractable.h"

FText IZInteractable::GetInteractionText_Implementation(APawn* Interactor) const
{
	return NSLOCTEXT("ZInteraction", "DefaultInteractionText", "Interact");
}
