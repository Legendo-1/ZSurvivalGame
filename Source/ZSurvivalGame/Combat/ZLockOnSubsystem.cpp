// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZLockOnSubsystem.h"
#include "Engine/World.h"
#include "Combat/ZLockOnTargetComponent.h"

UZLockOnSubsystem* UZLockOnSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UZLockOnSubsystem>() : nullptr;
}

void UZLockOnSubsystem::RegisterTarget(UZLockOnTargetComponent* Target)
{
	if (Target)
	{
		Targets.AddUnique(Target);
	}
}

void UZLockOnSubsystem::UnregisterTarget(UZLockOnTargetComponent* Target)
{
	// also drops entries whose component is already gone
	Targets.RemoveAll([Target](const TWeakObjectPtr<UZLockOnTargetComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == Target;
	});
}
