// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "ZAICharacter.generated.h"

class AZPickup;
class UAbilitySystemComponent;
class UAnimMontage;
class UGameplayAbility;
class UZAttributeSet;
class UZLootTable;

/**
 *  Base class for every AI character: zombies, other monsters and human NPCs
 *  Has its own ability system and attributes (health); its AI (AZAIController) only runs on the server
 *  Each enemy type is a child class or Blueprint that sets its looks and numbers (see AZZombieCharacter)
 */
UCLASS(Abstract)
class AZAICharacter : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface, public IZCharacterCuesInterface
{
	GENERATED_BODY()

	/** Runs the character's abilities (attacks) and holds its attributes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Health (and stamina, for future special attacks) */
	UPROPERTY()
	TObjectPtr<UZAttributeSet> AttributeSet;

public:

	AZAICharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** The character's Team */
	virtual FGenericTeamId GetGenericTeamId() const override;

	virtual const FZCharacterCues& GetCharacterCues() const override { return CharacterCues; }

	/** Server: starts the attack ability if the attack cooldown allows it; returns true if it started */
	bool TryAttack();

	/** True after health reached 0 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsDead() const { return bIsDead; }

	float GetAggroRange() const { return AggroRange; }
	float GetAttackRange() const { return AttackRange; }
	float GetMeleeDamage() const { return MeleeDamage; }
	float GetMeleeReach() const { return MeleeReach; }
	float GetMeleeRadius() const { return MeleeRadius; }
	float GetMeleeHitDelay() const { return MeleeHitDelay; }
	UAnimMontage* GetAttackMontage() const { return AttackMontage; }

protected:

	/** Sets up the ability system; the server also sets the health and gives the abilities */
	virtual void BeginPlay() override;

	virtual void PossessedBy(AController* NewController) override;

	/** Reduces health when damaged by engine damage (e.g. UGameplayStatics::ApplyDamage) */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	/** Server: called when Health reaches 0 (DamageInstigator is who did it, or null) */
	void HandleOutOfHealth(AActor* DamageInstigator);

	/** Every machine: ragdoll and stop moving */
	void PlayDeath();

	UFUNCTION()
	void OnRep_IsDead();

	/** Called on the server when the character dies, e.g. to drop loot or count the kill. Killer is who dealt the last damage (can be empty) */
	UFUNCTION(BlueprintImplementableEvent, Category = "AI", meta = (DisplayName = "On Died"))
	void BP_OnDied(AActor* Killer);

	/** --- STATS --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Stats", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	/** Who it fights: it attacks other teams and can't be hurt by its own */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Stats")
	EZTeam Team = EZTeam::Monsters;

	/** --- AI --- */

	/** Notices players this close, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	float AggroRange = 2500.f;

	/** Attacks when the target is this close (center to center, in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	float AttackRange = 150.f;

	/** Seconds between attacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	float AttackCooldown = 1.5f;

	/** Seconds the body stays after dying */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	float DeadBodyLifeSpan = 10.f;

	/** Its hit and death cues; give each kind of enemy its own (e.g. GameplayCue.Character.Death.Zombie) for its own sounds (see AUDIO.md) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FZCharacterCues CharacterCues;

	/** --- MELEE --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee", meta = (ClampMin = "0"))
	float MeleeDamage = 10.f;

	/** How far in front the hit reaches, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee", meta = (ClampMin = "0"))
	float MeleeReach = 120.f;

	/** Radius of the hit, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee", meta = (ClampMin = "0"))
	float MeleeRadius = 50.f;

	/** Seconds from the start of the attack until it hits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee", meta = (ClampMin = "0"))
	float MeleeHitDelay = 0.4f;

	/** Attack animation (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** --- LOOT --- */

	/** What it drops when it dies (rolled on the server) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Loot")
	TObjectPtr<UZLootTable> LootTable;

	/** Character experience every player gets when it dies (co-op: everyone shares it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Rewards", meta = (ClampMin = "0"))
	int32 ExperienceReward = 10;

	/** Trait points every player gets when it dies, e.g. 1 for a boss */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Rewards", meta = (ClampMin = "0"))
	int32 TraitPointReward = 0;

	/** Pickup spawned for each drop (AZPickup if empty) */
	UPROPERTY(EditAnywhere, Category = "AI|Loot")
	TSubclassOf<AZPickup> LootPickupClass;

	/** Seconds the loot lies on the ground before it disappears; 0 = stays */
	UPROPERTY(EditAnywhere, Category = "AI|Loot", meta = (ClampMin = "0"))
	float LootLifeSpan = 120.f;

	/** --- ABILITIES --- */

	/** The ability used to attack (UZGameplayAbility_MeleeAttack by default) */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> AttackAbility;

	/** Extra abilities, e.g. a spitter's ranged attack */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Set by the server when health reaches 0; everyone then plays the death (ragdoll) */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	/** When the last attack started (server) */
	double LastAttackTime = -1000.0;
};
