// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZGameplayEffect_Heal.generated.h"

/**
 *  Instant healing: adds the SetByCaller.Healing value to the target's IncomingHealing, which UZAttributeSet turns into health
 *  Applied by UZCombatLibrary::ApplyHealing
 */
UCLASS()
class UZGameplayEffect_Heal : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UZGameplayEffect_Heal();
};
