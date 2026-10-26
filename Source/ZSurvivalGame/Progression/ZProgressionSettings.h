// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ZProgressionSettings.generated.h"

/**
 *  The numbers behind progression: character levels, trait points and how tradeskills level up
 *  Project Settings → Game → Progression (saved in Config/DefaultGame.ini)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Progression"))
class UZProgressionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UZProgressionSettings();

	static const UZProgressionSettings* Get() { return GetDefault<UZProgressionSettings>(); }

	/** --- CHARACTER LEVEL --- */

	/** Highest character level */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character Level", meta = (ClampMin = "1"))
	int32 MaxCharacterLevel = 50;

	/** Total experience to reach a level = Base × (Level - 1) ^ Exponent. With 100 and 1.5: level 2 takes 100, level 10 takes 2700, level 50 takes 34300 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character Level", meta = (ClampMin = "1"))
	float CharacterExperienceBase = 100.f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Character Level", meta = (ClampMin = "1"))
	float CharacterExperienceExponent = 1.5f;

	/** --- TRAITS --- */

	/** Trait points for each level gained (bosses and Tomes of Knowledge give more) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Traits", meta = (ClampMin = "0"))
	int32 TraitPointsPerLevel = 1;

	/** Most trait points a player can earn in total, from levels, bosses and tomes; 0 = no limit */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Traits", meta = (ClampMin = "0"))
	int32 MaxTraitPoints = 0;

	/** Players can take back all their trait points and spend them again (so trying another build never ruins a character) */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Traits")
	bool bAllowTraitReset = true;

	/** Total experience needed to reach a character level (level 1 needs none) */
	int32 GetCharacterExperienceForLevel(int32 Level) const;

	/** The character level for this much total experience */
	int32 GetCharacterLevelForExperience(int32 Experience) const;

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
