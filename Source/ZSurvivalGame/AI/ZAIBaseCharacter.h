// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "GenericTeamAgentInterface.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AI/ZAITypes.h"
#include "ZAIBaseCharacter.generated.h"

class AZPickup;
class UAbilitySystemComponent;
class UAnimMontage;
class UGameplayAbility;
class UZAttributeSet;
class UZCharacterSpacingComponent;
class UZFootstepComponent;
class UZHitFlashComponent;
class UZLockOnTargetComponent;
class UZLootTable;

DECLARE_MULTICAST_DELEGATE_TwoParams(FZOnAICombatStateChanged, EZAICombatState /*NewState*/, EZAICombatState /*OldState*/);

/**
 *  Base class for every AI character: zombies, other monsters and human NPCs (see AI.md)
 *  Has its own ability system and attributes (health); its AI (AZAIBaseController, with the combat brain) only runs on the server
 *  The AI's combat state and target are replicated here, so animations and effects on every machine can use them
 *  Each enemy type is a child class or Blueprint that sets its looks, numbers and Combat Settings (see AZZombieCharacter)
 */
UCLASS(Abstract)
class AZAIBaseCharacter : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface, public IGameplayCueInterface, public IZCharacterCuesInterface, public IZCombatantInterface
{
	GENERATED_BODY()

	/** Runs the character's abilities (attacks) and holds its attributes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** Health (and stamina, for future special attacks) */
	UPROPERTY()
	TObjectPtr<UZAttributeSet> AttributeSet;

	/** Footstep sounds, matching the surface (its Footstep Data, or the project's default) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZFootstepComponent> FootstepComponent;

	/** Where players lock on to it (its chest); move it in the Blueprint for other body shapes, or add more for big enemies */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZLockOnTargetComponent> LockOnTarget;

	/** Makes it flash when it's hit (with its Hit Cue, on every machine) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZHitFlashComponent> HitFlashComponent;

	/** Keeps players from standing too close (an invisible, wider capsule only players bump into); its Extra Radius is set per enemy type */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZCharacterSpacingComponent> SpacingComponent;

public:

	AZAIBaseCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** The character's Team */
	virtual FGenericTeamId GetGenericTeamId() const override;

	virtual const FZCharacterCues& GetCharacterCues() const override { return CharacterCues; }

	/** Its Stagger Montage, for the stagger ability */
	virtual UAnimMontage* GetStaggerMontage() const override { return StaggerMontage; }

	using IGameplayCueInterface::HandleGameplayCue;

	/** Every machine: a cue arrived for this character. Sounds and effects are GameplayCue Notify assets; this plays the hit flash with its Hit Cue */
	virtual void HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	/** Server: starts the attack ability if the attack cooldown allows it; returns true if it started */
	bool TryAttack();

	/** True if its attack cooldown is over (and it's alive) */
	bool IsAttackReady() const;

	/** True while its Attack Montage plays (the swing can go on after the attack ability ended at its hit) */
	bool IsPlayingAttackAnimation() const;

	/** --- COMBAT STATE (set by the AI on the server, replicated to everyone) --- */

	/** What it's doing in the fight: Idle, Approaching, Strafing, Attacking, DodgeRecovering or Staggered (e.g. for its animations) */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	EZAICombatState GetCombatState() const { return CombatState; }

	/** Whom it fights, or null (e.g. for its head to follow the target) */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	AActor* GetCombatTarget() const { return CombatTarget; }

	/** Server (the AI brain): its new combat state, for everyone */
	void SetCombatState(EZAICombatState NewState);

	/** Server (the AI brain): its new target, for everyone */
	void SetCombatTarget(AActor* NewTarget);

	/** Every machine: the combat state changed */
	FZOnAICombatStateChanged OnCombatStateChanged;

	/** How it senses and fights (its AI reads them) */
	const FZAICombatSettings& GetCombatSettings() const { return CombatSettings; }

	/** Server: changes how it senses and fights during play, e.g. a boss that starts circling in its second phase (its AI picks them up right away) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AI|Combat")
	void SetCombatSettings(const FZAICombatSettings& NewSettings);

	/** Its movement component's Max Walk Speed as set up (the AI chases at it; strafing slows it down) */
	float GetDefaultWalkSpeed() const { return DefaultWalkSpeed; }

	/** True after health reached 0 */
	UFUNCTION(BlueprintPure, Category = "AI")
	bool IsDead() const { return bIsDead; }

	float GetAttackRange() const { return AttackRange; }
	float GetMeleeDamage() const { return MeleeDamage; }
	float GetMeleePoiseDamage() const { return MeleePoiseDamage; }
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

	/** Server: called when Poise reaches 0: it staggers */
	void HandlePoiseBroken(AActor* StaggerInstigator);

	/** Every machine: ragdoll and stop moving */
	void PlayDeath();

	UFUNCTION()
	void OnRep_IsDead();

	/** Called on the server when the character dies, e.g. to drop loot or count the kill. Killer is who dealt the last damage (can be empty) */
	UFUNCTION(BlueprintImplementableEvent, Category = "AI", meta = (DisplayName = "On Died"))
	void BP_OnDied(AActor* Killer);

	/** Every machine: its combat state changed, e.g. to play a roar when it starts approaching */
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|Combat", meta = (DisplayName = "On Combat State Changed"))
	void BP_OnCombatStateChanged(EZAICombatState NewState, EZAICombatState OldState);

	UFUNCTION()
	void OnRep_CombatState(EZAICombatState OldState);

	/** The same on every machine when the combat state changed (the server calls it itself) */
	void HandleCombatStateChanged(EZAICombatState OldState);

	/** --- STATS --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Stats", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	/** How much poise damage it takes before it staggers (it refills after a few seconds without hits, see Project Settings → Game → Combat). 0 = every hit with poise damage staggers it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Stats", meta = (ClampMin = "0"))
	float MaxPoise = 30.f;

	/** Who it fights: it attacks other teams and can't be hurt by its own */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Stats")
	EZTeam Team = EZTeam::Monsters;

	/** --- AI --- */

	/** How it senses enemies (sight, proximity, memory) and fights them (strafing, recovery); its AI reads these */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	FZAICombatSettings CombatSettings;

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

	/** How much each of its hits wears down the target's poise toward a stagger */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee", meta = (ClampMin = "0"))
	float MeleePoiseDamage = 15.f;

	/** Attack animation (optional) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Melee")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** --- STAGGER --- */

	/** Animation when its poise breaks (optional: without one it stands still for the Default Stagger Duration). It may move it (root motion) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Stagger")
	TObjectPtr<UAnimMontage> StaggerMontage;

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

	/** The ability that staggers it when its poise breaks (UZGameplayAbility_Stagger by default); empty = it never staggers, e.g. for a boss */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> StaggerAbility;

	/** Extra abilities, e.g. a spitter's ranged attack */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Set by the server when health reaches 0; everyone then plays the death (ragdoll) */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	/** When the last attack started (server) */
	double LastAttackTime = -1000.0;

	/** Its combat state, set by its AI on the server */
	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	EZAICombatState CombatState = EZAICombatState::Idle;

	/** Whom it fights, set by its AI on the server */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> CombatTarget;

	/** The movement component's Max Walk Speed at BeginPlay */
	float DefaultWalkSpeed = 0.f;
};
