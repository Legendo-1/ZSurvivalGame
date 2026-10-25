// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZCheatManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ZSurvivalGame.h"
#include "AI/ZAICharacter.h"
#include "AI/ZZombieCharacter.h"
#include "AbilitySystem/ZAttributeSet.h"

bool UZCheatManager::CheckIsHost(const TCHAR* CommandName) const
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Client)
	{
		return true;
	}

	UE_LOG(LogZSurvivalGame, Warning, TEXT("%s only works on the host: type it in the Server window"), CommandName);
	return false;
}

void UZCheatManager::ZSpawnZombies(int32 Count)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	if (!PlayerController || !CheckIsHost(TEXT("ZSpawnZombies")))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// in a row across the view, 8 m ahead of the camera
	const FRotator FlatRotation(0.f, ViewRotation.Yaw, 0.f);
	const FVector Forward = FlatRotation.Vector();
	const FVector Right = FRotationMatrix(FlatRotation).GetUnitAxis(EAxis::Y);
	const int32 NumZombies = FMath::Max(Count, 1);
	const float HalfHeight = GetDefault<AZZombieCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	for (int32 Index = 0; Index < NumZombies; ++Index)
	{
		const float SideOffset = (Index - (NumZombies - 1) * 0.5f) * 150.f;
		const FVector Spot = ViewLocation + Forward * 800.f + Right * SideOffset;

		// drop it onto the ground
		FHitResult Ground;
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZCheatSpawn), false, PlayerController->GetPawn());
		const bool bFoundGround = GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0.f, 0.f, 500.f), Spot - FVector(0.f, 0.f, 2000.f), ECC_Visibility, QueryParams);
		const FVector SpawnLocation = bFoundGround ? FVector(Ground.ImpactPoint) + FVector(0.f, 0.f, HalfHeight + 2.f) : Spot;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<AZZombieCharacter>(AZZombieCharacter::StaticClass(), SpawnLocation, (-Forward).Rotation(), SpawnParams);
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSpawnZombies: spawned %d"), NumZombies);
}

void UZCheatManager::ZKillEnemies()
{
	if (!CheckIsHost(TEXT("ZKillEnemies")))
	{
		return;
	}

	int32 NumKilled = 0;
	for (TActorIterator<AZAICharacter> It(GetWorld()); It; ++It)
	{
		UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
		if (ASC && !It->IsDead())
		{
			ASC->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), 0.f);
			++NumKilled;
		}
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZKillEnemies: killed %d"), NumKilled);
}

void UZCheatManager::ZHeal()
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	UAbilitySystemComponent* ASC = PlayerController ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->GetPawn()) : nullptr;
	if (!ASC || !CheckIsHost(TEXT("ZHeal")))
	{
		return;
	}

	ASC->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), ASC->GetNumericAttribute(UZAttributeSet::GetMaxHealthAttribute()));
	ASC->SetNumericAttributeBase(UZAttributeSet::GetStaminaAttribute(), ASC->GetNumericAttribute(UZAttributeSet::GetMaxStaminaAttribute()));
}
