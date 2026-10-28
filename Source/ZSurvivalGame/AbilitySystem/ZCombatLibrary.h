// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "UObject/Interface.h"
#include "ZCombatLibrary.generated.h"

class AZWeapon;
class UAbilitySystemComponent;
class UAnimMontage;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UZCombatantInterface : public UInterface
{
	GENERATED_BODY()
};

/** Something that fights: players and AI characters. Tells the shared combat abilities (e.g. the stagger) how this character looks doing them */
class IZCombatantInterface
{
	GENERATED_BODY()

public:

	/** The animation for staggering when poise breaks (null: it stands still for the Default Stagger Duration) */
	virtual UAnimMontage* GetStaggerMontage() const { return nullptr; }

	/** The weapon in its hands (its melee hitboxes follow its blade), or null */
	virtual AZWeapon* GetHeldWeapon() const { return nullptr; }
};

/**
 *  Damage, team and commitment helpers shared by players, AI and Blueprints (traps, explosions, ...)
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
	 *  Targets with an ability system lose health (less with armor) and poise through UZGameplayEffect_Damage and get a GameplayCue.Character.Hit;
	 *  anything else gets regular engine point damage, for Blueprints that react to it (e.g. the template's damageable boxes)
	 *  Invulnerable targets (State.Invulnerable, e.g. in a dodge) take nothing
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival|Combat", meta = (DefaultToSelf = "Source"))
	static bool ApplyDamage(AActor* Source, AActor* Target, float Damage, const FHitResult& Hit, float PoiseDamage = 0.f);

	/** Server only: heals Target (through UZGameplayEffect_Heal). Returns true if it had health to heal; the dead aren't healed */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival|Combat", meta = (DefaultToSelf = "Source"))
	static bool ApplyHealing(AActor* Source, AActor* Target, float Amount);

	/** --- COMMITMENT (see COMBAT.md) --- */

	/** True while the actor is committed to an action (State.Busy: an attack, a shot, a dodge, a stagger) */
	UFUNCTION(BlueprintPure, Category = "Survival|Combat")
	static bool IsBusy(const AActor* Actor);

	/** True while the actor is staggered */
	UFUNCTION(BlueprintPure, Category = "Survival|Combat")
	static bool IsStaggered(const AActor* Actor);

	/** Server only: staggers a character (its stagger ability, started by Event.Stagger), as when its poise breaks. Returns true if it staggered */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival|Combat")
	static bool Stagger(AActor* Target, AActor* Instigator);

	/**
	 *  True if the montage the ability system plays right now (e.g. an attack) is inside a window of this kind (e.g. Window.Combo),
	 *  marked in the montage by a window Anim Notify State (Combo Window, Dodge Window)
	 */
	static bool IsActionWindowOpen(const UAbilitySystemComponent* AbilitySystem, const FGameplayTag& Window);
};
