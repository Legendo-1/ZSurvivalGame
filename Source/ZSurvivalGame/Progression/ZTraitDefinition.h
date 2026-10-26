// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ZTraitDefinition.generated.h"

class UTexture2D;

/** One stat a trait raises, by the same amount for each rank */
USTRUCT(BlueprintType)
struct FZTraitModifier
{
	GENERATED_BODY()

	/** The stat, e.g. MaxHealth or DamageMultiplier (the ones traits can change: see UZAttributeSet::GetTraitAttributes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayAttribute Attribute;

	/** Added per rank, e.g. 10 for health, or 0.03 for +3% on a multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	float ValuePerRank = 0.f;
};

/**
 *  A trait players spend trait points on, e.g. Vigor (more health) or Steady Hands (less recoil). Each rank adds its modifiers again
 *  Traits change stats, never lock a play style: points can be reset and spent again
 *  One Data Asset per trait in Content/Progression/Traits: right-click → Miscellaneous → Data Asset → ZTraitDefinition
 *  The game finds every trait by itself (Asset Manager)
 */
UCLASS(BlueprintType)
class UZTraitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Primary asset type of every trait */
	static FPrimaryAssetType GetTraitAssetType() { return FPrimaryAssetType(TEXT("Trait")); }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Groups traits on the character screen, e.g. Trait.Category.Combat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait", meta = (Categories = "Trait.Category"))
	FGameplayTag Category;

	/** Order within its category: lower first, same number by name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait")
	int32 SortOrder = 0;

	/** Highest rank */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait", meta = (ClampMin = "1"))
	int32 MaxRank = 10;

	/** Trait points one rank costs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait", meta = (ClampMin = "1"))
	int32 PointsPerRank = 1;

	/** What each rank adds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trait")
	TArray<FZTraitModifier> Modifiers;

	/** Name for the UI: Display Name, or the asset name */
	UFUNCTION(BlueprintPure, Category = "Trait")
	FText GetTraitName() const;

	/** What the trait gives at this rank, one line per stat, e.g. "Max Health +30" and "Damage +9%" */
	UFUNCTION(BlueprintPure, Category = "Trait")
	FText GetEffectText(int32 Rank) const;

	/** A bonus as the UI shows it: multipliers and chances in percent ("Damage +9%"), the rest as numbers ("Max Health +30") */
	static FText DescribeBonus(const FGameplayAttribute& Attribute, float Value);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
