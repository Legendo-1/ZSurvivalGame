// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCharacterSpacingComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Character.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZCombatLibrary.h"

UZCharacterSpacingComponent::UZCharacterSpacingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// a query-only shape that only pawns bump into: no traces, overlaps, physics or navigation
	SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ZObjectChannel_EnemySpacing);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CanCharacterStepUpOn = ECB_No;
	SetShouldUpdatePhysicsVolume(false);
	bHiddenInGame = true;
}

void UZCharacterSpacingComponent::OnRegister()
{
	FitToOwnerCapsule();

	Super::OnRegister();
}

void UZCharacterSpacingComponent::FitToOwnerCapsule()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* OwnerCapsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (OwnerCapsule && OwnerCapsule != this)
	{
		const float Radius = OwnerCapsule->GetUnscaledCapsuleRadius() + ExtraRadius;
		SetCapsuleSize(Radius, FMath::Max(OwnerCapsule->GetUnscaledCapsuleHalfHeight(), Radius), false);
	}
}

void UZCharacterSpacingComponent::BeginPlay()
{
	Super::BeginPlay();

	// the owner's capsule may have been resized in its Blueprint after we registered
	FitToOwnerCapsule();

	// players' spacing is for enemies and the other way around (friendly NPCs are on the players' team)
	const bool bPlayerSide = UZCombatLibrary::GetTeam(GetOwner()) == FGenericTeamId(static_cast<uint8>(EZTeam::Players));
	SetCollisionObjectType(bPlayerSide ? ZObjectChannel_PlayerSpacing : ZObjectChannel_EnemySpacing);

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		UCapsuleComponent* OwnerCapsule = Character->GetCapsuleComponent();
		OwnerCapsule->SetCollisionResponseToChannel(bPlayerSide ? ZObjectChannel_EnemySpacing : ZObjectChannel_PlayerSpacing, ECR_Block);
		OwnerCapsule->SetCollisionResponseToChannel(bPlayerSide ? ZObjectChannel_PlayerSpacing : ZObjectChannel_EnemySpacing, ECR_Ignore);
	}
}

void UZCharacterSpacingComponent::SetSpacingEnabled(bool bEnabled)
{
	SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
