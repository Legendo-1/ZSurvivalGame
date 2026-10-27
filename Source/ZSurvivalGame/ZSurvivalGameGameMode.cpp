// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGameGameMode.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGamePlayerState.h"
#include "ZSurvivalGameState.h"
#include "SaveGame/ZPlayerSaveComponent.h"
#include "SaveGame/ZSaveSubsystem.h"

AZSurvivalGameGameMode::AZSurvivalGameGameMode()
{
	// the PlayerState holds each player's ability system, health and stamina
	PlayerStateClass = AZSurvivalGamePlayerState::StaticClass();

	// the GameState holds the world progress
	GameStateClass = AZSurvivalGameState::StaticClass();
}

void AZSurvivalGameGameMode::InitGameState()
{
	Super::InitGameState();

	// the world as the host left it (it's the host's: players who join keep their own world as it was)
	AZSurvivalGameState* ZGameState = GetGameState<AZSurvivalGameState>();
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(this);
	if (ZGameState && Saves)
	{
		ZGameState->SetWorldProgress(Saves->GetActiveWorld().Progress);
		ZGameState->OnWorldProgressChanged.AddUniqueDynamic(this, &AZSurvivalGameGameMode::HandleWorldProgressChanged);
	}
}

void AZSurvivalGameGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Remnant style: each player brings their character from their own PC, and spawns once it's in the game
	UZPlayerSaveComponent* PlayerSave = NewPlayer ? NewPlayer->FindComponentByClass<UZPlayerSaveComponent>() : nullptr;
	if (PlayerSave && !PlayerSave->IsCharacterReady())
	{
		PlayerSave->OnCharacterReady.AddUObject(this, &AZSurvivalGameGameMode::HandlePlayerCharacterReady);

		if (NewPlayer->IsLocalController())
		{
			// the host's own, from this PC's save right away (spawns before this returns)
			PlayerSave->LoadLocalCharacter();
		}
		else
		{
			// a joining player's comes from their PC
			PlayerSave->WaitForCharacter();
		}

		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

bool AZSurvivalGameGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	const UZPlayerSaveComponent* PlayerSave = Player ? Player->FindComponentByClass<UZPlayerSaveComponent>() : nullptr;
	return (!PlayerSave || PlayerSave->IsCharacterReady()) && Super::PlayerCanRestart_Implementation(Player);
}

void AZSurvivalGameGameMode::HandlePlayerCharacterReady(UZPlayerSaveComponent* PlayerSave)
{
	PlayerSave->OnCharacterReady.RemoveAll(this);

	APlayerController* Player = Cast<APlayerController>(PlayerSave->GetOwner());
	if (Player && !Player->GetPawn())
	{
		Super::HandleStartingNewPlayer_Implementation(Player);
	}
}

void AZSurvivalGameGameMode::HandleWorldProgressChanged(AZSurvivalGameState* ChangedGameState)
{
	const APlayerController* HostPlayer = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController(GetWorld()) : nullptr;
	if (UZPlayerSaveComponent* HostSave = HostPlayer ? HostPlayer->FindComponentByClass<UZPlayerSaveComponent>() : nullptr)
	{
		HostSave->MarkDirty();
	}
}

void AZSurvivalGameGameMode::RequestRespawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	FTimerHandle RespawnTimer;
	GetWorldTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateUObject(this, &AZSurvivalGameGameMode::Respawn, TWeakObjectPtr<AController>(Controller)), FMath::Max(RespawnDelay, 0.01f), false);
}

AActor* AZSurvivalGameGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (AActor* FreeStart = Super::ChoosePlayerStart_Implementation(Player))
	{
		return FreeStart;
	}

	// every start is crowded (e.g. by zombies or a body): use one anyway, the spawn moves the character to a free spot
	TArray<APlayerStart*> PlayerStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		PlayerStarts.Add(*It);
	}

	UE_CLOG(PlayerStarts.Num() == 0, LogZSurvivalGame, Warning, TEXT("The level has no Player Start: players spawn at the world origin. Add Player Starts (one per player) to the level."));
	return PlayerStarts.Num() > 0 ? PlayerStarts[FMath::RandRange(0, PlayerStarts.Num() - 1)] : nullptr;
}

APawn* AZSurvivalGameGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient; // We never want to save default player pawns into a map
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	UE_CLOG(!ResultPawn, LogZSurvivalGame, Warning, TEXT("Couldn't spawn the player's %s at %s"), *GetNameSafe(PawnClass), *SpawnTransform.ToHumanReadableString());
	return ResultPawn;
}

void AZSurvivalGameGameMode::Respawn(TWeakObjectPtr<AController> Controller)
{
	AController* PlayerController = Controller.Get();
	if (!PlayerController)
	{
		return;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Respawning '%s'"), *GetNameSafe(PlayerController));

	// leave the dead body for a while, then spawn a new character at a Player Start
	if (APawn* DeadPawn = PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
		DeadPawn->SetLifeSpan(DeadBodyLifeSpan);
	}

	RestartPlayer(PlayerController);
}
