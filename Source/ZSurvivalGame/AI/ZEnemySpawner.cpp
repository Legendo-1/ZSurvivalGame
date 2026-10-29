// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZEnemySpawner.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AI/ZAIBaseCharacter.h"
#include "AI/ZZombieCharacter.h"

AZEnemySpawner::AZEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// only the server spawns; the enemies replicate themselves
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(Root);
	}
#endif

	EnemyClass = AZZombieCharacter::StaticClass();
}

void AZEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		StartSpawning();
	}
}

void AZEnemySpawner::StartSpawning()
{
	// a placed actor that doesn't replicate also exists on every client, so check the net mode (not HasAuthority)
	if (GetNetMode() == NM_Client)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AZEnemySpawner::SpawnTick, SpawnInterval, true, 0.5f);
}

void AZEnemySpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
}

void AZEnemySpawner::SpawnTick()
{
	AliveEnemies.RemoveAll([](const TWeakObjectPtr<AZAIBaseCharacter>& Enemy) { return !Enemy.IsValid() || Enemy->IsDead(); });

	if (TotalToSpawn > 0 && SpawnedCount >= TotalToSpawn)
	{
		StopSpawning();
		return;
	}

	FVector SpawnLocation;
	if (!EnemyClass || AliveEnemies.Num() >= MaxAlive || !FindSpawnLocation(SpawnLocation))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	const FRotator SpawnRotation(0.f, FMath::FRandRange(0.f, 360.f), 0.f);
	if (AZAIBaseCharacter* Enemy = GetWorld()->SpawnActor<AZAIBaseCharacter>(EnemyClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		AliveEnemies.Add(Enemy);
		++SpawnedCount;
	}
}

bool AZEnemySpawner::FindSpawnLocation(FVector& OutLocation) const
{
	// a random spot around the spawner, dropped onto the ground
	const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);
	const FVector Candidate = GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f);
	const FVector TraceStart = Candidate + FVector(0.f, 0.f, 500.f);
	const FVector TraceEnd = Candidate - FVector(0.f, 0.f, 2000.f);

	FHitResult Ground;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZSpawnerGround), false, this);
	if (!GetWorld()->LineTraceSingleByChannel(Ground, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	// stand on the ground: capsule half height above it
	const float HalfHeight = EnemyClass->GetDefaultObject<AZAIBaseCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	OutLocation = Ground.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 2.f);
	return true;
}
