// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZHitFlashComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ZSurvivalGame.h"
#include "Combat/ZCombatSettings.h"

namespace
{
	const FName FlashAmountParameter(TEXT("FlashAmount"));
	const FName FlashColorParameter(TEXT("FlashColor"));
}

UZHitFlashComponent::UZHitFlashComponent()
{
	// ticks only while a flash fades
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UZHitFlashComponent::BeginPlay()
{
	Super::BeginPlay();

	// nothing to see on a server without a screen
	if (GetNetMode() != NM_DedicatedServer)
	{
		UZCombatSettings::Get()->HitFlashMaterial.LoadSynchronous();
	}
}

void UZHitFlashComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFlash();

	Super::EndPlay(EndPlayReason);
}

UMeshComponent* UZHitFlashComponent::GetFlashMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UZHitFlashComponent::Flash()
{
	const UZCombatSettings* Settings = UZCombatSettings::Get();
	UMaterialInterface* Material = Settings->HitFlashMaterial.Get();
	UMeshComponent* Mesh = GetFlashMesh();
	if (!Material || !Mesh || Settings->HitFlashTime <= 0.f || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!FlashMaterial || FlashMaterial->Parent != Material)
	{
		FlashMaterial = UMaterialInstanceDynamic::Create(Material, this);
	}

	// remember what lay over the mesh before (normally nothing), unless a flash is still on
	if (FlashTimeLeft <= 0.f)
	{
		OverlayBeforeFlash = Mesh->GetOverlayMaterial();
	}

	FlashMaterial->SetVectorParameterValue(FlashColorParameter, Settings->HitFlashColor);
	FlashMaterial->SetScalarParameterValue(FlashAmountParameter, 1.f);
	Mesh->SetOverlayMaterial(FlashMaterial);

	FlashTimeLeft = Settings->HitFlashTime;
	SetComponentTickEnabled(true);
	UE_LOG(LogZSurvivalGame, VeryVerbose, TEXT("'%s' flashes from a hit"), *GetNameSafe(GetOwner()));
}

void UZHitFlashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FlashTimeLeft -= DeltaTime;
	if (FlashTimeLeft <= 0.f)
	{
		StopFlash();
		return;
	}

	// fades out
	if (FlashMaterial)
	{
		FlashMaterial->SetScalarParameterValue(FlashAmountParameter, FMath::Clamp(FlashTimeLeft / FMath::Max(UZCombatSettings::Get()->HitFlashTime, KINDA_SMALL_NUMBER), 0.f, 1.f));
	}
}

void UZHitFlashComponent::StopFlash()
{
	UMeshComponent* Mesh = GetFlashMesh();
	if (Mesh && FlashMaterial && Mesh->GetOverlayMaterial() == FlashMaterial)
	{
		Mesh->SetOverlayMaterial(OverlayBeforeFlash);
	}

	FlashTimeLeft = 0.f;
	OverlayBeforeFlash = nullptr;
	SetComponentTickEnabled(false);
}
