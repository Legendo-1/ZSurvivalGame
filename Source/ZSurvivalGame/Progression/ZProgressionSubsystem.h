// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ZProgressionSubsystem.generated.h"

class UZTraitDefinition;

/**
 *  Knows every trait in the project: the Asset Manager finds them (primary asset type "Trait"), and they're loaded the first time they're needed
 */
UCLASS()
class UZProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** The subsystem of the world's game instance, or null */
	static UZProgressionSubsystem* Get(const UObject* WorldContextObject);

	/** Every trait, sorted for the character screen (category, sort order, name) */
	UFUNCTION(BlueprintPure, Category = "Progression")
	TArray<UZTraitDefinition*> GetAllTraits() const;

	/** A trait by its asset name, e.g. TR_Vigor */
	UFUNCTION(BlueprintPure, Category = "Progression")
	UZTraitDefinition* FindTraitByName(FName TraitName) const;

private:

	void EnsureTraitsLoaded() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UZTraitDefinition>> Traits;

	bool bTraitsLoaded = false;
};
