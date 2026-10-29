// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZLockOnComponent.generated.h"

class APawn;
class APlayerController;
class USpringArmComponent;
class UZLockOnTargetComponent;

/** Why a lock ended or moved to another target */
UENUM(BlueprintType)
enum class EZLockOnChangeReason : uint8
{
	/** The lock-on key, or a new lock */
	Input,
	/** The target died or can't be targeted any more */
	TargetLost,
	/** The target got farther than Lose Lock Distance */
	TooFar,
	/** Walls hid the target longer than Lose Sight Time */
	OutOfSight,
	/** The player turned the camera toward another enemy */
	Switched,
	/** The player turned the camera past Break Lock Angle */
	TurnedAway,
	/** Aiming a gun (Release Lock On When Aiming) */
	Aiming,
	/** The player died, or no longer controls the character */
	OwnerGone
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZLockOnTargetChangedSignature, UZLockOnTargetComponent*, NewTarget, UZLockOnTargetComponent*, OldTarget);

/** Where the player looks from, for picking a target (also used by the automated tests) */
struct FZLockOnView
{
	/** The camera */
	FVector ViewLocation = FVector::ZeroVector;

	/** Where the middle of the screen points (unit length) */
	FVector ViewDirection = FVector::ForwardVector;

	/** The player's character (distances are measured from it) */
	FVector OwnerLocation = FVector::ZeroVector;
};

/**
 *  Lock-on for a player's character (see COMBAT.md → Lock-on): the lock-on key picks the enemy nearest the middle of the screen
 *  within Lock On Distance that walls don't hide; then the character faces it and the camera follows it
 *
 *  Multiplayer: everything runs on the player's own machine. The character faces the target through the control rotation (sent to the server
 *  with every move, like the camera direction always was), and the movement component strafes while locked (a move flag), so the server moves
 *  and turns the character the same way. The server gets the target too (one RPC per change), checks it, and replicates it to the other players
 *  (e.g. for animations); it never decides anything with it
 *
 *  While locked, the camera has its own rotation (the spring arm stops following the control rotation). Mouse or stick input turns it away from
 *  the target and it drifts back. Turned at least Switch Target Angle toward another enemy (it comes near the middle of the screen), the lock
 *  switches to it; turned past Break Lock Angle without one, it lets go
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UZLockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZLockOnComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The lock-on key (player's own machine): locks on to the enemy nearest the middle of the screen, or lets go if already locked */
	UFUNCTION(BlueprintCallable, Category = "Lock-On")
	void ToggleLockOn();

	/** Player's own machine: locks on to the enemy nearest the middle of the screen; false if there's none in range and in sight */
	UFUNCTION(BlueprintCallable, Category = "Lock-On")
	bool LockOn();

	/** Player's own machine: lets go of the target */
	UFUNCTION(BlueprintCallable, Category = "Lock-On")
	void ReleaseLockOn();

	/** The point locked on to: on the player's own machine its current target, elsewhere the server's copy; null when not locked on */
	UFUNCTION(BlueprintPure, Category = "Lock-On")
	UZLockOnTargetComponent* GetTarget() const;

	/** The actor locked on to, or null */
	UFUNCTION(BlueprintPure, Category = "Lock-On")
	AActor* GetTargetActor() const;

	UFUNCTION(BlueprintPure, Category = "Lock-On")
	bool IsLockedOn() const { return GetTarget() != nullptr; }

	/** The target changed on this machine (locked, switched or let go) */
	UPROPERTY(BlueprintAssignable, Category = "Lock-On")
	FZLockOnTargetChangedSignature OnTargetChanged;

	/**
	 *  Whether a point could be locked on to from this view, and how good a choice it is (lower is better: degrees from the middle of the screen,
	 *  plus DistanceWeight per meter from the character). False if it's farther than MaxDistance from the character or more than MaxAngle from the middle
	 */
	static bool ScoreTarget(const FZLockOnView& View, const FVector& TargetLocation, float MaxDistance, float MaxAngle, float DistanceWeight, float& OutScore);

	/** "locked on to X at 2.1 m" for the ZCombat cheat */
	FString GetDebugString() const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Tells the server the new target (null: none), which checks it and passes it on to the other players */
	UFUNCTION(Server, Reliable)
	void ServerSetTarget(UZLockOnTargetComponent* NewTarget);

	UFUNCTION()
	void OnRep_ReplicatedTarget(UZLockOnTargetComponent* OldTarget);

private:

	/** The best point to lock on to from this view (not Exclude), that walls don't hide; null if none */
	UZLockOnTargetComponent* FindBestTarget(const FZLockOnView& View, const UZLockOnTargetComponent* Exclude) const;

	/** True if walls (static and moving level geometry, not characters) don't hide the point from this location */
	bool HasLineOfSight(const FVector& From, const UZLockOnTargetComponent* Target) const;

	/** Player's own machine: switches target (null: lets go), hands the camera over and tells the server */
	void SetTarget(UZLockOnTargetComponent* NewTarget, EZLockOnChangeReason Reason);

	/** The lock-on camera takes over from the control rotation where the camera is now, and gives it back the same way */
	void TakeCamera(const APlayerController* PlayerController);
	void ReturnCamera(APlayerController* PlayerController);

	/** Player's own machine, every frame while locked: lets go or switches when needed, then turns the character and the camera to the target */
	void UpdateLock(float DeltaTime);

	/** The camera's view for picking targets: the camera itself before locking, the lock-on camera's direction while locked */
	bool GetView(FZLockOnView& OutView) const;

	APawn* GetPawn() const;
	APlayerController* GetPlayerController() const;
	bool IsLocalPlayer() const;

	/** The point locked on to, on the player's own machine */
	TWeakObjectPtr<UZLockOnTargetComponent> LocalTarget;

	/** True while locked on, on the player's own machine (also if the target was just destroyed: the camera is still ours) */
	bool bLocked = false;

	/** The server's copy of the target, for the server and the other players (not the owner, who has its own) */
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedTarget)
	TObjectPtr<UZLockOnTargetComponent> ReplicatedTarget;

	/** The spring arm the camera hangs on; it follows our rotation while locked */
	TWeakObjectPtr<USpringArmComponent> CameraBoom;
	bool bBoomUsedPawnControlRotation = true;
	FRotator BoomRelativeRotation = FRotator::ZeroRotator;

	/** The lock-on camera's rotation, and how far the player turned it away from the target (drifts back to zero) */
	FRotator CameraRotation = FRotator::ZeroRotator;
	FRotator LookOffset = FRotator::ZeroRotator;

	/** The control rotation as we set it last frame: what changed it since then is the player's look input */
	FRotator LastControlRotation = FRotator::ZeroRotator;

	/** How long walls have hidden the target, and when to check again */
	float HiddenTime = 0.f;
	float SightCheckCountdown = 0.f;
	bool bTargetHidden = false;
};
