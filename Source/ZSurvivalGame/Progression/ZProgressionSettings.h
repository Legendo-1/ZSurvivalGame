// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ZProgressionSettings.generated.h"

/**
 *  The numbers behind progression: how tradeskills level up
 *  Project Settings → Game → Progression (saved in Config/DefaultGame.ini)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Progression"))
class UZProgressionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UZProgressionSettings();

	static const UZProgressionSettings* Get() { return GetDefault<UZProgressionSettings>(); }

	/** --- TRADESKILLS --- */

	/** Highest level of a tradeskill */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tradeskills", meta = (ClampMin = "1"))
	int32 MaxTradeskillLevel = 100;

	/** Total experience for a tradeskill level = Base × Level ^ Exponent. With 10 and 1.5: level 10 takes 316, level 25 takes 1250, level 100 takes 10000 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tradeskills", meta = (ClampMin = "1"))
	float TradeskillExperienceBase = 10.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tradeskills", meta = (ClampMin = "1"))
	float TradeskillExperienceExponent = 1.5f;

	/** How many levels above a recipe's needed level the recipe stops teaching anything (its experience fades out over these levels), so players have to move on to harder recipes */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Tradeskills", meta = (ClampMin = "1"))
	int32 TradeskillExperienceFalloffLevels = 25;

	/** Total experience needed to reach a tradeskill level */
	int32 GetTradeskillExperienceForLevel(int32 Level) const;

	/** The tradeskill level for this much total experience */
	int32 GetTradeskillLevelForExperience(int32 Experience) const;

	/** How much of a recipe's experience a player at this level still gets: 1 up to the needed level, then less, down to 0 */
	float GetTradeskillExperienceScale(int32 PlayerLevel, int32 RequiredLevel) const;
};
