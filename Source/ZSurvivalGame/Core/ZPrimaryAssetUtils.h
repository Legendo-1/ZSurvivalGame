// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "ZSurvivalGame.h"

/** Helpers for data assets the Asset Manager finds by their primary asset type (items, recipes, traits, ...) */
namespace ZPrimaryAssetUtils
{
	/** Loads every primary asset of this type (blocking; for small data assets), sorted by asset name */
	template <typename AssetType>
	TArray<AssetType*> LoadAllOfType(const FPrimaryAssetType& Type)
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FPrimaryAssetId> AssetIds;
		AssetManager.GetPrimaryAssetIdList(Type, AssetIds);

		TArray<AssetType*> Loaded;
		for (const FPrimaryAssetId& AssetId : AssetIds)
		{
			if (AssetType* Asset = Cast<AssetType>(AssetManager.GetPrimaryAssetPath(AssetId).TryLoad()))
			{
				Loaded.Add(Asset);
			}
			else
			{
				UE_LOG(LogZSurvivalGame, Warning, TEXT("Couldn't load '%s'"), *AssetId.ToString());
			}
		}

		Loaded.Sort([](const AssetType& A, const AssetType& B) { return A.GetName() < B.GetName(); });
		return Loaded;
	}
}
