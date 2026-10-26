// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ZFootstepSettings.generated.h"

class UZFootstepData;

/**
 *  Project-wide footstep settings
 *  Project Settings → Game → Footsteps (saved in Config/DefaultGame.ini)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Footsteps"))
class UZFootstepSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	static const UZFootstepSettings* Get() { return GetDefault<UZFootstepSettings>(); }

	/** Footstep sounds for characters whose Footsteps component has no Footstep Data of their own, and for hearing Footstep notifies in the animation editor */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TSoftObjectPtr<UZFootstepData> DefaultFootstepData;
};
