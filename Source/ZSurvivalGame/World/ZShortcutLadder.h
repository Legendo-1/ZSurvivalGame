// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/ZShortcutBase.h"
#include "ZShortcutLadder.generated.h"

class UStaticMeshComponent;

/**
 *  A ladder hanging out of reach until someone kicks it down from the top, then players climb it with the interact key (up or down)
 *  Place it against the wall with its pivot halfway up the ladder and the green arrow pointing away from the wall
 *  (the bottom floor is in front of the wall, the top floor behind it). Ladder Height is from the bottom floor to the top floor
 */
UCLASS()
class AZShortcutLadder : public AZShortcutBase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LadderMesh;

	/** Where a player climbing up arrives (on the top floor) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> TopExit;

	/** Where a player climbing down arrives (on the bottom floor) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> BottomExit;

public:

	AZShortcutLadder();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** It's kicked down from the top */
	virtual bool IsOnOpenSide(const APawn* Pawn) const override;

	/** Its top end for someone on the top floor (the middle is below the ledge, out of sight), else its middle */
	virtual FVector GetInteractionLocation_Implementation(APawn* Interactor) const override;

protected:

	virtual void ApplyOpenAlpha(float Alpha) override;

	virtual bool CanInteractWhileOpen(const APawn* Interactor) const override { return Interactor != nullptr; }
	virtual FText GetOpenInteractionText(const APawn* Interactor) const override;

	/** Climbs to the other end */
	virtual void InteractWhileOpen(APawn* Interactor) override;

	/** True if the pawn stands on the top floor */
	bool IsAtTop(const APawn* Pawn) const;

	/**
	 *  From the bottom floor to the top floor, in cm
	 *  At most 5.5 m: from the bottom floor the interaction range (4 m) is measured to its middle; higher needs two ladders
	 */
	UPROPERTY(EditAnywhere, Category = "Shortcut|Ladder", meta = (ClampMin = "150", ClampMax = "550"))
	float LadderHeight = 500.f;

	UPROPERTY(EditAnywhere, Category = "Shortcut|Ladder", meta = (ClampMin = "30"))
	float LadderWidth = 60.f;

	/** How much of its height it's pulled up before it's kicked down (0.85: the bottom end hangs far out of jumping reach) */
	UPROPERTY(EditAnywhere, Category = "Shortcut|Ladder", meta = (ClampMin = "0", ClampMax = "1"))
	float RaisedFraction = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Shortcut|Ladder")
	FText ClimbUpText;

	UPROPERTY(EditAnywhere, Category = "Shortcut|Ladder")
	FText ClimbDownText;

private:

	/** Sizes the ladder, its interaction box and the exits to Ladder Height */
	void UpdateLayout();
};
