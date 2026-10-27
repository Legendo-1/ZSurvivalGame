// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Tasks/Pipe.h"
#include "ZSaveStorage.generated.h"

/**
 *  Where saves are kept. The save system only hands it bytes under a name, so a child class can keep them anywhere:
 *  files on this PC (UZLocalSaveStorage, the default), or later a cloud or a game server (e.g. for online matches with characters nobody can edit).
 *  Pick the class in Project Settings → Game → Saving → Storage Class
 */
UCLASS(Abstract)
class UZSaveStorage : public UObject
{
	GENERATED_BODY()

public:

	/** Called once before anything else. Profile keeps separate sets of saves apart, as if on different PCs (empty: the normal saves) */
	virtual void Initialize(const FString& InProfile) { Profile = InProfile; }

	/** Names of everything stored */
	virtual TArray<FString> GetSaveNames() const PURE_VIRTUAL(UZSaveStorage::GetSaveNames, return {};);

	/** Reads what was stored under the name. False if there's nothing, or it's damaged */
	virtual bool Read(const FString& Name, TArray<uint8>& OutData) const PURE_VIRTUAL(UZSaveStorage::Read, return false;);

	/** Stores data under the name and waits until it's done (e.g. when quitting) */
	virtual bool Write(const FString& Name, const TArray<uint8>& Data) PURE_VIRTUAL(UZSaveStorage::Write, return false;);

	/** Stores data without holding up the game. OnDone gets whether it worked, on the game thread. This version writes right away */
	virtual void WriteAsync(const FString& Name, TArray<uint8> Data, TFunction<void(bool bSuccess)> OnDone);

	virtual bool Delete(const FString& Name) PURE_VIRTUAL(UZSaveStorage::Delete, return false;);

	/** Waits for writes still going on, e.g. before quitting */
	virtual void Flush() {}

	/** Where saves are, for the log (e.g. a folder) */
	virtual FString Describe() const { return GetClass()->GetName(); }

protected:

	FString Profile;
};

/**
 *  Saves as files on this PC, in Saved/SaveGames (with -SaveProfile=Name or a second Play-In-Editor window: in a folder of their own)
 *  Each write goes to a new file first and replaces the old one only once it's complete, and the previous save is kept as a backup,
 *  so a crash or power cut while saving never loses the save. Files have a checksum, so a damaged one is noticed and the backup used
 */
UCLASS()
class UZLocalSaveStorage : public UZSaveStorage
{
	GENERATED_BODY()

public:

	virtual void Initialize(const FString& InProfile) override;
	virtual void BeginDestroy() override;

	virtual TArray<FString> GetSaveNames() const override;
	virtual bool Read(const FString& Name, TArray<uint8>& OutData) const override;
	virtual bool Write(const FString& Name, const TArray<uint8>& Data) override;
	virtual void WriteAsync(const FString& Name, TArray<uint8> Data, TFunction<void(bool bSuccess)> OnDone) override;
	virtual bool Delete(const FString& Name) override;
	virtual void Flush() override;
	virtual FString Describe() const override { return Directory; }

private:

	FString GetFilePath(const FString& Name) const;

	/** Writes the file safely (new file, then swap); can run on any thread */
	static bool WriteFile(const FString& Path, const TArray<uint8>& Data);

	/** Reads one file and checks it; false if it's missing or damaged */
	static bool ReadFile(const FString& Path, TArray<uint8>& OutData);

	FString Directory;

	/** Background writes, one at a time */
	TUniquePtr<UE::Tasks::FPipe> WritePipe;
};
