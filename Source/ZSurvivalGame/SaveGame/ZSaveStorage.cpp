// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveGame/ZSaveStorage.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "ZSurvivalGame.h"

namespace
{
	// "ZSV1" at the start of every save file, then the payload's size and checksum
	constexpr uint32 SaveFileMagic = 0x3156535A;
	constexpr int32 SaveFileHeaderSize = 3 * sizeof(uint32);

	const TCHAR* SaveExtension = TEXT(".sav");
	const TCHAR* TempSuffix = TEXT(".tmp");
	const TCHAR* BackupSuffix = TEXT(".bak");
}

void UZSaveStorage::WriteAsync(const FString& Name, TArray<uint8> Data, TFunction<void(bool bSuccess)> OnDone)
{
	const bool bSuccess = Write(Name, Data);
	if (OnDone)
	{
		OnDone(bSuccess);
	}
}

void UZLocalSaveStorage::Initialize(const FString& InProfile)
{
	Super::Initialize(InProfile);

	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames"));
	if (!Profile.IsEmpty())
	{
		Directory /= FPaths::MakeValidFileName(Profile);
	}

	WritePipe = MakeUnique<UE::Tasks::FPipe>(TEXT("ZSaveWrites"));
}

void UZLocalSaveStorage::BeginDestroy()
{
	// a write still going on finishes first
	Flush();

	Super::BeginDestroy();
}

FString UZLocalSaveStorage::GetFilePath(const FString& Name) const
{
	// slashes and other characters a file name can't have become _, so a name never points outside the folder
	return Directory / FPaths::MakeValidFileName(Name) + SaveExtension;
}

TArray<FString> UZLocalSaveStorage::GetSaveNames() const
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*")), true, false);

	// a save can be there only as its new file or its backup (e.g. after a crash while saving)
	TSet<FString> Names;
	for (const FString& File : Files)
	{
		FString Name = File;
		if (!Name.RemoveFromEnd(TempSuffix))
		{
			Name.RemoveFromEnd(BackupSuffix);
		}

		if (Name.RemoveFromEnd(SaveExtension))
		{
			Names.Add(Name);
		}
	}

	return Names.Array();
}

bool UZLocalSaveStorage::Read(const FString& Name, TArray<uint8>& OutData) const
{
	const FString Path = GetFilePath(Name);

	// the save; else the new one a crash interrupted before it took the save's place; else the save before
	for (const FString& Candidate : { Path, Path + TempSuffix, Path + BackupSuffix })
	{
		if (ReadFile(Candidate, OutData))
		{
			UE_CLOG(Candidate != Path, LogZSurvivalGame, Warning, TEXT("Save: '%s' was missing or damaged, so its backup '%s' was used"), *Path, *FPaths::GetCleanFilename(Candidate));
			return true;
		}
	}

	return false;
}

bool UZLocalSaveStorage::Write(const FString& Name, const TArray<uint8>& Data)
{
	// after any background writes, so an older save never lands on top of this one
	Flush();
	return WriteFile(GetFilePath(Name), Data);
}

void UZLocalSaveStorage::WriteAsync(const FString& Name, TArray<uint8> Data, TFunction<void(bool bSuccess)> OnDone)
{
	WritePipe->Launch(TEXT("ZSaveWrite"), [Path = GetFilePath(Name), Data = MoveTemp(Data), OnDone = MoveTemp(OnDone)]()
	{
		const bool bSuccess = WriteFile(Path, Data);
		AsyncTask(ENamedThreads::GameThread, [OnDone, bSuccess]()
		{
			if (OnDone)
			{
				OnDone(bSuccess);
			}
		});
	});
}

bool UZLocalSaveStorage::Delete(const FString& Name)
{
	Flush();

	const FString Path = GetFilePath(Name);
	IFileManager& FileManager = IFileManager::Get();
	FileManager.Delete(*(Path + TempSuffix), false, true, true);
	FileManager.Delete(*(Path + BackupSuffix), false, true, true);
	return FileManager.Delete(*Path, false, true, true);
}

void UZLocalSaveStorage::Flush()
{
	if (WritePipe)
	{
		WritePipe->WaitUntilEmpty();
	}
}

bool UZLocalSaveStorage::WriteFile(const FString& Path, const TArray<uint8>& Data)
{
	TArray<uint8> FileData;
	FileData.Reserve(SaveFileHeaderSize + Data.Num());

	FMemoryWriter Writer(FileData);
	uint32 Magic = SaveFileMagic;
	uint32 Size = static_cast<uint32>(Data.Num());
	uint32 Checksum = FCrc::MemCrc32(Data.GetData(), Data.Num());
	Writer << Magic << Size << Checksum;
	FileData.Append(Data);

	// the new file is complete before it replaces anything, and the old save stays as the backup
	const FString TempPath = Path + TempSuffix;
	if (!FFileHelper::SaveArrayToFile(FileData, *TempPath))
	{
		UE_LOG(LogZSurvivalGame, Error, TEXT("Save: couldn't write '%s' (disk full, or no permission?)"), *TempPath);
		return false;
	}

	IFileManager& FileManager = IFileManager::Get();
	if (FileManager.FileExists(*Path))
	{
		FileManager.Move(*(Path + BackupSuffix), *Path, true, true, false, true);
	}

	if (!FileManager.Move(*Path, *TempPath, true, true, false, true))
	{
		UE_LOG(LogZSurvivalGame, Error, TEXT("Save: couldn't replace '%s' with the new save"), *Path);
		return false;
	}

	return true;
}

bool UZLocalSaveStorage::ReadFile(const FString& Path, TArray<uint8>& OutData)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Path, FILEREAD_Silent) || FileData.Num() < SaveFileHeaderSize)
	{
		return false;
	}

	FMemoryReader Reader(FileData);
	uint32 Magic = 0;
	uint32 Size = 0;
	uint32 Checksum = 0;
	Reader << Magic << Size << Checksum;

	const uint8* Payload = FileData.GetData() + SaveFileHeaderSize;
	const int32 PayloadSize = FileData.Num() - SaveFileHeaderSize;
	if (Magic != SaveFileMagic || Size != static_cast<uint32>(PayloadSize) || Checksum != FCrc::MemCrc32(Payload, PayloadSize))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: '%s' is damaged"), *Path);
		return false;
	}

	OutData = TArray<uint8>(Payload, PayloadSize);
	return true;
}
