// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ZLockOnSubsystem.generated.h"

class UZLockOnTargetComponent;

/**
 *  Every lock-on point in the world (UZLockOnTargetComponent registers itself), so looking for a target only checks those instead of every actor
 *  Exists on every machine; only players' own machines search it (their lock-on component)
 */
UCLASS()
class UZLockOnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** The subsystem of this object's world, or null */
	static UZLockOnSubsystem* Get(const UObject* WorldContextObject);

	void RegisterTarget(UZLockOnTargetComponent* Target);
	void UnregisterTarget(UZLockOnTargetComponent* Target);

	/** Every registered lock-on point (some may be switched off or belong to the dead: check UZLockOnTargetComponent::IsTargetable) */
	const TArray<TWeakObjectPtr<UZLockOnTargetComponent>>& GetTargets() const { return Targets; }

private:

	TArray<TWeakObjectPtr<UZLockOnTargetComponent>> Targets;
};
