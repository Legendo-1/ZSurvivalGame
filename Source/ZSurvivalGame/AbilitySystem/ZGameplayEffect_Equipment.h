// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayEffect_Traits.h"
#include "ZGameplayEffect_Equipment.generated.h"

/**
 *  All the stats a player's equipped items change, as one effect that stays on (e.g. Armor from a jacket, MaxHealth from a necklace)
 *  The same modifiers as the traits' effect (every stat attribute, each set by caller), but its own effect, so equipment and traits change separately
 *  The equipment component applies it again whenever what's equipped changes
 */
UCLASS()
class UZGameplayEffect_Equipment : public UZGameplayEffect_Traits
{
	GENERATED_BODY()
};
