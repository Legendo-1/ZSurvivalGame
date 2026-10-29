// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ZLockOnTargetComponent.generated.h"

/**
 *  A point players can lock on to (see COMBAT.md → Lock-on). The camera aims at it and the lock-on marker sits on it
 *  Every AI character has one on its chest; a big enemy can get more (one per body part): add the component in its Blueprint and attach it to a bone
 *  Registers itself with the world's lock-on subsystem, so finding targets never searches the whole level
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UZLockOnTargetComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UZLockOnTargetComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** True if players may lock on to this point now: it's switched on and its owner is alive */
	UFUNCTION(BlueprintPure, Category = "Lock-On")
	bool IsTargetable() const;

	/** Server: switches this point on or off for everyone, e.g. for a boss phase where it can't be targeted */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lock-On")
	void SetCanBeLockedOn(bool bNewCanBeLockedOn);

	bool CanBeLockedOn() const { return bCanBeLockedOn; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Players may lock on to this point (replicated, so the server can switch it for everyone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Lock-On")
	bool bCanBeLockedOn = true;
};
