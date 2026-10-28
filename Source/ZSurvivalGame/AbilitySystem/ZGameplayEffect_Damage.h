// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ZGameplayEffect_Damage.generated.h"

/**
 *  Instant damage: adds the SetByCaller.Damage value to the target's IncomingDamage, which UZAttributeSet turns into lost health,
 *  and SetByCaller.PoiseDamage to its IncomingPoiseDamage, which wears its poise down toward a stagger
 *  Applied by UZCombatLibrary::ApplyDamage. Make a Blueprint child of it to add tags (e.g. damage types) or gameplay cues
 */
UCLASS()
class UZGameplayEffect_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:

	UZGameplayEffect_Damage();
};
