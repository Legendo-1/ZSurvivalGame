// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression/ZProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Core/ZPrimaryAssetUtils.h"
#include "Progression/ZTraitDefinition.h"

UZProgressionSubsystem* UZProgressionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UZProgressionSubsystem>() : nullptr;
}

TArray<UZTraitDefinition*> UZProgressionSubsystem::GetAllTraits() const
{
	EnsureTraitsLoaded();

	TArray<UZTraitDefinition*> Result;
	for (UZTraitDefinition* Trait : Traits)
	{
		Result.Add(Trait);
	}

	return Result;
}

UZTraitDefinition* UZProgressionSubsystem::FindTraitByName(FName TraitName) const
{
	EnsureTraitsLoaded();

	for (UZTraitDefinition* Trait : Traits)
	{
		if (Trait->GetFName() == TraitName)
		{
			return Trait;
		}
	}

	return nullptr;
}

void UZProgressionSubsystem::EnsureTraitsLoaded() const
{
	if (bTraitsLoaded)
	{
		return;
	}

	// loading only fills the cache, so it's fine from const getters
	UZProgressionSubsystem* MutableThis = const_cast<UZProgressionSubsystem*>(this);
	MutableThis->bTraitsLoaded = true;

	TArray<UZTraitDefinition*> Loaded = ZPrimaryAssetUtils::LoadAllOfType<UZTraitDefinition>(UZTraitDefinition::GetTraitAssetType());
	Loaded.StableSort([](const UZTraitDefinition& A, const UZTraitDefinition& B)
	{
		if (A.Category != B.Category)
		{
			return A.Category.ToString() < B.Category.ToString();
		}

		if (A.SortOrder != B.SortOrder)
		{
			return A.SortOrder < B.SortOrder;
		}

		return A.GetTraitName().CompareTo(B.GetTraitName()) < 0;
	});

	for (UZTraitDefinition* Trait : Loaded)
	{
		MutableThis->Traits.Add(Trait);
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Progression: %d traits"), Traits.Num());
}
