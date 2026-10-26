// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression/ZProgressionSettings.h"

UZProgressionSettings::UZProgressionSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Progression");
}

int32 UZProgressionSettings::GetCharacterExperienceForLevel(int32 Level) const
{
	if (Level <= 1)
	{
		return 0;
	}

	const double Experience = CharacterExperienceBase * FMath::Pow(static_cast<double>(Level - 1), static_cast<double>(CharacterExperienceExponent));
	return static_cast<int32>(FMath::Min(FMath::RoundToDouble(Experience), static_cast<double>(MAX_int32)));
}

int32 UZProgressionSettings::GetCharacterLevelForExperience(int32 Experience) const
{
	// invert the curve, then step to the exact level (rounding)
	int32 Level = 1 + FMath::FloorToInt32(FMath::Pow(FMath::Max(Experience, 0) / FMath::Max(static_cast<double>(CharacterExperienceBase), 1.0), 1.0 / FMath::Max(static_cast<double>(CharacterExperienceExponent), 1.0)));
	Level = FMath::Clamp(Level, 1, MaxCharacterLevel);

	while (Level < MaxCharacterLevel && GetCharacterExperienceForLevel(Level + 1) <= Experience)
	{
		++Level;
	}

	while (Level > 1 && GetCharacterExperienceForLevel(Level) > Experience)
	{
		--Level;
	}

	return Level;
}

int32 UZProgressionSettings::GetTradeskillExperienceForLevel(int32 Level) const
{
	if (Level <= 0)
	{
		return 0;
	}

	const double Experience = TradeskillExperienceBase * FMath::Pow(static_cast<double>(Level), static_cast<double>(TradeskillExperienceExponent));
	return static_cast<int32>(FMath::Min(FMath::RoundToDouble(Experience), static_cast<double>(MAX_int32)));
}

int32 UZProgressionSettings::GetTradeskillLevelForExperience(int32 Experience) const
{
	if (Experience <= 0)
	{
		return 0;
	}

	// invert the curve, then step to the exact level (rounding)
	int32 Level = FMath::FloorToInt32(FMath::Pow(Experience / FMath::Max(static_cast<double>(TradeskillExperienceBase), 1.0), 1.0 / FMath::Max(static_cast<double>(TradeskillExperienceExponent), 1.0)));
	Level = FMath::Clamp(Level, 0, MaxTradeskillLevel);

	while (Level < MaxTradeskillLevel && GetTradeskillExperienceForLevel(Level + 1) <= Experience)
	{
		++Level;
	}

	while (Level > 0 && GetTradeskillExperienceForLevel(Level) > Experience)
	{
		--Level;
	}

	return Level;
}

float UZProgressionSettings::GetTradeskillExperienceScale(int32 PlayerLevel, int32 RequiredLevel) const
{
	const int32 LevelsAbove = PlayerLevel - RequiredLevel;
	if (LevelsAbove <= 0)
	{
		return 1.f;
	}

	return FMath::Clamp(1.f - static_cast<float>(LevelsAbove) / FMath::Max(TradeskillExperienceFalloffLevels, 1), 0.f, 1.f);
}
