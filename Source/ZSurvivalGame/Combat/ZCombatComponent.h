// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ZSurvivalGameTypes.h"
#include "ZCombatComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UZMeleeMoveset;
struct FGameplayAbilitySpec;

/**
 *  A player's input buffer and combo, on their character (souls-like commitment, see COMBAT.md)
 *  On the player's machine: attacks, dodges, shots, reloads and weapon switches go through RequestAction. One that can't start yet because the player
 *  is busy (mid-swing, mid-dodge, a shot's recovery, staggered) waits in the buffer and starts as soon as it may (in a Combo or Dodge Window, or when the
 *  action is over), unless Input Buffer Time passes first (Project Settings → Game → Combat). Only the newest press waits.
 *  On the player's machine and the server: remembers the current attack, so the next press continues the combo
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZCombatComponent();

	/** Player's machine: does the action now, or keeps it in the buffer until the player may. SwitchWeapon needs the kind of weapon slot (e.g. Equipment.Slot.Melee) */
	UFUNCTION(BlueprintCallable, Category = "Survival|Combat", meta = (AutoCreateRefTerm = "WeaponSlot"))
	void RequestAction(EZCombatAction Action, const FGameplayTag& WeaponSlot);

	/** The action waiting in the buffer, or None */
	UFUNCTION(BlueprintPure, Category = "Survival|Combat")
	EZCombatAction GetBufferedAction() const { return BufferedAction; }

	/** Forgets the action waiting in the buffer (e.g. when the player dies) */
	void ClearBuffer();

	/** True if the action would have to wait now: the player is busy with another one (outside its window for this action) or staggered */
	bool IsActionBlocked(EZCombatAction Action) const;

	/** --- COMBO --- */

	/** Which attack of this kind comes next (from 0): the follow-up of the current attack while it's in its Combo Window, otherwise the first */
	int32 GetNextAttackStep(const UZMeleeMoveset* Moveset, EZMeleeAttackType Type) const;

	/** Server: may this attack follow the attack before? The player's machine picks the attack; a first attack always may */
	bool IsValidFollowUp(const UZMeleeMoveset* Moveset, EZMeleeAttackType Type, int32 Step) const;

	/** An attack started (the attack ability, on the player's machine and the server) */
	void NotifyAttackStarted(const UGameplayAbility* Attack, EZMeleeAttackType Type, int32 Step);

	/** An attack ended (cut short or not) */
	void NotifyAttackEnded(const UGameplayAbility* Attack);

	/** True while an attack runs */
	bool IsAttacking() const { return CurrentAttack.IsValid(); }

	/** What it's doing, for the ZCombat cheat */
	FString GetDebugString() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	/** Starts the action now (it may still fail, e.g. without stamina or with nothing to reload) */
	void DoAction(EZCombatAction Action, const FGameplayTag& WeaponSlot);

	UAbilitySystemComponent* GetAbilitySystem() const;

	/** The ability of an action: dodge, fire and reload by their input, the attacks by their tag. Null for a weapon switch (not an ability) */
	const FGameplayAbilitySpec* FindActionSpec(EZCombatAction Action) const;

	/** The moveset of the melee weapon in the hands, or null */
	const UZMeleeMoveset* GetMoveset() const;

	double GetTime() const;

	/** The press waiting for its turn */
	EZCombatAction BufferedAction = EZCombatAction::None;
	FGameplayTag BufferedWeaponSlot;
	double BufferedTime = 0.0;

	/** The attack running now, and the last one that ran (its combo continues) */
	TWeakObjectPtr<const UGameplayAbility> CurrentAttack;
	EZMeleeAttackType LastAttackType = EZMeleeAttackType::Light;
	int32 LastAttackStep = INDEX_NONE;
	double LastAttackEndTime = -1000.0;
};
