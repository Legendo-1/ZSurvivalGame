// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGameCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameGameMode.h"
#include "ZSurvivalGamePlayerState.h"
#include "ZCharacterMovementComponent.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Sprint.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Aim.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Dodge.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Fire.h"
#include "AbilitySystem/Abilities/ZGameplayAbility_Reload.h"
#include "Interaction/ZInteractionComponent.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemFragments.h"
#include "Items/ZItemInstance.h"
#include "Weapons/ZWeapon.h"
#include "Weapons/ZWeaponData.h"

AZSurvivalGameCharacter::AZSurvivalGameCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UZCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true; // Enable ticking for stamina management

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Weapon shots pass the capsule and hit the mesh's physics bodies, so a hit knows the bone (e.g. for headshots)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Block);

	// Don't snap to the controller rotation; the movement component smoothly turns us instead.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	// Face the camera direction by default (Remnant-style); the movement component switches this before each move
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
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

	InteractionComponent = CreateDefaultSubobject<UZInteractionComponent>(TEXT("Interaction"));

	// The player's abilities, given to the PlayerState's ability system in PossessedBy
	DefaultAbilities.Add(UZGameplayAbility_Sprint::StaticClass());
	DefaultAbilities.Add(UZGameplayAbility_Aim::StaticClass());
	DefaultAbilities.Add(UZGameplayAbility_Dodge::StaticClass());
	DefaultAbilities.Add(UZGameplayAbility_Fire::StaticClass());
	DefaultAbilities.Add(UZGameplayAbility_Reload::StaticClass());

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character)
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AZSurvivalGameCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// everyone needs the weapon type for the animations
	DOREPLIFETIME(AZSurvivalGameCharacter, CurrentWeaponType);

	// the owner already knows whether it's aiming
	DOREPLIFETIME_CONDITION(AZSurvivalGameCharacter, bIsAiming, COND_SkipOwner);

	// everyone sees the weapon in the hands; only the owner needs to know which inventory item it is
	DOREPLIFETIME(AZSurvivalGameCharacter, EquippedWeapon);
	DOREPLIFETIME_CONDITION(AZSurvivalGameCharacter, EquippedItem, COND_OwnerOnly);

	DOREPLIFETIME(AZSurvivalGameCharacter, bIsDead);
}

UAbilitySystemComponent* AZSurvivalGameCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

FGenericTeamId AZSurvivalGameCharacter::GetGenericTeamId() const
{
	return FGenericTeamId(static_cast<uint8>(EZTeam::Players));
}

void AZSurvivalGameCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Respect whatever boom length the Blueprint uses as the non-aiming distance
	DefaultArmLength = CameraBoom->TargetArmLength;
}

void AZSurvivalGameCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsLocallyControlled())
	{
		// Smoothly zoom the camera in/out for aiming (only the local player looks through this camera)
		const float TargetArmLength = IsAiming() ? AimArmLength : DefaultArmLength;
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaTime, AimZoomSpeed);

		// automatic weapons keep firing while the button is held (the Fire ability keeps the fire rate), and reload when empty
		if (bFireHeld && EquippedWeapon && EquippedWeapon->IsAutomatic())
		{
			if (EquippedWeapon->GetAmmo() > 0)
			{
				PressAbilityInput(EZAbilityInputID::Fire);
			}
			else if (CanReloadEquippedWeapon())
			{
				PressAbilityInput(EZAbilityInputID::Reload);
			}
		}
	}

	if (HasAuthority() && !bIsDead)
	{
		// keep the other players' copy of the aiming state up to date
		bIsAiming = IsAiming();

		UpdateStamina(DeltaTime);
	}
}

void AZSurvivalGameCharacter::Destroyed()
{
	if (HasAuthority())
	{
		UnequipWeapon();
	}

	Super::Destroyed();
}

void AZSurvivalGameCharacter::UpdateStamina(float DeltaTime)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const UZCharacterMovementComponent* Movement = GetZMovement();
	if (!ASC || !Movement)
	{
		return;
	}

	const FGameplayAttribute StaminaAttribute = UZAttributeSet::GetStaminaAttribute();
	const float OldStamina = ASC->GetNumericAttributeBase(StaminaAttribute);
	float NewStamina = OldStamina;

	// Handle Sprinting Stamina Drain (no drain while sprint is held but blocked by direction)
	if (Movement->IsSprinting() && GetVelocity().SizeSquared() > 0.f)
	{
		NewStamina = FMath::Max(0.f, OldStamina - (StaminaDrainRate * DeltaTime));
		if (NewStamina <= 0.f)
		{
			// Stop sprinting when out of stamina (the server ends the ability, which also ends it on the player's machine)
			const FGameplayTagContainer SprintTags(ZGameplayTags::Ability_Sprint.GetTag());
			ASC->CancelAbilities(&SprintTags);
		}
	}
	else
	{
		// Regenerate stamina when not sprinting (faster with the player's stamina regen from traits)
		const float MaxStamina = ASC->GetNumericAttribute(UZAttributeSet::GetMaxStaminaAttribute());
		const float RegenRate = StaminaRegenRate * ASC->GetNumericAttribute(UZAttributeSet::GetStaminaRegenMultiplierAttribute());
		NewStamina = FMath::Min(MaxStamina, OldStamina + (RegenRate * DeltaTime));
	}

	if (NewStamina != OldStamina)
	{
		ASC->SetNumericAttributeBase(StaminaAttribute, NewStamina);
	}
}

float AZSurvivalGameCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// health only changes on the server; the new value replicates to everyone (UZAttributeSet keeps it in range)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (HasAuthority() && ASC && ActualDamage > 0.f && !bIsDead)
	{
		const FGameplayAttribute HealthAttribute = UZAttributeSet::GetHealthAttribute();
		ASC->SetNumericAttributeBase(HealthAttribute, ASC->GetNumericAttributeBase(HealthAttribute) - ActualDamage);
	}

	return ActualDamage;
}

void AZSurvivalGameCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// the server sets up the ability system here; clients do it in OnRep_PlayerState
	InitAbilitySystem();
	GiveDefaultAbilities();
	GiveStartingItems();

	// put the weapon away when its item leaves the inventory (e.g. dropped)
	if (UZInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnItemRemoving.RemoveAll(this);
		Inventory->OnItemRemoving.AddUObject(this, &AZSurvivalGameCharacter::OnInventoryItemRemoving);
	}

	// take out the weapon for the starting animation set (e.g. set in Class Defaults)
	EquipWeaponOfType(CurrentWeaponType);
}

void AZSurvivalGameCharacter::UnPossessed()
{
	// this body no longer listens to the player's health and inventory (e.g. it died and the player respawned)
	if (AZSurvivalGamePlayerState* ZPlayerState = GetPlayerState<AZSurvivalGamePlayerState>())
	{
		ZPlayerState->GetAttributeSet()->OnOutOfHealth.RemoveAll(this);
		ZPlayerState->GetInventory()->OnItemRemoving.RemoveAll(this);
	}

	Super::UnPossessed();

	AbilitySystemComponent = nullptr;
}

void AZSurvivalGameCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilitySystem();
}

void AZSurvivalGameCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// on clients the controller can arrive after the PlayerState (the server already did this in PossessedBy)
	if (!HasAuthority())
	{
		InitAbilitySystem();
	}
}

void AZSurvivalGameCharacter::InitAbilitySystem()
{
	AZSurvivalGamePlayerState* ZPlayerState = GetPlayerState<AZSurvivalGamePlayerState>();
	if (!ZPlayerState)
	{
		UE_CLOG(GetPlayerState() != nullptr, LogZSurvivalGame, Warning, TEXT("'%s' has no AZSurvivalGamePlayerState, so it has no abilities. Check the GameMode's Player State Class."), *GetNameSafe(this));
		AbilitySystemComponent = nullptr;
		return;
	}

	// the PlayerState owns the ability system; this character is the body it acts through
	UAbilitySystemComponent* ASC = ZPlayerState->GetAbilitySystemComponent();
	ASC->InitAbilityActorInfo(ZPlayerState, this);
	AbilitySystemComponent = ASC;

	if (HasAuthority())
	{
		UZAttributeSet* Attributes = ZPlayerState->GetAttributeSet();
		Attributes->OnOutOfHealth.RemoveAll(this);
		Attributes->OnOutOfHealth.AddUObject(this, &AZSurvivalGameCharacter::HandleOutOfHealth);

		// respawned: the PlayerState still has the old character's death, so start fresh
		if (ASC->HasMatchingGameplayTag(ZGameplayTags::State_Dead))
		{
			ASC->RemoveLooseGameplayTag(ZGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);
			ASC->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), ASC->GetNumericAttribute(UZAttributeSet::GetMaxHealthAttribute()));
			ASC->SetNumericAttributeBase(UZAttributeSet::GetStaminaAttribute(), ASC->GetNumericAttribute(UZAttributeSet::GetMaxStaminaAttribute()));
		}
	}
}

void AZSurvivalGameCharacter::GiveDefaultAbilities()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !ASC)
	{
		return;
	}

	for (const TSubclassOf<UZGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		// the PlayerState keeps its abilities across respawns, so don't give them twice
		if (!AbilityClass || ASC->FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		// the input ID lets the ability react to its key (see PressAbilityInput)
		const EZAbilityInputID InputID = AbilityClass.GetDefaultObject()->InputID;
		const int32 SpecInputID = InputID == EZAbilityInputID::None ? INDEX_NONE : static_cast<int32>(InputID);
		ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, SpecInputID, this));
	}
}

void AZSurvivalGameCharacter::HandleOutOfHealth(AActor* DamageInstigator)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	UE_LOG(LogZSurvivalGame, Log, TEXT("Player '%s' died (killed by '%s')"), *GetNameSafe(this), *GetNameSafe(DamageInstigator));

	// death sound and effects for everyone
	FGameplayCueParameters DeathCue;
	DeathCue.Instigator = DamageInstigator;
	DeathCue.EffectCauser = DamageInstigator;
	UZGameplayCueLibrary::ExecuteCue(this, CharacterCues.DeathCue, DeathCue);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// State.Dead blocks every ability (see UZGameplayAbility); it replicates so clients block them too
		ASC->CancelAllAbilities();
		ASC->AddLooseGameplayTag(ZGameplayTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);
	}

	// the items stay in the inventory (TODO: decide whether dying drops some of them)
	UnequipWeapon();
	CurrentWeaponType = EZWeaponType::Unarmed;

	PlayDeath();

	// TODO: a downed state that teammates can revive, before dying for real
	if (AZSurvivalGameGameMode* GameMode = GetWorld()->GetAuthGameMode<AZSurvivalGameGameMode>())
	{
		GameMode->RequestRespawn(GetController());
	}
}

void AZSurvivalGameCharacter::PlayDeath()
{
	bFireHeld = false;

	// stop moving and fall over as a ragdoll
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	// the Ragdoll profile blocks every channel it doesn't name: let shots pass through the body so it can't shield the living
	GetMesh()->SetCollisionResponseToChannel(ZTraceChannel_Weapon, ECR_Ignore);
}

void AZSurvivalGameCharacter::OnRep_IsDead()
{
	if (bIsDead)
	{
		PlayDeath();
	}
}

void AZSurvivalGameCharacter::GiveStartingItems()
{
	AZSurvivalGamePlayerState* ZPlayerState = GetPlayerState<AZSurvivalGamePlayerState>();
	if (!HasAuthority() || !ZPlayerState || ZPlayerState->bReceivedStartingItems)
	{
		return;
	}

	// only once per player: after a respawn the inventory still has everything
	ZPlayerState->bReceivedStartingItems = true;

	for (const FZItemStack& Stack : StartingItems)
	{
		ZPlayerState->GetInventory()->AddItem(Stack.Item, Stack.Count);
	}
}

UZInventoryComponent* AZSurvivalGameCharacter::GetInventory() const
{
	const AZSurvivalGamePlayerState* ZPlayerState = GetPlayerState<AZSurvivalGamePlayerState>();
	return ZPlayerState ? ZPlayerState->GetInventory() : nullptr;
}

UZItemInstance* AZSurvivalGameCharacter::FindWeaponItemOfType(EZWeaponType WeaponType) const
{
	const UZInventoryComponent* Inventory = GetInventory();
	if (!Inventory || WeaponType == EZWeaponType::Unarmed)
	{
		return nullptr;
	}

	return Inventory->FindFirstInstanceMatching([WeaponType](const UZItemInstance* Item)
	{
		const UZItemFragment_Weapon* WeaponFragment = Item->FindFragment<UZItemFragment_Weapon>();
		return WeaponFragment && WeaponFragment->WeaponData && WeaponFragment->WeaponData->WeaponType == WeaponType;
	});
}

void AZSurvivalGameCharacter::EquipWeaponOfType(EZWeaponType WeaponType)
{
	if (UZItemInstance* WeaponItem = FindWeaponItemOfType(WeaponType))
	{
		EquipWeaponItem(WeaponItem);
	}
	else
	{
		// no weapon of this type: only the animation set changes
		UnequipWeapon();
	}

	CurrentWeaponType = WeaponType;
}

void AZSurvivalGameCharacter::EquipWeaponItem(UZItemInstance* Item)
{
	const UZItemFragment_Weapon* WeaponFragment = Item ? Item->FindFragment<UZItemFragment_Weapon>() : nullptr;
	if (!HasAuthority() || !WeaponFragment || !WeaponFragment->WeaponData || Item == EquippedItem)
	{
		return;
	}

	UnequipWeapon();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AZWeapon* Weapon = GetWorld()->SpawnActor<AZWeapon>(AZWeapon::StaticClass(), GetActorTransform(), SpawnParams))
	{
		Weapon->InitWeapon(WeaponFragment->WeaponData, Item);
		EquippedWeapon = Weapon;
		EquippedItem = Item;
		CurrentWeaponType = WeaponFragment->WeaponData->WeaponType;

		// the weapon's equip cue: its sound for everyone, and its Equip Montage on every machine (HandleGameplayCue)
		FGameplayCueParameters EquipCue;
		EquipCue.Instigator = this;
		EquipCue.EffectCauser = Weapon;
		EquipCue.SourceObject = WeaponFragment->WeaponData;
		UZGameplayCueLibrary::ExecuteCue(this, WeaponFragment->WeaponData->EquipCue, EquipCue);
	}
}

void AZSurvivalGameCharacter::HandleGameplayCue(UObject* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	// the animation for taking a weapon out comes with its equip cue, so every machine plays it at the moment it happens
	// (a player who comes near later doesn't see an old one)
	if (EventType == EGameplayCueEvent::Executed && GameplayCueTag.MatchesTag(ZGameplayTags::GameplayCue_Weapon_Equip) && !bIsDead)
	{
		const UZWeaponData* WeaponData = Cast<UZWeaponData>(Parameters.SourceObject.Get());
		if (UAnimMontage* EquipMontage = WeaponData ? WeaponData->EquipMontage.Get() : nullptr)
		{
			// it starts on every machine on its own, without prediction, so one that moves the character would put its owner out of step with the server
			if (EquipMontage->HasRootMotion())
			{
				UE_LOG(LogZSurvivalGame, Warning, TEXT("'%s' doesn't play the equip animation '%s': it moves the character (root motion), and take-out animations must stay in place"), *GetNameSafe(this), *GetNameSafe(EquipMontage));
			}
			else if (PlayAnimMontage(EquipMontage) > 0.f)
			{
				UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' plays the equip animation '%s'"), *GetNameSafe(this), *GetNameSafe(EquipMontage));
			}
			else
			{
				UE_LOG(LogZSurvivalGame, Warning, TEXT("'%s' can't play the equip animation '%s' (made for another skeleton?)"), *GetNameSafe(this), *GetNameSafe(EquipMontage));
			}
		}
	}

	// the default: Blueprint functions named after the tag
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void AZSurvivalGameCharacter::UnequipWeapon()
{
	if (!HasAuthority() || (!EquippedWeapon && !EquippedItem))
	{
		return;
	}

	// stop shooting or reloading it
	CancelWeaponAbilities();

	// the item keeps the magazine for next time
	if (EquippedWeapon)
	{
		EquippedWeapon->SaveAmmoToItem();
		EquippedWeapon->Destroy();
	}

	EquippedWeapon = nullptr;
	EquippedItem = nullptr;
}

void AZSurvivalGameCharacter::OnInventoryItemRemoving(UZInventoryComponent* Inventory, UZItemInstance* Item)
{
	if (Item && Item == EquippedItem)
	{
		UnequipWeapon();
		CurrentWeaponType = EZWeaponType::Unarmed;
	}
}

void AZSurvivalGameCharacter::EquipItem(UZItemInstance* Item)
{
	if (!bIsDead)
	{
		ServerEquipItem(Item);
	}
}

void AZSurvivalGameCharacter::ServerEquipItem_Implementation(UZItemInstance* Item)
{
	// only items from this player's own inventory
	const UZInventoryComponent* Inventory = GetInventory();
	if (!bIsDead && Inventory && Inventory->GetStackCount(Item) > 0)
	{
		EquipWeaponItem(Item);
	}
}

int32 AZSurvivalGameCharacter::GetReserveAmmo() const
{
	const UZItemDefinition* AmmoItem = EquippedWeapon ? EquippedWeapon->GetAmmoItem() : nullptr;
	const UZInventoryComponent* Inventory = GetInventory();
	return AmmoItem && Inventory ? Inventory->GetItemCount(AmmoItem) : -1;
}

bool AZSurvivalGameCharacter::CanReloadEquippedWeapon() const
{
	if (!EquippedWeapon || EquippedWeapon->GetAmmo() >= EquippedWeapon->GetMagazineSize())
	{
		return false;
	}

	// -1 = unlimited reserve
	const int32 ReserveAmmo = GetReserveAmmo();
	return ReserveAmmo != 0;
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

		// Aiming
		if (AimAction)
		{
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AZSurvivalGameCharacter::DoAimStart);
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AZSurvivalGameCharacter::DoAimEnd);
		}

		// Firing
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AZSurvivalGameCharacter::DoFireStart);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AZSurvivalGameCharacter::DoFireEnd);
		}

		// Reloading
		if (ReloadAction)
		{
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Started, this, &AZSurvivalGameCharacter::DoReload);
		}

		// Interacting
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AZSurvivalGameCharacter::DoInteractStart);
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &AZSurvivalGameCharacter::DoInteractEnd);
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
	// the Sprint ability checks aiming, speed limits and stamina
	PressAbilityInput(EZAbilityInputID::Sprint);
}

void AZSurvivalGameCharacter::DoSprintEnd()
{
	ReleaseAbilityInput(EZAbilityInputID::Sprint);
}

void AZSurvivalGameCharacter::DoAimStart()
{
	// the Aim ability also cancels sprint
	PressAbilityInput(EZAbilityInputID::Aim);
}

void AZSurvivalGameCharacter::DoAimEnd()
{
	ReleaseAbilityInput(EZAbilityInputID::Aim);
}

void AZSurvivalGameCharacter::DoDodge()
{
	// a dodge is a single press; the Dodge ability checks and pays the stamina
	PressAbilityInput(EZAbilityInputID::Dodge);
	ReleaseAbilityInput(EZAbilityInputID::Dodge);
}

void AZSurvivalGameCharacter::DoFireStart()
{
	bFireHeld = true;

	// empty magazine: reload instead (if there's ammo for it)
	if (EquippedWeapon && EquippedWeapon->GetAmmo() <= 0)
	{
		if (CanReloadEquippedWeapon())
		{
			DoReload();
		}

		return;
	}

	PressAbilityInput(EZAbilityInputID::Fire);
}

void AZSurvivalGameCharacter::DoFireEnd()
{
	bFireHeld = false;

	ReleaseAbilityInput(EZAbilityInputID::Fire);
}

void AZSurvivalGameCharacter::DoReload()
{
	// a reload is a single press; the Reload ability checks there's something to reload
	PressAbilityInput(EZAbilityInputID::Reload);
	ReleaseAbilityInput(EZAbilityInputID::Reload);
}

void AZSurvivalGameCharacter::DoInteractStart()
{
	if (!bIsDead)
	{
		InteractionComponent->StartInteraction();
	}
}

void AZSurvivalGameCharacter::DoInteractEnd()
{
	InteractionComponent->StopInteraction();
}

void AZSurvivalGameCharacter::PressAbilityInput(EZAbilityInputID InputID)
{
	// activates the ability with this input ID, predicted on this machine and confirmed by the server
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->AbilityLocalInputPressed(static_cast<int32>(InputID));
	}
}

void AZSurvivalGameCharacter::ReleaseAbilityInput(EZAbilityInputID InputID)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->AbilityLocalInputReleased(static_cast<int32>(InputID));
	}
}

void AZSurvivalGameCharacter::CancelWeaponAbilities()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		FGameplayTagContainer WeaponAbilityTags;
		WeaponAbilityTags.AddTag(ZGameplayTags::Ability_Fire);
		WeaponAbilityTags.AddTag(ZGameplayTags::Ability_Reload);
		ASC->CancelAbilities(&WeaponAbilityTags);
	}
}

void AZSurvivalGameCharacter::SetSpeedLimit(EZSpeedLimit NewLimit)
{
	UZCharacterMovementComponent* Movement = GetZMovement();
	if (!Movement)
	{
		return;
	}

	// Any speed limit cancels sprint
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (NewLimit != EZSpeedLimit::None && ASC)
	{
		const FGameplayTagContainer SprintTags(ZGameplayTags::Ability_Sprint.GetTag());
		ASC->CancelAbilities(&SprintTags);
	}

	// the movement component sends it to the server with the next move
	Movement->SetSpeedLimit(NewLimit);
}

void AZSurvivalGameCharacter::SetWeaponType(EZWeaponType NewWeaponType)
{
	if (bIsDead)
	{
		return;
	}

	// stop shooting or reloading the old weapon
	if (NewWeaponType != CurrentWeaponType)
	{
		CancelWeaponAbilities();
	}

	// the animation set changes right away for this player; the server swaps the weapon for everyone
	CurrentWeaponType = NewWeaponType;

	if (HasAuthority())
	{
		EquipWeaponOfType(NewWeaponType);
	}
	else
	{
		ServerSetWeaponType(NewWeaponType);
	}
}

void AZSurvivalGameCharacter::ServerSetWeaponType_Implementation(EZWeaponType NewWeaponType)
{
	if (!bIsDead)
	{
		EquipWeaponOfType(NewWeaponType);
	}
}

void AZSurvivalGameCharacter::ClientConfirmHit_Implementation(bool bKilledTarget)
{
	LastHitTime = GetWorld()->GetTimeSeconds();
	bLastHitWasKill = bKilledTarget;
}

float AZSurvivalGameCharacter::GetTimeSinceLastHit() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastHitTime) : TNumericLimits<float>::Max();
}

float AZSurvivalGameCharacter::GetHealthPercent() const
{
	return GetAttributePercent(UZAttributeSet::GetHealthAttribute(), UZAttributeSet::GetMaxHealthAttribute());
}

float AZSurvivalGameCharacter::GetStaminaPercent() const
{
	return GetAttributePercent(UZAttributeSet::GetStaminaAttribute(), UZAttributeSet::GetMaxStaminaAttribute());
}

float AZSurvivalGameCharacter::GetAttributePercent(const FGameplayAttribute& Attribute, const FGameplayAttribute& MaxAttribute) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC)
	{
		return 0.f;
	}

	const float MaxValue = ASC->GetNumericAttribute(MaxAttribute);
	return MaxValue > 0.f ? ASC->GetNumericAttribute(Attribute) / MaxValue : 0.f;
}

bool AZSurvivalGameCharacter::IsAiming() const
{
	// the owner and the server know it from the movement component; the other players get a replicated copy
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return bIsAiming;
	}

	const UZCharacterMovementComponent* Movement = GetZMovement();
	return Movement && Movement->WantsToAim();
}

float AZSurvivalGameCharacter::GetMaxWalkSpeedFor(bool bSprinting, bool bAiming, EZSpeedLimit Limit) const
{
	// Base speed from state: aiming > sprinting (forward/sideways only) > jog
	float NewSpeed = JogSpeed;

	if (bAiming)
	{
		NewSpeed = AimWalkSpeed;
	}
	else if (bSprinting)
	{
		NewSpeed = SprintSpeed;
	}

	// Apply the temporary cap (weapon swap, reload, ...)
	if (Limit == EZSpeedLimit::Jog)
	{
		NewSpeed = FMath::Min(NewSpeed, JogSpeed);
	}
	else if (Limit == EZSpeedLimit::Walk)
	{
		NewSpeed = FMath::Min(NewSpeed, WalkSpeed);
	}

	return NewSpeed;
}

bool AZSurvivalGameCharacter::CanSprintInMoveDirection() const
{
	// Use the input direction so the check reacts immediately, not after the velocity has turned
	const FVector MoveDirection = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (MoveDirection.IsNearlyZero())
	{
		return true; // Not pressing a direction, nothing to block
	}

	const FVector FacingDirection = GetActorForwardVector().GetSafeNormal2D();
	return FVector::DotProduct(MoveDirection, FacingDirection) >= FMath::Cos(FMath::DegreesToRadians(MaxSprintAngle));
}

UZCharacterMovementComponent* AZSurvivalGameCharacter::GetZMovement() const
{
	return Cast<UZCharacterMovementComponent>(GetCharacterMovement());
}

FRotator AZSurvivalGameCharacter::GetAimOffset() const
{
	// difference between where the camera aims and where the body faces, kept in -180..180
	// (on other players' machines the engine replicates the aim pitch, see APawn::GetBaseAimRotation)
	return (GetBaseAimRotation() - GetActorRotation()).GetNormalized();
}
