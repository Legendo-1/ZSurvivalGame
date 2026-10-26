// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ZInteractable.generated.h"

class APawn;

UINTERFACE(MinimalAPI, BlueprintType)
class UZInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Something a player can use by looking at it and pressing the interact key:
 *  pickups now, later doors, crafting stations, harvesting and reviving teammates
 *  Implement it in C++ or in a Blueprint (Class Settings → Interfaces → Add → ZInteractable)
 *  The actor needs a collision that blocks the Interaction trace channel, so players can find it
 */
class IZInteractable
{
	GENERATED_BODY()

public:

	/** Can this player use it right now? Checked on the player's machine for the prompt, and on the server before Interact */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(APawn* Interactor) const;

	/** Prompt text, e.g. "Pick up Rifle Ammo x15" */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionText(APawn* Interactor) const;

	/** Seconds the key has to be held (0 = right away), e.g. for reviving or harvesting */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	float GetInteractionDuration(APawn* Interactor) const;

	/** Server: the player used it */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(APawn* Interactor);

	virtual bool CanInteract_Implementation(APawn* Interactor) const { return true; }
	virtual FText GetInteractionText_Implementation(APawn* Interactor) const;
	virtual float GetInteractionDuration_Implementation(APawn* Interactor) const { return 0.f; }
	virtual void Interact_Implementation(APawn* Interactor) {}
};
