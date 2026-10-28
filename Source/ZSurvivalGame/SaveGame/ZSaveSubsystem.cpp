// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveGame/ZSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGamePlayerController.h"
#include "SaveGame/ZPlayerSaveComponent.h"
#include "SaveGame/ZSaveGame.h"
#include "SaveGame/ZSaveSettings.h"
#include "SaveGame/ZSaveStorage.h"

#define LOCTEXT_NAMESPACE "ZSave"

namespace
{
	FString CleanCharacterName(const FString& Name)
	{
		const FString Clean = Name.TrimStartAndEnd().Left(ZMaxCharacterNameLength);
		return Clean.IsEmpty() ? UZSaveSettings::Get()->DefaultCharacterName : Clean;
	}
}

UZSaveSubsystem* UZSaveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UZSaveSubsystem>() : nullptr;
}

void UZSaveSubsystem::Deinitialize()
{
	// a save still being written finishes before the game closes
	if (Storage)
	{
		Storage->Flush();
	}

	Super::Deinitialize();
}

UZSaveStorage* UZSaveSubsystem::GetStorage()
{
	if (Storage)
	{
		return Storage;
	}

	UClass* StorageClass = UZSaveSettings::Get()->StorageClass.LoadSynchronous();
	if (!StorageClass || StorageClass->HasAnyClassFlags(CLASS_Abstract))
	{
		StorageClass = UZLocalSaveStorage::StaticClass();
	}

	// a set of saves of its own for each test profile (-SaveProfile=Name) and each extra Play-In-Editor window, as if they were different PCs
	FString Profile;
	FParse::Value(FCommandLine::Get(), TEXT("SaveProfile="), Profile);
	const FWorldContext* WorldContext = GetGameInstance()->GetWorldContext();
	if (Profile.IsEmpty() && WorldContext && WorldContext->WorldType == EWorldType::PIE && WorldContext->PIEInstance > 0)
	{
		Profile = FString::Printf(TEXT("PIE%d"), WorldContext->PIEInstance);
	}

	Storage = NewObject<UZSaveStorage>(this, StorageClass);
	Storage->Initialize(Profile);
	UE_LOG(LogZSurvivalGame, Log, TEXT("Save: saves are kept in %s"), *Storage->Describe());
	return Storage;
}

bool UZSaveSubsystem::IsSavingEnabled() const
{
	if (GetGameInstance()->IsDedicatedServerInstance())
	{
		return false;
	}

#if WITH_EDITOR
	if (GIsEditor && !UZSaveSettings::Get()->bUseSavesInEditor)
	{
		return false;
	}
#endif

	return true;
}

TArray<FZSaveSlotInfo> UZSaveSubsystem::GetSaveSlots()
{
	TArray<FZSaveSlotInfo> Slots;
	for (const FString& SlotName : GetStorage()->GetSaveNames())
	{
		FZSaveSlotInfo& Info = Slots.AddDefaulted_GetRef();

		// the slot in use is newest in memory (a write may still be on its way)
		const UZSaveGame* Save = SlotName == ActiveSlotName && ActiveSave ? ActiveSave.Get() : ReadSlotData(SlotName);
		if (Save)
		{
			Info = Save->MakeSlotInfo(SlotName);
			Info.bCanLoad = Save->SaveVersion <= static_cast<int32>(EZSaveVersion::Latest);
		}
		else
		{
			Info.SlotName = SlotName;
			Info.bCanLoad = false;
		}

		Info.bIsActive = SlotName == ActiveSlotName;
	}

	Slots.Sort([](const FZSaveSlotInfo& A, const FZSaveSlotInfo& B) { return A.LastSaved > B.LastSaved; });
	return Slots;
}

FZSaveSlotInfo UZSaveSubsystem::GetActiveSlotInfo()
{
	EnsureActiveSlot();

	FZSaveSlotInfo Info = ActiveSave->MakeSlotInfo(ActiveSlotName);
	Info.PlayTime += FTimespan::FromSeconds(FPlatformTime::Seconds() - PlayTimeCountedUntil);
	Info.bIsActive = true;
	return Info;
}

FString UZSaveSubsystem::CreateSlot(const FString& CharacterName)
{
	if (!IsSavingEnabled())
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: saving is off, so no slot was made"));
		return FString();
	}

	SaveRunningGame();

	const FString SlotName = MakeUniqueSlotName();
	ActivateSlot(SlotName, MakeNewSave(CharacterName));
	if (!WriteActiveSave(true))
	{
		return FString();
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Save: made slot '%s' for the new character '%s'"), *SlotName, *ActiveSave->Character.CharacterName);
	return SlotName;
}

bool UZSaveSubsystem::SelectSlot(const FString& SlotName)
{
	if (!IsSavingEnabled())
	{
		return false;
	}

	if (SlotName == ActiveSlotName && ActiveSave)
	{
		return true;
	}

	UZSaveGame* Save = ReadSlot(SlotName);
	if (!Save)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: slot '%s' can't be loaded (missing, damaged, or from a newer version of the game)"), *SlotName);
		return false;
	}

	// the game running now keeps its progress
	SaveRunningGame();
	ActivateSlot(SlotName, Save);
	return true;
}

bool UZSaveSubsystem::LoadSlot(const FString& SlotName)
{
	if (!SelectSlot(SlotName))
	{
		return false;
	}

	FString Map = ActiveSave->World.MapName;
	if (Map.IsEmpty() || !FPackageName::DoesPackageExist(Map))
	{
		Map = UGameMapsSettings::GetGameDefaultMap();
	}

	// as the host: friends can join this character's world
	UE_LOG(LogZSurvivalGame, Log, TEXT("Save: continuing slot '%s' in %s"), *SlotName, *Map);
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*Map), true, TEXT("listen"));
	return true;
}

void UZSaveSubsystem::SaveNow()
{
	AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetGameInstance()->GetFirstLocalPlayerController());
	if (!IsSavingEnabled())
	{
		if (PlayerController)
		{
			PlayerController->ShowMessage(LOCTEXT("SavingOff", "Saving is off (Project Settings > Saving > Use Saves In Editor)."));
		}

		return;
	}

	UZPlayerSaveComponent* LocalSave = FindLocalPlayerSave();
	if (!LocalSave)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Save: there's no game running to save"));
		return;
	}

	bSaveRequestedByPlayer = true;
	LocalSave->RequestSave();
}

bool UZSaveSubsystem::DeleteSlot(const FString& SlotName)
{
	if (SlotName.IsEmpty() || SlotName.Equals(ActiveSlotName, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: slot '%s' is in use, so it can't be deleted (choose another slot first)"), *SlotName);
		return false;
	}

	if (!GetStorage()->Delete(SlotName))
	{
		return false;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Save: deleted slot '%s'"), *SlotName);
	OnSaveSlotsChanged.Broadcast();
	return true;
}

const FZCharacterSaveData& UZSaveSubsystem::GetActiveCharacter()
{
	EnsureActiveSlot();
	return ActiveSave->Character;
}

const FZWorldSaveData& UZSaveSubsystem::GetActiveWorld()
{
	EnsureActiveSlot();
	return ActiveSave->World;
}

bool UZSaveSubsystem::WriteActiveSlot(const FZCharacterSaveData& Character, const FZWorldSaveData* World, bool bWaitUntilWritten)
{
	if (!IsSavingEnabled() || ActiveSlotName.IsEmpty() || !ActiveSave)
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	if (bKeepTestSave)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Save: not saved: slot '%s' keeps what ZBreakSave wrote until the next game"), *ActiveSlotName);
		return false;
	}
#endif

	// never another character over this one, e.g. from a game still running after another slot was chosen
	if (Character.CharacterId != ActiveSave->Character.CharacterId)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Save: not saved: the game's character '%s' isn't the one in slot '%s' (another slot was chosen since)"),
			*Character.CharacterName, *ActiveSlotName);
		return false;
	}

	ActiveSave->Character = Character;
	if (World)
	{
		ActiveSave->World = *World;
	}

	return WriteActiveSave(bWaitUntilWritten);
}

#if !UE_BUILD_SHIPPING
UZSaveGame* UZSaveSubsystem::GetActiveSaveForTesting()
{
	EnsureActiveSlot();
	return ActiveSave;
}

bool UZSaveSubsystem::WriteActiveSaveForTesting()
{
	if (!IsSavingEnabled() || ActiveSlotName.IsEmpty() || !WriteActiveSave(true))
	{
		return false;
	}

	// the game running now would save its character over it when it ends
	bKeepTestSave = true;
	return true;
}
#endif

void UZSaveSubsystem::EnsureActiveSlot()
{
	if (ActiveSave)
	{
		return;
	}

	// nothing is saved: a new character for this game only
	if (!IsSavingEnabled())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Save: saving is off here, so this game plays a new character that isn't saved"));
		ActivateSlot(FString(), MakeNewSave(FString()));
		return;
	}

	// a slot named on the command line (e.g. for test runs): that one, made new if it isn't there
	FString SlotName;
	if (FParse::Value(FCommandLine::Get(), TEXT("SaveSlot="), SlotName) && !SlotName.IsEmpty())
	{
		if (UZSaveGame* Save = ReadSlot(SlotName))
		{
			ActivateSlot(SlotName, Save);
			return;
		}

		// there, but it can't be read: it stays untouched
		if (GetStorage()->GetSaveNames().ContainsByPredicate([&SlotName](const FString& Name) { return Name.Equals(SlotName, ESearchCase::IgnoreCase); }))
		{
			UE_LOG(LogZSurvivalGame, Error, TEXT("Save: slot '%s' can't be loaded (damaged, or from a newer version of the game). This game plays a new character that isn't saved, so the slot stays as it is."), *SlotName);
			ActivateSlot(FString(), MakeNewSave(FString()));
			return;
		}

		ActivateSlot(SlotName, MakeNewSave(FString()));
		WriteActiveSave(true);
		return;
	}

	// the most recently played slot that can be loaded
	for (const FZSaveSlotInfo& Slot : GetSaveSlots())
	{
		if (UZSaveGame* Save = Slot.bCanLoad ? ReadSlot(Slot.SlotName) : nullptr)
		{
			ActivateSlot(Slot.SlotName, Save);
			return;
		}
	}

	// the first time: a new character in a new slot (slots that can't be loaded keep their names and stay untouched)
	ActivateSlot(MakeUniqueSlotName(), MakeNewSave(FString()));
	WriteActiveSave(true);
}

void UZSaveSubsystem::ActivateSlot(const FString& SlotName, UZSaveGame* Save)
{
	ActiveSlotName = SlotName;
	ActiveSave = Save;
	PlayTimeCountedUntil = FPlatformTime::Seconds();

#if !UE_BUILD_SHIPPING
	bKeepTestSave = false;
#endif

	UE_CLOG(!SlotName.IsEmpty(), LogZSurvivalGame, Log, TEXT("Save: using slot '%s': %s"), *SlotName, *Save->MakeSlotInfo(SlotName).CharacterName);
	OnSaveSlotsChanged.Broadcast();
}

UZSaveGame* UZSaveSubsystem::ReadSlot(const FString& SlotName)
{
	UZSaveGame* Save = ReadSlotData(SlotName);
	if (!Save || !Save->UpgradeToLatest())
	{
		return nullptr;
	}

	// every character has an id of its own (a save edited by hand might not)
	if (!Save->Character.CharacterId.IsValid())
	{
		Save->Character.CharacterId = FGuid::NewGuid();
	}

	return Save;
}

UZSaveGame* UZSaveSubsystem::ReadSlotData(const FString& SlotName)
{
	TArray<uint8> Data;
	if (SlotName.IsEmpty() || !GetStorage()->Read(SlotName, Data))
	{
		return nullptr;
	}

	UZSaveGame* Save = Cast<UZSaveGame>(UGameplayStatics::LoadGameFromMemory(Data));
	UE_CLOG(!Save, LogZSurvivalGame, Warning, TEXT("Save: slot '%s' isn't a save of this game"), *SlotName);
	return Save;
}

UZSaveGame* UZSaveSubsystem::MakeNewSave(const FString& CharacterName)
{
	UZSaveGame* Save = NewObject<UZSaveGame>(this);
	Save->Character.CharacterId = FGuid::NewGuid();
	Save->Character.CharacterName = CleanCharacterName(CharacterName);
	Save->CreatedAt = FDateTime::UtcNow();
	Save->SavedAt = Save->CreatedAt;
	return Save;
}

FString UZSaveSubsystem::MakeUniqueSlotName()
{
	const TArray<FString> Existing = GetStorage()->GetSaveNames();
	for (int32 Number = 1; ; ++Number)
	{
		const FString Name = FString::Printf(TEXT("Slot%d"), Number);
		if (!Existing.ContainsByPredicate([&Name](const FString& Other) { return Other.Equals(Name, ESearchCase::IgnoreCase); }))
		{
			return Name;
		}
	}
}

void UZSaveSubsystem::SaveRunningGame()
{
	if (UZPlayerSaveComponent* LocalSave = FindLocalPlayerSave())
	{
		LocalSave->RequestSave(true);
	}
}

bool UZSaveSubsystem::WriteActiveSave(bool bWaitUntilWritten)
{
	const double Now = FPlatformTime::Seconds();
	ActiveSave->PlayTimeSeconds += FMath::Max(Now - PlayTimeCountedUntil, 0.0);
	PlayTimeCountedUntil = Now;
	ActiveSave->SavedAt = FDateTime::UtcNow();
	ActiveSave->SaveVersion = static_cast<int32>(EZSaveVersion::Latest);

	TArray<uint8> Data;
	if (!UGameplayStatics::SaveGameToMemory(ActiveSave, Data))
	{
		UE_LOG(LogZSurvivalGame, Error, TEXT("Save: couldn't put slot '%s' into a save file"), *ActiveSlotName);
		HandleWriteDone(false);
		return false;
	}

	if (bWaitUntilWritten)
	{
		// this has the latest data, so a write waiting for its turn isn't needed any more
		bWriteWaiting = false;
		const bool bSuccess = GetStorage()->Write(ActiveSlotName, Data);
		HandleWriteDone(bSuccess);
		return bSuccess;
	}

	// one background write at a time; a waiting one takes the latest data when it's its turn
	if (bWriteInProgress)
	{
		bWriteWaiting = true;
		return true;
	}

	bWriteInProgress = true;
	GetStorage()->WriteAsync(ActiveSlotName, MoveTemp(Data), [WeakThis = TWeakObjectPtr<UZSaveSubsystem>(this)](bool bSuccess)
	{
		if (UZSaveSubsystem* This = WeakThis.Get())
		{
			This->HandleAsyncWriteDone(bSuccess);
		}
	});

	return true;
}

void UZSaveSubsystem::HandleAsyncWriteDone(bool bSuccess)
{
	bWriteInProgress = false;
	HandleWriteDone(bSuccess);

	if (bWriteWaiting)
	{
		bWriteWaiting = false;
		WriteActiveSave(false);
	}
}

void UZSaveSubsystem::HandleWriteDone(bool bSuccess)
{
	if (bSuccess)
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("Save: wrote slot '%s' (%s, %d items)"), *ActiveSlotName, *ActiveSave->Character.CharacterName, ActiveSave->Character.Inventory.Num());
	}
	else
	{
		UE_LOG(LogZSurvivalGame, Error, TEXT("Save: couldn't write slot '%s'"), *ActiveSlotName);
	}

	OnGameSaved.Broadcast(bSuccess);

	if (bSaveRequestedByPlayer)
	{
		bSaveRequestedByPlayer = false;
		if (AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))
		{
			PlayerController->ShowMessage(bSuccess ? LOCTEXT("Saved", "Game saved.") : LOCTEXT("SaveFailed", "The game couldn't be saved."));
		}
	}
}

UZPlayerSaveComponent* UZSaveSubsystem::FindLocalPlayerSave() const
{
	const APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	return PlayerController ? PlayerController->FindComponentByClass<UZPlayerSaveComponent>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
