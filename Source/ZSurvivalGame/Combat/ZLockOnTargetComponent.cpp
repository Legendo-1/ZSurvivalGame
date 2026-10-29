// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZLockOnTargetComponent.h"
#include "Net/UnrealNetwork.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "Combat/ZLockOnSubsystem.h"

UZLockOnTargetComponent::UZLockOnTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UZLockOnTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UZLockOnTargetComponent, bCanBeLockedOn);
}

bool UZLockOnTargetComponent::IsTargetable() const
{
	const AActor* Owner = GetOwner();
	return bCanBeLockedOn && IsRegistered() && Owner && !Owner->IsActorBeingDestroyed() && !UZCombatLibrary::IsDead(Owner);
}

void UZLockOnTargetComponent::SetCanBeLockedOn(bool bNewCanBeLockedOn)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bCanBeLockedOn = bNewCanBeLockedOn;
	}
}

void UZLockOnTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UZLockOnSubsystem* Subsystem = UZLockOnSubsystem::Get(this))
	{
		Subsystem->RegisterTarget(this);
	}
}

void UZLockOnTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UZLockOnSubsystem* Subsystem = UZLockOnSubsystem::Get(this))
	{
		Subsystem->UnregisterTarget(this);
	}

	Super::EndPlay(EndPlayReason);
}
