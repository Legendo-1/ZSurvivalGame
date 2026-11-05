// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/ZInteractable.h"
#include "ZShortcutBase.generated.h"

class AZSurvivalGameState;
class UArrowComponent;
class UBoxComponent;

/**
 *  A shortcut that opens for good: an iron gate, a kick-down ladder, ...
 *  It's opened once with the interact key from its open side only (one way), then it stays open for everyone and in the host's save:
 *  it's open while the world has its Shortcut Tag (WorldProgress.Shortcut.*, see SAVING.md)
 *  The actor's location is where players use it from (Get Interaction Location can pick another point): the interaction range (4 m) is measured to it
 */
UCLASS(Abstract)
class AZShortcutBase : public AActor, public IZInteractable
{
	GENERATED_BODY()

protected:

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	/** Found by the players' interaction trace (blocks only the Interaction channel) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

#if WITH_EDITORONLY_DATA
	/** Editor only: points to the side it opens from */
	UPROPERTY()
	TObjectPtr<UArrowComponent> OpenSideArrow;
#endif

public:

	AZShortcutBase();

	virtual void Tick(float DeltaSeconds) override;

	/** True once it was opened in this world */
	UFUNCTION(BlueprintPure, Category = "Shortcut")
	bool IsOpen() const { return bOpen; }

	/** True if the pawn stands where this shortcut opens from (default: in front of the actor, its +X side) */
	UFUNCTION(BlueprintPure, Category = "Shortcut")
	virtual bool IsOnOpenSide(const APawn* Pawn) const;

	/** IZInteractable */
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual FText GetInteractionText_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Using it once it's open, e.g. climbing the ladder (nothing by default) */
	virtual bool CanInteractWhileOpen(const APawn* Interactor) const { return false; }
	virtual FText GetOpenInteractionText(const APawn* Interactor) const { return FText::GetEmpty(); }

	/** Server */
	virtual void InteractWhileOpen(APawn* Interactor) {}

	/** Moves its parts: 0 = closed, 1 = open (every machine) */
	virtual void ApplyOpenAlpha(float Alpha) {}

	/** It's open (every machine): bJustOpened is false when it was already open when the level loaded, e.g. for sounds */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shortcut", meta = (DisplayName = "On Opened"))
	void BP_OnOpened(bool bJustOpened);

	/** Saved in the world progress once it's opened: one tag per shortcut, e.g. WorldProgress.Shortcut.CathedralDistrict.IronGate */
	UPROPERTY(EditAnywhere, Category = "Shortcut", meta = (Categories = "WorldProgress.Shortcut"))
	FGameplayTag ShortcutTag;

	/** Prompt on the open side */
	UPROPERTY(EditAnywhere, Category = "Shortcut")
	FText OpenText;

	/** Prompt on the other side, while it's still closed */
	UPROPERTY(EditAnywhere, Category = "Shortcut")
	FText LockedText;

	/** Seconds the opening takes */
	UPROPERTY(EditAnywhere, Category = "Shortcut", meta = (ClampMin = "0"))
	float OpenDuration = 1.5f;

private:

	UFUNCTION()
	void HandleWorldProgressChanged(AZSurvivalGameState* GameState);

	/** Listens to the world progress (a client may get the game state a little later) */
	void BindToGameState();

	/** Opens or closes to match the world progress */
	void RefreshOpenState(bool bAnimate);

	TWeakObjectPtr<AZSurvivalGameState> BoundGameState;

	FTimerHandle BindRetryTimer;

	int32 BindAttempts = 0;

	bool bOpen = false;

	/** 0 = closed, 1 = open, moves over Open Duration */
	float OpenAlpha = 0.f;
};
