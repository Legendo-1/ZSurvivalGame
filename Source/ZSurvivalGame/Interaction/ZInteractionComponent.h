// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZInteractionComponent.generated.h"

/**
 *  On the player's character: finds the interactable the player looks at (or the closest one nearby, e.g. on the floor),
 *  and uses it when the interact key is pressed. Interactions that take time are held; looking away or letting go cancels them
 *  The player's machine finds the target, the server checks range and line of sight before it happens
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZInteractionComponent();

	/** Owning player's machine: keeps the focused interactable up to date */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Owning player: the interact key was pressed. Interacts right away, or starts holding for interactions that take time */
	void StartInteraction();

	/** Owning player: the interact key was released (stops holding) */
	void StopInteraction();

	/** The interactable the player would use (owning player's machine) */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	/** Its prompt text, e.g. "Pick up Rifle Ammo x15" */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetFocusedText() const { return FocusedText; }

	/** 0..1 while holding the key on an interaction that takes time, otherwise 0 */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetHoldProgress() const;

protected:

	/** Owning player's machine: finds the interactable to focus */
	void UpdateFocus();

	/** The interactable under the crosshair, or else the closest one nearby in front of the character */
	AActor* FindBestInteractable() const;

	/** Can the character use this right now: in range, nothing in between, and the interactable agrees? */
	bool CanInteractWith(AActor* Target, float ExtraRange = 0.f) const;

	/** The character this component is on */
	APawn* GetPawn() const;

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerStartHold(AActor* Target);

	UFUNCTION(Server, Reliable)
	void ServerStopHold();

	/** Server: the hold time is up */
	void CompleteHold();

	/** How far from the character the player can interact, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0"))
	float InteractionRange = 250.f;

	/** Things this close are found without looking right at them, in cm (e.g. items on the floor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0"))
	float NearbyRadius = 150.f;

private:

	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedText;

	/** Holding the key, on the owning player's machine */
	bool bHolding = false;
	double HoldStartTime = 0.0;
	float HoldDuration = 0.f;

	/** What's being held, on the server */
	TWeakObjectPtr<AActor> ServerHoldTarget;
	FTimerHandle HoldTimer;
};
