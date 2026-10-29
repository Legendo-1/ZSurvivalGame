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
class UZMeleeTraceComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FZOnAICombatStateChanged, EZAICombatState /*NewState*/, EZAICombatState /*OldState*/);

/** A mesh an enemy carries on its body, e.g. a weapon or a shield: it follows the socket and nothing collides with it */
USTRUCT(BlueprintType)
struct FZAttachedMesh
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	TObjectPtr<UStaticMesh> Mesh;

	/** The socket (or bone) of its body mesh it follows, e.g. weapon_rSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	FName Socket;

	/** Moved, turned and scaled from the socket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looks")
	FTransform Offset;

	/** Its attacks hit with this mesh: the Weapon Hitbox notifies in its attack montages sweep it along the swing.
	 *  The part that hits: between the mesh's sockets BladeStart and BladeEnd if it has them, otherwise the whole mesh along its longest side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bIsWeapon = false;

	/** How thick the part that hits is, in cm of the mesh; 0 = as thick as the mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0", Units = "cm", EditCondition = "bIsWeapon"))
	float BladeRadius = 0.f;
};

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

	/** Finds what its attacks hit, along its weapon, while a Weapon Hitbox notify in the attack's montage is active (on the server) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZMeleeTraceComponent> MeleeTraceComponent;

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

	/** Its weapon for the hitboxes of its attacks: the first of its Attached Meshes marked Is Weapon */
	virtual bool GetMeleeBlade(FZMeleeBlade& OutBlade) const override;

	/** Server: picks one of its attacks that fits a target this far away (middle to middle, along the ground) and starts it; true if one started */
	bool TryAttack(float DistanceToTarget);

	/** True if it may attack now: alive, the time between attacks is over, and one of its attacks is ready */
	bool IsAttackReady() const;

	/** Server, at each decision of its AI: keeps the attack it goes for next, or picks another (by weight) when that one can't be used any more */
	void UpdateAttackPlan(float DistanceToTarget);

	/** How close its AI wants to get to a target this far away: into the range of the attack it plans (without one, ZAICombatRules::GetApproachRange) */
	float GetApproachRange(float DistanceToTarget) const;

	/** One of its Attacks, or null */
	const FZEnemyAttack* GetAttack(int32 Index) const { return Attacks.IsValidIndex(Index) ? &Attacks[Index] : nullptr; }

	/** The attack it's making or made last (server), or null */
	const FZEnemyAttack* GetCurrentAttack() const { return GetAttack(CurrentAttackIndex); }

	/** Which of its Attacks that is (server), or INDEX_NONE */
	int32 GetCurrentAttackIndex() const { return CurrentAttackIndex; }

	/** True while the montage of its current attack plays (server) */
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

protected:

	/** Sets up the ability system and its Attached Meshes; the server also sets the health and gives the abilities */
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

	/** Every machine: makes a component for each of its Attached Meshes */
	void CreateAttachedMeshes();

	/** --- LOOKS --- */

	/** Meshes it carries on its body, e.g. a hammer on weapon_rSocket and a shield on weapon_lSocket; every machine makes them itself
	 *  when the game starts (they don't show in the Blueprint's viewport), and nothing collides with them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Looks")
	TArray<FZAttachedMesh> AttachedMeshes;

	/** The components made for its Attached Meshes */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> AttachedMeshComponents;

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

	/** At least this long from the start of one attack to the start of the next, in seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0", Units = "s"))
	float AttackCooldown = 1.5f;

	/** Seconds the body stays after dying */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0"))
	float DeadBodyLifeSpan = 10.f;

	/** Its hit and death cues; give each kind of enemy its own (e.g. GameplayCue.Character.Death.Zombie) for its own sounds (see AUDIO.md) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FZCharacterCues CharacterCues;

	/** --- ATTACKS --- */

	/** Its attacks: it picks one that fits how far away its target is (each with its range, damage, cooldown and weight; see AI.md → Attacks) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Attacks")
	TArray<FZEnemyAttack> Attacks;

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

	/** When each of its Attacks may be used again (server) */
	TArray<double> AttackReadyTimes;

	/** The attack it's making or made last (server) */
	int32 CurrentAttackIndex = INDEX_NONE;

	/** The attack it goes for next: its AI walks into that one's range (server) */
	int32 PlannedAttackIndex = INDEX_NONE;

	/** Its attacks as the AI's rules see them (ready or not, at the current time) */
	TArray<FZAttackOption> MakeAttackOptions() const;

	/** Its combat state, set by its AI on the server */
	UPROPERTY(ReplicatedUsing = OnRep_CombatState)
	EZAICombatState CombatState = EZAICombatState::Idle;

	/** Whom it fights, set by its AI on the server */
	UPROPERTY(Replicated)
	TObjectPtr<AActor> CombatTarget;

	/** The movement component's Max Walk Speed at BeginPlay */
	float DefaultWalkSpeed = 0.f;
};
