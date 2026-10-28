// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_ComboAttack.generated.h"

class AZWeapon;
class UZCombatComponent;
class UZMeleeTraceComponent;
struct FZMeleeAttack;

/**
 *  One attack of a melee weapon's combo (light or heavy, from the weapon's moveset): plays its animation, and what its Weapon Hitbox notifies touch takes damage
 *  Started by the player's combat component with a gameplay event (Event.Attack.Light / Heavy) whose Event Magnitude is the attack's number in the combo (from 0)
 *  Each attack is its own predicted activation: started in the Combo Window of the attack before, it cuts that one short (see COMBAT.md)
 *  The player's machine finds the hits and sends them to the server, which checks them (reach, hits per target) and decides the damage and poise damage
 *  No walking during the swing (the animation's root motion still moves the character); costs stamina
 */
UCLASS(Abstract)
class UZGameplayAbility_ComboAttack : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_ComboAttack();

	/** Light or heavy: which combo of the moveset it plays */
	EZMeleeAttackType GetAttackType() const { return AttackType; }

	/** Needs a melee weapon with attacks of this kind, and (on the player's machine) the character on the ground */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** The attack's number must exist; the server also checks it may follow the attack before */
	virtual bool ShouldAbilityRespondToEvent(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayEventData* Payload) const override;

	/** Any stamina left? (souls-like: an attack may take the last of it) */
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Pays the attack's stamina, on the server */
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** Called by the light and heavy attacks' constructors: which combo it plays, and the event that starts it */
	void InitAttackType(EZMeleeAttackType InAttackType);

	/** The attack of this kind with this number in the moveset of the weapon in the hands, or null */
	const FZMeleeAttack* FindAttack(const FGameplayAbilityActorInfo* ActorInfo, int32 InStep) const;

	/** The melee weapon in the character's hands, or null */
	static AZWeapon* GetMeleeWeapon(const FGameplayAbilityActorInfo* ActorInfo);

	/** The character's combat component (it remembers the combo), or null */
	static UZCombatComponent* GetCombatComponent(const FGameplayAbilityActorInfo* ActorInfo);

	/** The hitboxes touched someone: the player's machine sends it to the server; the server (AI, the host's own attacks) uses it right away */
	void OnMeleeHit(const FHitResult& Hit);

	/** Server: hits found on the player's machine arrived */
	void OnServerTargetData(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);

	/** Server: checks a hit (reach, hits per target) and deals its damage and poise damage */
	void ApplyHit(const FHitResult& Hit);

	/** The weapon's Impact Cue at the hit (Raw Magnitude = the damage): predicted on the player's machine, sent to everyone by the server */
	void ExecuteImpactCue(const FHitResult& Hit, float Damage) const;

	/** The animation is over (blending out): the attack ends */
	UFUNCTION()
	void OnMontageFinished();

	/** The animation was stopped by another one or couldn't play: the attack is cut short */
	UFUNCTION()
	void OnMontageInterrupted();

	/** Light or heavy */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	EZMeleeAttackType AttackType = EZMeleeAttackType::Light;

private:

	/** This activation's attack: its number in the combo (from 0) */
	int32 Step = 0;

	/** Server: how often each target took a hit in this attack, and the most it may (one per Weapon Hitbox notify in the animation) */
	TMap<TWeakObjectPtr<AActor>, int32> HitCounts;
	int32 MaxHitsPerTarget = 1;

	TWeakObjectPtr<UZMeleeTraceComponent> HitTracer;
	FDelegateHandle MeleeHitDelegateHandle;
	FDelegateHandle TargetDataDelegateHandle;
};

/** The light combo (left mouse button with a melee weapon) */
UCLASS()
class UZGameplayAbility_LightAttack : public UZGameplayAbility_ComboAttack
{
	GENERATED_BODY()

public:

	UZGameplayAbility_LightAttack();
};

/** The heavy combo (right mouse button with a melee weapon) */
UCLASS()
class UZGameplayAbility_HeavyAttack : public UZGameplayAbility_ComboAttack
{
	GENERATED_BODY()

public:

	UZGameplayAbility_HeavyAttack();
};
