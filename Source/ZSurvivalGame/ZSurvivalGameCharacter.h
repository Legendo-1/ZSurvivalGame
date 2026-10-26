// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayCueInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Logging/LogMacros.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "Audio/ZFootstepComponent.h"
#include "Items/ZItemDefinition.h"
#include "ZSurvivalGameCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UAbilitySystemComponent;
class UZGameplayAbility;
class UZCharacterMovementComponent;
class UZInteractionComponent;
class UZInventoryComponent;
class UZItemInstance;
class AZWeapon;
struct FInputActionValue;
struct FGameplayAttribute;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 *  Sprint, aim, dodge, fire and reload are abilities (see AbilitySystem/); health and stamina live on the PlayerState
 */
UCLASS(abstract)
class AZSurvivalGameCharacter : public ACharacter, public IAbilitySystemInterface, public IGenericTeamAgentInterface, public IGameplayCueInterface, public IZCharacterCuesInterface, public IZGaitSpeedsInterface
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Finds and uses interactables (pickups, later doors, crafting stations, ...) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZInteractionComponent> InteractionComponent;

	/** Footstep sounds, matching the surface and the gait (walk, jog, sprint) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZFootstepComponent> FootstepComponent;

protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Sprint Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SprintAction;

	/** Dodge Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* DodgeAction;

	/** Aim Input Action (hold) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* AimAction;

	/** Fire Input Action (hold for automatic weapons) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* FireAction;

	/** Reload Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ReloadAction;

	/** Interact Input Action (hold for interactions that take time) */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* InteractAction;

	/** --- ABILITIES --- */

	/** Abilities the player gets when this character is possessed */
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UZGameplayAbility>> DefaultAbilities;

	/** --- SURVIVAL STATS & MOVEMENT --- */

	// Health and stamina are attributes on the PlayerState (UZAttributeSet); the dodge cost is on the Dodge ability

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float StaminaDrainRate = 15.f; // Stamina per second while sprinting

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float StaminaRegenRate = 10.f; // Stamina per second while idle/walking

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement")
	float JogSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement")
	float SprintSpeed = 550.f;

	/** Max angle (degrees) between facing and movement direction that still allows sprint speed; 90 allows sideways, blocks backwards */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement", meta = (ClampMin = "0", ClampMax = "180"))
	float MaxSprintAngle = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement")
	float AimWalkSpeed = 150.f;

	/** Speed cap while the Walk speed limit is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement")
	float WalkSpeed = 150.f;

	/** --- AIMING --- */

	/** If true, the character always turns to face the camera direction; if false, only while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Aiming")
	bool bAlwaysFaceCamera = true;

	/** Copy of the aiming state for the other players' machines (the owner and the server read it from the movement component) */
	UPROPERTY(Replicated)
	bool bIsAiming = false;

	/** Camera boom length while aiming (closer over-the-shoulder view) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Aiming")
	float AimArmLength = 150.f;

	/** How fast the camera zooms in/out when aiming starts/stops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Aiming")
	float AimZoomSpeed = 10.f;

	/** Boom length when not aiming, cached from the CameraBoom on BeginPlay */
	float DefaultArmLength = 250.f;

	/** --- INVENTORY & WEAPON --- */

	/** Items a player gets the first time they spawn (kept in their inventory on the PlayerState, also after dying) */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Inventory")
	TArray<FZItemStack> StartingItems;

	/** Current weapon animation set; follows the equipped weapon (set with SetWeaponType) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Survival|Weapon")
	EZWeaponType CurrentWeaponType = EZWeaponType::Unarmed;

	/** The weapon in the hands (spawned by the server from the equipped item), or null when unarmed */
	UPROPERTY(Replicated)
	TObjectPtr<AZWeapon> EquippedWeapon;

	/** The inventory item in the hands; only the owning player needs it */
	UPROPERTY(Replicated)
	TObjectPtr<UZItemInstance> EquippedItem;

	/** True while the fire button is held, for automatic weapons (owning player's machine) */
	bool bFireHeld = false;

	/** When the server last confirmed a hit, and whether it was a kill (owning player's machine) */
	double LastHitTime = -1000.0;
	bool bLastHitWasKill = false;

	/** --- DEATH --- */

	/** Set by the server when health reaches 0; everyone then plays the death (ragdoll) */
	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	/** --- EFFECTS --- */

	/** The player's hit and death cues (GameplayCue Notify assets with these tags play the sounds and effects, see AUDIO.md) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FZCharacterCues CharacterCues;

	/** The player's ability system, owned by the PlayerState */
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

public:

	/** Constructor */
	AZSurvivalGameCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns the PlayerState's ability system (null until it's set up) */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Players are all on the same team */
	virtual FGenericTeamId GetGenericTeamId() const override;

	virtual const FZCharacterCues& GetCharacterCues() const override { return CharacterCues; }

	using IGameplayCueInterface::HandleGameplayCue;

	/** Every machine: a cue arrived for this character. Sounds and effects are GameplayCue Notify assets; this plays the weapon's Equip Montage with its equip cue */
	virtual void HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

protected:

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/** Server: puts the weapon away with the character */
	virtual void Destroyed() override;

	/** Reduces health when damaged (e.g. via UGameplayStatics::ApplyDamage) */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	/** Server: sets up the ability system, gives the default abilities and starting items, and takes out the weapon */
	virtual void PossessedBy(AController* NewController) override;

	/** Server: lets go of the PlayerState's ability system and inventory (e.g. the dead body after a respawn) */
	virtual void UnPossessed() override;

	/** Client: sets up the ability system once the PlayerState has arrived */
	virtual void OnRep_PlayerState() override;

	/** Client: sets up the ability system again when the controller arrives, so it knows this player is local */
	virtual void NotifyControllerChanged() override;

	/** Connects the PlayerState's ability system to this character */
	void InitAbilitySystem();

	/** Server: gives the abilities in DefaultAbilities, unless the player already has them (e.g. after a respawn) */
	void GiveDefaultAbilities();

	/** Server: called when Health reaches 0 (DamageInstigator is who did it, or null) */
	void HandleOutOfHealth(AActor* DamageInstigator);

	/** Every machine: ragdoll and stop moving */
	void PlayDeath();

	UFUNCTION()
	void OnRep_IsDead();

	/** Server: drains stamina while sprinting and regenerates it otherwise */
	void UpdateStamina(float DeltaTime);

	/** Server: gives the StartingItems, once per player */
	void GiveStartingItems();

	/** Server: takes out the inventory's weapon of this type (or puts the weapon away, e.g. for Unarmed) */
	void EquipWeaponOfType(EZWeaponType WeaponType);

	/** Server: puts this weapon item in the hands */
	void EquipWeaponItem(UZItemInstance* Item);

	/** Server: puts the weapon in the hands away (its ammo stays on the item) */
	void UnequipWeapon();

	/** The first weapon item of this type in the inventory, or null */
	UZItemInstance* FindWeaponItemOfType(EZWeaponType WeaponType) const;

	/** Server: an item is leaving the inventory; put it away first if it's in the hands */
	void OnInventoryItemRemoving(UZInventoryComponent* Inventory, UZItemInstance* Item);

	/** Asks the server to put this item in the hands */
	UFUNCTION(Server, Reliable)
	void ServerEquipItem(UZItemInstance* Item);

	/** Sends an ability input press or release to the ability system */
	void PressAbilityInput(EZAbilityInputID InputID);
	void ReleaseAbilityInput(EZAbilityInputID InputID);

	/** Cancels firing and reloading, e.g. when switching weapons */
	void CancelWeaponAbilities();

	/** Attribute as 0..1 of its max, for UI bars */
	float GetAttributePercent(const FGameplayAttribute& Attribute, const FGameplayAttribute& MaxAttribute) const;

	/** Asks the server to change the weapon type */
	UFUNCTION(Server, Reliable)
	void ServerSetWeaponType(EZWeaponType NewWeaponType);

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Survival Action Input Handlers (they press/release the matching ability) */
	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoSprintStart();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoSprintEnd();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoDodge();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoAimStart();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoAimEnd();

	/** Fire pressed: shoots (automatic weapons keep shooting while held); reloads instead when the magazine is empty */
	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoFireStart();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoFireEnd();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoReload();

	/** Interact pressed: uses what the player looks at (held for interactions that take time) */
	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoInteractStart();

	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	virtual void DoInteractEnd();

	/** Caps movement speed, e.g. Jog or Walk for the duration of a weapon swap or reload; None removes the cap. Call it on the owning player's machine (or in a predicted ability); it reaches the server with the next move */
	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	void SetSpeedLimit(EZSpeedLimit NewLimit);

	/** Switches to the inventory's weapon of this type (or just the animation set if there's none), right away for this player and through the server for everyone */
	UFUNCTION(BlueprintCallable, Category = "Survival|Weapon")
	void SetWeaponType(EZWeaponType NewWeaponType);

	/** Puts this inventory item in the hands (e.g. from an inventory screen); only weapons for now */
	UFUNCTION(BlueprintCallable, Category = "Survival|Weapon")
	void EquipItem(UZItemInstance* Item);

	/** The weapon in the hands, or null when unarmed */
	UFUNCTION(BlueprintPure, Category = "Survival|Weapon")
	AZWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	/** The inventory item in the hands (server and owning player), or null */
	UFUNCTION(BlueprintPure, Category = "Survival|Weapon")
	UZItemInstance* GetEquippedItem() const { return EquippedItem; }

	/** Reserve ammo for the weapon in the hands (in the inventory); -1 if it has unlimited reserve or there's no weapon */
	UFUNCTION(BlueprintPure, Category = "Survival|Weapon")
	int32 GetReserveAmmo() const;

	/** True if the weapon in the hands isn't full and there's ammo to load */
	bool CanReloadEquippedWeapon() const;

	/** The player's inventory (on their PlayerState), or null before it has arrived */
	UFUNCTION(BlueprintPure, Category = "Survival|Inventory")
	UZInventoryComponent* GetInventory() const;

	/** Finds and uses interactables */
	UZInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/** The Interact Input Action (e.g. so the HUD can show its key) */
	const UInputAction* GetInteractAction() const { return InteractAction; }

	/** Health as 0..1, for UI bars */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	float GetHealthPercent() const;

	/** Stamina as 0..1, for UI bars */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	float GetStaminaPercent() const;

	/** True after health reached 0, until the player respawns as a new character */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	bool IsDead() const { return bIsDead; }

	/** Sent by the server to this player's machine when one of their shots dealt damage (for the hit marker) */
	UFUNCTION(Client, Unreliable)
	void ClientConfirmHit(bool bKilledTarget);

	/** Seconds since this player's last confirmed hit (for the HUD's hit marker) */
	UFUNCTION(BlueprintPure, Category = "Survival|Weapon")
	float GetTimeSinceLastHit() const;

	/** True if the last confirmed hit killed its target */
	UFUNCTION(BlueprintPure, Category = "Survival|Weapon")
	bool WasLastHitAKill() const { return bLastHitWasKill; }

	/** Animation getters */
	UFUNCTION(BlueprintPure, Category = "Survival|Animation")
	bool IsAiming() const;

	UFUNCTION(BlueprintPure, Category = "Survival|Animation")
	EZWeaponType GetWeaponType() const { return CurrentWeaponType; }

	/** Aim direction relative to the character's facing, for Aim Offsets (Pitch = up/down, Yaw = left/right) */
	UFUNCTION(BlueprintPure, Category = "Survival|Animation")
	FRotator GetAimOffset() const;

	/** Max walk speed for the given state: aiming > sprinting > jog, capped by the speed limit. Used by the movement component */
	float GetMaxWalkSpeedFor(bool bSprinting, bool bAiming, EZSpeedLimit Limit) const;

	/** Walking (aiming or the Walk speed limit), jogging and sprinting speeds, for the footsteps */
	virtual FZGaitSpeeds GetGaitSpeeds() const override;

	/** True if the current movement direction is within MaxSprintAngle of where the character faces */
	bool CanSprintInMoveDirection() const;

	/** True if the character always faces the camera, not only while aiming */
	bool ShouldAlwaysFaceCamera() const { return bAlwaysFaceCamera; }

	/** Returns the movement component, which syncs sprint, aim and speed limits with the server */
	UZCharacterMovementComponent* GetZMovement() const;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

