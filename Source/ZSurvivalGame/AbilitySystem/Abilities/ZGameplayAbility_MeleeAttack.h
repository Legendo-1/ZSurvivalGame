// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "ZGameplayAbility_MeleeAttack.generated.h"

class AZAIBaseCharacter;
class UZMeleeTraceComponent;
struct FZEnemyAttack;

/**
 *  Melee attack for AI characters (server only): plays the attack the character picked (AZAIBaseCharacter::TryAttack, from its Attacks)
 *  Where it hits: the Weapon Hitbox notifies in the attack's montage sweep the character's weapon along the swing (UZMeleeTraceComponent),
 *  like a player's attack, each target once per hitbox; a montage without them hits everyone hostile in front of it at the attack's Hit Delay
 *  Damage and poise damage come from the attack. It lasts as long as the montage; a stagger cuts it short
 *  (Players' melee attacks are UZGameplayAbility_LightAttack and UZGameplayAbility_HeavyAttack)
 */
UCLASS()
class UZGameplayAbility_MeleeAttack : public UZGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility_MeleeAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:

	/** A montage without hitboxes: the attack reaches its hit time */
	UFUNCTION()
	void OnHitTime();

	/** The weapon's hitbox touched someone */
	void OnWeaponHit(const FHitResult& Hit);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();

private:

	/** The attack it makes (from the character's Attacks), or null */
	const FZEnemyAttack* GetAttack() const;

	/** Damages one target with this attack (server) */
	void HitTarget(AZAIBaseCharacter& Attacker, AActor* Target, const FHitResult& Hit);

	/** Which of the character's Attacks this activation makes */
	int32 AttackIndex = INDEX_NONE;

	/** Its hits come from the montage's hitboxes (not a timed hit) */
	bool bUsesHitboxes = false;

	/** The hitboxes it listens to, while the attack runs */
	TWeakObjectPtr<UZMeleeTraceComponent> HitTracer;
	FDelegateHandle MeleeHitDelegateHandle;
};
