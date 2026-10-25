// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GenericTeamAgentInterface.h"
#include "ZCombatLibrary.generated.h"

/**
 *  Damage and team helpers shared by players, AI and Blueprints (traps, explosions, ...)
 */
UCLASS()
class UZCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** The team of an actor (or of its controller); NoTeam if it has none */
	static FGenericTeamId GetTeam(const AActor* Actor);

	/** True if Source may damage Target: not itself and not on the same team */
	UFUNCTION(BlueprintPure, Category = "Survival|Combat")
	static bool CanDamage(const AActor* Source, const AActor* Target);

	/** True if the actor has an ability system and is dead (State.Dead) */
	UFUNCTION(BlueprintPure, Category = "Survival|Combat")
	static bool IsDead(const AActor* Actor);

	/**
	 *  Server only: deals damage from Source to Target. Returns true if a character with health took it
	 *  Targets with an ability system lose health through UZGameplayEffect_Damage and get a GameplayCue.Character.Hit;
	 *  anything else gets regular engine point damage, for Blueprints that react to it (e.g. the template's damageable boxes)
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival|Combat", meta = (DefaultToSelf = "Source"))
	static bool ApplyDamage(AActor* Source, AActor* Target, float Damage, const FHitResult& Hit);
};
