// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGameCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ZSurvivalGame.h"

AZSurvivalGameCharacter::AZSurvivalGameCharacter()
{
	PrimaryActorTick.bCanEverTick = true; // Enable ticking for stamina management

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 250.0f; // Pulled closer than default 400
	CameraBoom->SocketOffset = FVector(0.f, 50.f, 15.f); // Shifted right over shoulder
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AZSurvivalGameCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Handle Sprinting Stamina Drain & Speed Control
	if (bIsSprinting && GetVelocity().SizeSquared() > 0.f)
	{
		CurrentStamina = FMath::Max(0.f, CurrentStamina - (StaminaDrainRate * DeltaTime));
		if (CurrentStamina <= 0.f)
		{
			DoSprintEnd(); // Stop sprinting when out of stamina
		}
	}
	else
	{
		// Regenerate stamina when not sprinting
		if (CurrentStamina < MaxStamina)
		{
			CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + (StaminaRegenRate * DeltaTime));
		}
	}
}

void AZSurvivalGameCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving & Looking
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AZSurvivalGameCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AZSurvivalGameCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AZSurvivalGameCharacter::Look);

		// Sprinting
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AZSurvivalGameCharacter::DoSprintStart);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AZSurvivalGameCharacter::DoSprintEnd);
		}

		// Dodging
		if (DodgeAction)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AZSurvivalGameCharacter::DoDodge);
		}
	}
	else
	{
		UE_LOG(LogZSurvivalGame, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AZSurvivalGameCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AZSurvivalGameCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AZSurvivalGameCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AZSurvivalGameCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AZSurvivalGameCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AZSurvivalGameCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AZSurvivalGameCharacter::DoSprintStart()
{
	if (CurrentStamina > 10.f) // Minimum stamina needed to initiate sprint
	{
		bIsSprinting = true;
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void AZSurvivalGameCharacter::DoSprintEnd()
{
	bIsSprinting = false;
	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
}

void AZSurvivalGameCharacter::DoDodge()
{
	if (CurrentStamina >= DodgeStaminaCost)
	{
		CurrentStamina -= DodgeStaminaCost;

		// Directional Impulse Dodge Roll
		FVector LaunchDirection = GetLastMovementInputVector();
		if (LaunchDirection.IsNearlyZero())
		{
			LaunchDirection = GetActorForwardVector(); // Fallback to forward dodge if stationary
		}

		LaunchDirection.Normalize();
		LaunchCharacter(LaunchDirection * 1200.f + FVector(0.f, 0.f, 150.f), true, true);

		// TODO Phase 1 Step 2: Play Dodge Roll Animation Montage here when ready
	}
}
