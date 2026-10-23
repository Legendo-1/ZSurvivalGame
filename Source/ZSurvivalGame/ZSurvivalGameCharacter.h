// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ZSurvivalGameCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/** Which weapon animation set the character uses */
UENUM(BlueprintType)
enum class EZWeaponType : uint8
{
	Unarmed,
	Pistol,
	Rifle
};

/** Temporary cap on movement speed, e.g. during a weapon swap or reload */
UENUM(BlueprintType)
enum class EZSpeedLimit : uint8
{
	None,	// no cap
	Jog,	// can't sprint, jogging allowed
	Walk	// capped at WalkSpeed
};

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AZSurvivalGameCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
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

	/** --- SURVIVAL STATS & MOVEMENT --- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float CurrentHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float MaxStamina = 100.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float CurrentStamina = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float StaminaDrainRate = 15.f; // Stamina per second while sprinting

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float StaminaRegenRate = 10.f; // Stamina per second while idle/walking

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Stats")
	float DodgeStaminaCost = 25.f;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival|State")
	bool bIsSprinting = false;

	/** Active temporary speed cap (see SetSpeedLimit) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival|State")
	EZSpeedLimit SpeedLimit = EZSpeedLimit::None;

	/** --- AIMING --- */

	/** If true, the character always turns to face the camera direction; if false, only while aiming */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Aiming")
	bool bAlwaysFaceCamera = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival|State")
	bool bIsAiming = false;

	/** Camera boom length while aiming (closer over-the-shoulder view) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Aiming")
	float AimArmLength = 150.f;

	/** How fast the camera zooms in/out when aiming starts/stops */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Aiming")
	float AimZoomSpeed = 10.f;

	/** Boom length when not aiming, cached from the CameraBoom on BeginPlay */
	float DefaultArmLength = 250.f;

	/** --- WEAPON --- */

	/** Current weapon animation set (set by hand for now; the weapon system will drive this in Phase 2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Weapon")
	EZWeaponType CurrentWeaponType = EZWeaponType::Unarmed;

public:

	/** Constructor */
	AZSurvivalGameCharacter();

protected:

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/** Reduces health when damaged (e.g. via UGameplayStatics::ApplyDamage) */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	/** Switches between free movement rotation and facing the camera, based on current state */
	void UpdateRotationMode();

	/** Sets MaxWalkSpeed from the current state (aiming, sprinting or jog), capped by SpeedLimit */
	void UpdateMovementSpeed();

	/** True if the current movement direction is within MaxSprintAngle of where the character faces */
	bool CanSprintInMoveDirection() const;

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

	/** Survival Action Input Handlers */
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

	/** Caps movement speed, e.g. Jog or Walk for the duration of a weapon swap or reload; None removes the cap */
	UFUNCTION(BlueprintCallable, Category = "Survival|Actions")
	void SetSpeedLimit(EZSpeedLimit NewLimit);

	/** Health as 0..1, for UI bars */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	/** Stamina as 0..1, for UI bars */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	float GetStaminaPercent() const { return MaxStamina > 0.f ? CurrentStamina / MaxStamina : 0.f; }

	/** Animation getters */
	UFUNCTION(BlueprintPure, Category = "Survival|Animation")
	bool IsAiming() const { return bIsAiming; }

	UFUNCTION(BlueprintPure, Category = "Survival|Animation")
	EZWeaponType GetWeaponType() const { return CurrentWeaponType; }

	/** Aim direction relative to the character's facing, for Aim Offsets (Pitch = up/down, Yaw = left/right) */
	UFUNCTION(BlueprintPure, Category = "Survival|Animation")
	FRotator GetAimOffset() const;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

