// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "ZCharacterSpacingComponent.generated.h"

/**
 *  Keeps characters of the other side (players and enemies) from standing too close to this one, so melee swings don't pass behind them (see COMBAT.md → Spacing)
 *  An invisible capsule around the character, Extra Radius wider than its own, that only the other side's capsules bump into:
 *  players keep enemies away and enemies keep players away; players don't block each other, nor do enemies (crowds, doorways)
 *
 *  How: the capsule is on the PlayerSpacing or EnemySpacing collision channel (by the owner's team) and blocks only pawns;
 *  at BeginPlay it makes the owner's own capsule block the other side's channel. Nothing else (traces, the camera, physics, navigation) sees it
 *  Movement collides with it like with any capsule, so the player's machine and the server agree, and an attack's root motion stops at it too
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UZCharacterSpacingComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:

	UZCharacterSpacingComponent();

	/** How much farther apart than their bodies touching this character keeps the other side, in cm (of two characters, the larger one counts) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spacing", meta = (ClampMin = "0", Units = "cm"))
	float ExtraRadius = 45.f;

	/** Switches the spacing on or off, e.g. off when the character dies (call it on every machine) */
	UFUNCTION(BlueprintCallable, Category = "Spacing")
	void SetSpacingEnabled(bool bEnabled);

protected:

	/** Takes the size of the owner's capsule plus Extra Radius */
	virtual void OnRegister() override;

	/** Picks the channel by the owner's team and makes the owner's capsule bump into the other side's spacing */
	virtual void BeginPlay() override;

private:

	/** Sets the size from the owner's capsule */
	void FitToOwnerCapsule();
};
