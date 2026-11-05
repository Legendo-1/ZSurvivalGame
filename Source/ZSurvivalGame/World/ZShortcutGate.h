// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "World/ZShortcutBase.h"
#include "ZShortcutGate.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EZShortcutGateMotion : uint8
{
	/** Rises like a portcullis */
	SlideUp,

	/** Swings open around its left edge (seen from the open side), away from whoever opens it */
	Swing
};

/**
 *  An iron gate that opens once, from its open side only (the green arrow)
 *  Place it with its pivot on the floor in the middle of the opening; Gate Width and Gate Height are the opening's size
 *  Swap the Gate Mesh for the art; its size comes from the cube until then
 */
UCLASS()
class AZShortcutGate : public AZShortcutBase
{
	GENERATED_BODY()

	/** What moves when it opens */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> GatePivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GateMesh;

public:

	AZShortcutGate();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	virtual void ApplyOpenAlpha(float Alpha) override;

	UPROPERTY(EditAnywhere, Category = "Shortcut|Gate")
	EZShortcutGateMotion Motion = EZShortcutGateMotion::SlideUp;

	/** Width of the opening, in cm */
	UPROPERTY(EditAnywhere, Category = "Shortcut|Gate", meta = (ClampMin = "100"))
	float GateWidth = 400.f;

	/** Height of the opening, in cm */
	UPROPERTY(EditAnywhere, Category = "Shortcut|Gate", meta = (ClampMin = "200"))
	float GateHeight = 400.f;

	UPROPERTY(EditAnywhere, Category = "Shortcut|Gate", meta = (ClampMin = "1"))
	float GateThickness = 20.f;

	/** Slide Up: how far it rises, in cm (0 = its height + 20 cm, so a player fits under it) */
	UPROPERTY(EditAnywhere, Category = "Shortcut|Gate", meta = (ClampMin = "0", EditCondition = "Motion == EZShortcutGateMotion::SlideUp"))
	float SlideHeight = 0.f;

	/** Swing: how far it turns, in degrees */
	UPROPERTY(EditAnywhere, Category = "Shortcut|Gate", meta = (ClampMin = "0", ClampMax = "180", EditCondition = "Motion == EZShortcutGateMotion::Swing"))
	float SwingAngle = 100.f;

private:

	/** Sizes the gate and the interaction box to the opening */
	void UpdateLayout();
};
