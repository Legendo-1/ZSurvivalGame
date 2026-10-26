// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZGameplayEffect_Traits.generated.h"

/**
 *  All of a player's trait bonuses as one effect that stays on: it adds to every attribute traits can change
 *  (UZAttributeSet::GetTraitAttributes), each by an amount set when it's applied (set by caller, named after the attribute)
 *  The progression component applies it again whenever the traits change
 */
UCLASS()
class UZGameplayEffect_Traits : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UZGameplayEffect_Traits();

	/** The set-by-caller name for the bonus to an attribute */
	static FName GetMagnitudeName(const FGameplayAttribute& Attribute);
};
