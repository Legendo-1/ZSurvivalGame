// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZLockOnComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "ZCharacterMovementComponent.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Combat/ZCombatSettings.h"
#include "Combat/ZLockOnSubsystem.h"
#include "Combat/ZLockOnTargetComponent.h"

namespace
{
	// how often the player's machine checks whether walls hide the target, in seconds
	constexpr float SightCheckInterval = 0.1f;

	// how far up or down the character and the camera turn toward a target, in degrees
	constexpr float MaxFacingPitch = 60.f;
	constexpr float MaxCameraPitch = 50.f;

	// extra distance the server accepts for a target a player reports (they see everyone a little out of date), in cm
	constexpr float ServerDistanceTolerance = 500.f;

	const TCHAR* GetReasonName(EZLockOnChangeReason Reason)
	{
		switch (Reason)
		{
		case EZLockOnChangeReason::TargetLost:	return TEXT("target lost");
		case EZLockOnChangeReason::TooFar:		return TEXT("too far");
		case EZLockOnChangeReason::OutOfSight:	return TEXT("out of sight");
		case EZLockOnChangeReason::Switched:	return TEXT("turned to it");
		case EZLockOnChangeReason::TurnedAway:	return TEXT("turned away");
		case EZLockOnChangeReason::Aiming:		return TEXT("aiming");
		case EZLockOnChangeReason::OwnerGone:	return TEXT("player gone");
		default:								return TEXT("input");
		}
	}
}

UZLockOnComponent::UZLockOnComponent()
{
	// ticks only on the player's own machine while locked on
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	SetIsReplicatedByDefault(true);
}

void UZLockOnComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// the owner has its own target
	DOREPLIFETIME_CONDITION(UZLockOnComponent, ReplicatedTarget, COND_SkipOwner);
}

void UZLockOnComponent::BeginPlay()
{
	Super::BeginPlay();

	// the camera's spring arm: while locked, it takes its rotation from us, so we tick before it (after the character moved this frame)
	CameraBoom = GetOwner() ? GetOwner()->FindComponentByClass<USpringArmComponent>() : nullptr;
	if (CameraBoom.IsValid())
	{
		CameraBoom->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	}
}

void UZLockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// the character is going away: give the camera back, nothing to tell anyone
	if (bLocked)
	{
		ReturnCamera(GetPlayerController());
		bLocked = false;
		LocalTarget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

APawn* UZLockOnComponent::GetPawn() const
{
	return Cast<APawn>(GetOwner());
}

APlayerController* UZLockOnComponent::GetPlayerController() const
{
	const APawn* Pawn = GetPawn();
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool UZLockOnComponent::IsLocalPlayer() const
{
	const APawn* Pawn = GetPawn();
	return Pawn && Pawn->IsLocallyControlled() && GetPlayerController();
}

UZLockOnTargetComponent* UZLockOnComponent::GetTarget() const
{
	const APawn* Pawn = GetPawn();
	return Pawn && Pawn->IsLocallyControlled() ? LocalTarget.Get() : ReplicatedTarget.Get();
}

AActor* UZLockOnComponent::GetTargetActor() const
{
	const UZLockOnTargetComponent* Target = GetTarget();
	return Target ? Target->GetOwner() : nullptr;
}

void UZLockOnComponent::ToggleLockOn()
{
	if (bLocked)
	{
		ReleaseLockOn();
	}
	else
	{
		LockOn();
	}
}

bool UZLockOnComponent::LockOn()
{
	if (!IsLocalPlayer() || UZCombatLibrary::IsDead(GetOwner()))
	{
		return false;
	}

	FZLockOnView View;
	UZLockOnTargetComponent* Target = GetView(View) ? FindBestTarget(View, nullptr) : nullptr;
	if (!Target)
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("Lock-on: '%s' found nothing to lock on to"), *GetNameSafe(GetOwner()));
		return false;
	}

	SetTarget(Target, EZLockOnChangeReason::Input);
	return true;
}

void UZLockOnComponent::ReleaseLockOn()
{
	if (bLocked)
	{
		SetTarget(nullptr, EZLockOnChangeReason::Input);
	}
}

bool UZLockOnComponent::ScoreTarget(const FZLockOnView& View, const FVector& TargetLocation, float MaxDistance, float MaxAngle, float DistanceWeight, float& OutScore)
{
	const float Distance = FVector::Dist(View.OwnerLocation, TargetLocation);
	const FVector ToTarget = (TargetLocation - View.ViewLocation).GetSafeNormal();
	if (Distance > MaxDistance || ToTarget.IsNearlyZero())
	{
		return false;
	}

	const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToTarget, View.ViewDirection.GetSafeNormal()), -1.f, 1.f)));
	if (Angle > MaxAngle)
	{
		return false;
	}

	OutScore = Angle + DistanceWeight * Distance / 100.f;
	return true;
}

bool UZLockOnComponent::GetView(FZLockOnView& OutView) const
{
	const APawn* Pawn = GetPawn();
	const APlayerController* PlayerController = GetPlayerController();
	if (!Pawn || !PlayerController)
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// while locked the camera manager shows last frame's view; our camera rotation is this frame's
	if (bLocked)
	{
		ViewRotation = CameraRotation;
	}

	OutView.ViewLocation = ViewLocation;
	OutView.ViewDirection = ViewRotation.Vector();
	OutView.OwnerLocation = Pawn->GetActorLocation();
	return true;
}

UZLockOnTargetComponent* UZLockOnComponent::FindBestTarget(const FZLockOnView& View, const UZLockOnTargetComponent* Exclude) const
{
	const UZLockOnSubsystem* Subsystem = UZLockOnSubsystem::Get(this);
	const UZCombatSettings* Settings = UZCombatSettings::Get();
	if (!Subsystem)
	{
		return nullptr;
	}

	// score every point in range and near the middle of the screen (cheap math only)
	TArray<TPair<float, UZLockOnTargetComponent*>, TInlineAllocator<16>> Candidates;
	for (const TWeakObjectPtr<UZLockOnTargetComponent>& Entry : Subsystem->GetTargets())
	{
		UZLockOnTargetComponent* Target = Entry.Get();
		const AActor* TargetOwner = Target ? Target->GetOwner() : nullptr;
		if (!Target || Target == Exclude || TargetOwner == GetOwner() || !Target->IsTargetable() || !UZCombatLibrary::CanDamage(GetOwner(), TargetOwner))
		{
			continue;
		}

		float Score = 0.f;
		if (ScoreTarget(View, Target->GetComponentLocation(), Settings->LockOnDistance, Settings->LockOnAngle, Settings->LockOnDistanceWeight, Score))
		{
			Candidates.Emplace(Score, Target);
		}
		else
		{
			UE_LOG(LogZSurvivalGame, VeryVerbose, TEXT("Lock-on: '%s' is out of range (%.0f cm from the character, %.0f deg from the middle of the screen)"), *GetNameSafe(TargetOwner),
				FVector::Dist(View.OwnerLocation, Target->GetComponentLocation()),
				FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct((Target->GetComponentLocation() - View.ViewLocation).GetSafeNormal(), View.ViewDirection), -1.f, 1.f))));
		}
	}

	// then the best one that walls don't hide (a trace only for the few left)
	Candidates.Sort([](const TPair<float, UZLockOnTargetComponent*>& A, const TPair<float, UZLockOnTargetComponent*>& B) { return A.Key < B.Key; });
	for (const TPair<float, UZLockOnTargetComponent*>& Candidate : Candidates)
	{
		if (HasLineOfSight(View.ViewLocation, Candidate.Value))
		{
			return Candidate.Value;
		}

		UE_LOG(LogZSurvivalGame, VeryVerbose, TEXT("Lock-on: '%s' is hidden behind level geometry"), *GetNameSafe(Candidate.Value->GetOwner()));
	}

	return nullptr;
}

bool UZLockOnComponent::HasLineOfSight(const FVector& From, const UZLockOnTargetComponent* Target) const
{
	if (!Target || !GetWorld())
	{
		return false;
	}

	// only level geometry hides a target: other characters (and our own weapon) don't
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZLockOnSight), false, GetOwner());
	Params.AddIgnoredActor(Target->GetOwner());

	TArray<AActor*> AttachedActors;
	GetOwner()->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	return !GetWorld()->LineTraceTestByObjectType(From, Target->GetComponentLocation(), Objects, Params);
}

void UZLockOnComponent::SetTarget(UZLockOnTargetComponent* NewTarget, EZLockOnChangeReason Reason)
{
	UZLockOnTargetComponent* OldTarget = LocalTarget.Get();
	const bool bLock = NewTarget != nullptr;
	if (NewTarget == OldTarget && bLock == bLocked)
	{
		return;
	}

	APawn* Pawn = GetPawn();
	APlayerController* PlayerController = GetPlayerController();
	if (bLock && !bLocked)
	{
		TakeCamera(PlayerController);
	}
	else if (!bLock && bLocked)
	{
		ReturnCamera(PlayerController);
	}

	bLocked = bLock;
	LocalTarget = NewTarget;

	// a new target starts in the middle and in sight
	LookOffset = FRotator::ZeroRotator;
	HiddenTime = 0.f;
	SightCheckCountdown = 0.f;
	bTargetHidden = false;

	// face the target (or not) the same way on the server: the movement component sends it with every move
	if (UZCharacterMovementComponent* Movement = Pawn ? Cast<UZCharacterMovementComponent>(Pawn->GetMovementComponent()) : nullptr)
	{
		Movement->SetWantsToStrafe(NewTarget != nullptr);
	}

	SetComponentTickEnabled(NewTarget != nullptr);

	if (NewTarget)
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("Lock-on: '%s' %s '%s' (%s), %.0f cm away"), *GetNameSafe(GetOwner()), OldTarget ? TEXT("switched to") : TEXT("locked on to"),
			*GetNameSafe(NewTarget->GetOwner()), GetReasonName(Reason), Pawn ? FVector::Dist(Pawn->GetActorLocation(), NewTarget->GetComponentLocation()) : 0.f);
	}
	else
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("Lock-on: '%s' let go of '%s' (%s)"), *GetNameSafe(GetOwner()), OldTarget ? *GetNameSafe(OldTarget->GetOwner()) : TEXT("its target"), GetReasonName(Reason));
	}

	// the server checks it and passes it on to the others (on the host this runs right here)
	ServerSetTarget(NewTarget);

	OnTargetChanged.Broadcast(NewTarget, OldTarget);
}

void UZLockOnComponent::TakeCamera(const APlayerController* PlayerController)
{
	CameraRotation = PlayerController && PlayerController->PlayerCameraManager ? PlayerController->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;
	CameraRotation.Roll = 0.f;
	LastControlRotation = PlayerController ? PlayerController->GetControlRotation() : FRotator::ZeroRotator;

	if (USpringArmComponent* Boom = CameraBoom.Get())
	{
		bBoomUsedPawnControlRotation = Boom->bUsePawnControlRotation;
		BoomRelativeRotation = Boom->GetRelativeRotation();
		Boom->bUsePawnControlRotation = false;
		Boom->SetWorldRotation(CameraRotation);
	}
}

void UZLockOnComponent::ReturnCamera(APlayerController* PlayerController)
{
	// the control rotation takes over the lock-on camera's rotation, so the view doesn't jump (the character then turns to it)
	if (PlayerController)
	{
		PlayerController->SetControlRotation(FRotator(CameraRotation.Pitch, CameraRotation.Yaw, 0.f));
	}

	if (USpringArmComponent* Boom = CameraBoom.Get())
	{
		Boom->bUsePawnControlRotation = bBoomUsedPawnControlRotation;
		Boom->SetRelativeRotation(BoomRelativeRotation);
	}
}

void UZLockOnComponent::ServerSetTarget_Implementation(UZLockOnTargetComponent* NewTarget)
{
	// only real, living, hostile targets roughly in range (the player's machine sees everyone a little out of date)
	const AActor* Owner = GetOwner();
	if (NewTarget && (!Owner || !NewTarget->IsTargetable() || !UZCombatLibrary::CanDamage(Owner, NewTarget->GetOwner())
		|| FVector::Dist(Owner->GetActorLocation(), NewTarget->GetComponentLocation()) > UZCombatSettings::Get()->LoseLockDistance + ServerDistanceTolerance))
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("Lock-on: the server ignores '%s' locking on to '%s' (dead, friendly or too far)"), *GetNameSafe(Owner), *GetNameSafe(NewTarget->GetOwner()));
		NewTarget = nullptr;
	}

	UZLockOnTargetComponent* OldTarget = ReplicatedTarget;
	if (OldTarget == NewTarget)
	{
		return;
	}

	ReplicatedTarget = NewTarget;
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("Lock-on: the server has '%s' locked on to '%s'"), *GetNameSafe(Owner), NewTarget ? *GetNameSafe(NewTarget->GetOwner()) : TEXT("nothing"));

	// the host's own character already told its machine; for the others the server is their machine here
	if (!IsLocalPlayer())
	{
		OnTargetChanged.Broadcast(NewTarget, OldTarget);
	}
}

void UZLockOnComponent::OnRep_ReplicatedTarget(UZLockOnTargetComponent* OldTarget)
{
	OnTargetChanged.Broadcast(ReplicatedTarget, OldTarget);
}

void UZLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bLocked)
	{
		UpdateLock(DeltaTime);
	}
}

void UZLockOnComponent::UpdateLock(float DeltaTime)
{
	APawn* Pawn = GetPawn();
	APlayerController* PlayerController = GetPlayerController();
	const UZCombatSettings* Settings = UZCombatSettings::Get();

	// dead, or no longer the player's: let go
	if (!Pawn || !PlayerController || !Pawn->IsLocallyControlled() || UZCombatLibrary::IsDead(Pawn))
	{
		SetTarget(nullptr, EZLockOnChangeReason::OwnerGone);
		return;
	}

	// aiming a gun: the player aims themselves
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (Settings->bReleaseLockOnWhenAiming && ASC && ASC->HasMatchingGameplayTag(ZGameplayTags::State_Aiming))
	{
		SetTarget(nullptr, EZLockOnChangeReason::Aiming);
		return;
	}

	// the player's look input since last frame: whatever moved the control rotation after we set it (mouse or stick, any input scaling)
	const FRotator LookInput = (PlayerController->GetControlRotation() - LastControlRotation).GetNormalized();
	LookOffset.Yaw += LookInput.Yaw;
	LookOffset.Pitch += LookInput.Pitch;
	LookOffset = FMath::RInterpTo(LookOffset, FRotator::ZeroRotator, DeltaTime, Settings->LockOnCameraReturnSpeed);
	LookOffset.Roll = 0.f;

	UZLockOnTargetComponent* Target = LocalTarget.Get();
	FZLockOnView View;
	GetView(View);

	// the target died or was switched off: the next one near the middle of the screen, or let go
	if (!Target || !Target->IsTargetable())
	{
		UZLockOnTargetComponent* NextTarget = Settings->bSwitchTargetOnKill ? FindBestTarget(View, Target) : nullptr;
		SetTarget(NextTarget, EZLockOnChangeReason::TargetLost);
		if (!NextTarget)
		{
			return;
		}

		Target = NextTarget;
	}

	if (FVector::Dist(Pawn->GetActorLocation(), Target->GetComponentLocation()) > Settings->LoseLockDistance)
	{
		SetTarget(nullptr, EZLockOnChangeReason::TooFar);
		return;
	}

	// turned the camera away from the target: switch to another enemy as soon as it's near the middle of the screen; turned far away without one, let go
	const float TurnedAngle = FMath::Sqrt(FMath::Square(LookOffset.Yaw) + FMath::Square(LookOffset.Pitch));
	if (TurnedAngle > Settings->SwitchTargetAngle)
	{
		// where the player turned the camera to: the direction of the target plus the turn
		View.ViewDirection = ((Target->GetComponentLocation() - View.ViewLocation).Rotation() + FRotator(LookOffset.Pitch, LookOffset.Yaw, 0.f)).Vector();
		if (UZLockOnTargetComponent* NextTarget = FindBestTarget(View, Target))
		{
			SetTarget(NextTarget, EZLockOnChangeReason::Switched);
			Target = NextTarget;
		}
		else if (TurnedAngle > Settings->BreakLockAngle)
		{
			SetTarget(nullptr, EZLockOnChangeReason::TurnedAway);
			return;
		}
	}

	// walls hiding the target (checked a few times a second): let go after Lose Sight Time
	SightCheckCountdown -= DeltaTime;
	if (SightCheckCountdown <= 0.f)
	{
		SightCheckCountdown = SightCheckInterval;
		bTargetHidden = !HasLineOfSight(View.ViewLocation, Target);
	}

	HiddenTime = bTargetHidden ? HiddenTime + DeltaTime : 0.f;
	if (HiddenTime > Settings->LoseSightTime)
	{
		SetTarget(nullptr, EZLockOnChangeReason::OutOfSight);
		return;
	}

	const FVector TargetLocation = Target->GetComponentLocation();

	// the character faces the target: the control rotation, which the movement turns it to (and sends to the server with every move)
	FRotator Facing = (TargetLocation - Pawn->GetActorLocation()).Rotation();
	Facing.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Facing.Pitch), -MaxFacingPitch, MaxFacingPitch);
	Facing.Roll = 0.f;
	PlayerController->SetControlRotation(Facing);
	LastControlRotation = Facing;

	// the camera follows the target, turned by the player's look offset; the spring arm takes its rotation from us
	const FVector CameraLocation = PlayerController->PlayerCameraManager ? PlayerController->PlayerCameraManager->GetCameraLocation() : View.ViewLocation;
	FRotator DesiredCamera = (TargetLocation - CameraLocation).Rotation();
	DesiredCamera.Pitch = FMath::Clamp(FRotator::NormalizeAxis(DesiredCamera.Pitch + LookOffset.Pitch), -MaxCameraPitch, MaxCameraPitch);
	DesiredCamera.Yaw += LookOffset.Yaw;
	DesiredCamera.Roll = 0.f;

	CameraRotation = FMath::RInterpTo(CameraRotation, DesiredCamera, DeltaTime, Settings->LockOnCameraSpeed);
	CameraRotation.Roll = 0.f;
	if (USpringArmComponent* Boom = CameraBoom.Get())
	{
		Boom->SetWorldRotation(CameraRotation);
	}
}

FString UZLockOnComponent::GetDebugString() const
{
	const UZLockOnTargetComponent* Target = GetTarget();
	const AActor* Owner = GetOwner();
	if (!Target || !Owner)
	{
		return TEXT("not locked on");
	}

	// how far the character's facing is from the target (it turns at its rotation rate, so a little while it moves)
	const float TargetYaw = (Target->GetComponentLocation() - Owner->GetActorLocation()).Rotation().Yaw;
	const float FacingError = FMath::Abs(FRotator::NormalizeAxis(TargetYaw - Owner->GetActorRotation().Yaw));

	return FString::Printf(TEXT("locked on to '%s' at %.0f cm, facing %.0f deg off it, camera turned %.0f deg away"), *GetNameSafe(Target->GetOwner()),
		FVector::Dist(Owner->GetActorLocation(), Target->GetComponentLocation()), FacingError, FMath::Sqrt(FMath::Square(LookOffset.Yaw) + FMath::Square(LookOffset.Pitch)));
}
