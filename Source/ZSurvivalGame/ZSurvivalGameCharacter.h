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
	float JogSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement")
	float SprintSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Survival|Movement")
	float AimWalkSpeed = 300.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival|State")
	bool bIsSprinting = false;

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

	/** Health as 0..1, for UI bars */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	/** Stamina as 0..1, for UI bars */
	UFUNCTION(BlueprintPure, Category = "Survival|Stats")
	float GetStaminaPercent() const { return MaxStamina > 0.f ? CurrentStamina / MaxStamina : 0.f; }

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

