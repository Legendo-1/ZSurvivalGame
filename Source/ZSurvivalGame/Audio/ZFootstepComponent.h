// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "UObject/Interface.h"
#include "Audio/ZFootstepData.h"
#include "ZFootstepComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;
class USkeletalMeshComponent;
class USoundBase;

/** Which foot a footstep is for */
UENUM(BlueprintType)
enum class EZFootstepFoot : uint8
{
	/** The lower foot at that moment (the one touching the ground) */
	Auto,
	Left,
	Right,
	/** Between the feet, e.g. jumping and landing */
	Both
};

/** A character's walking, jogging and sprinting speeds. A speed counts as the gait it's closest to */
USTRUCT(BlueprintType)
struct FZGaitSpeeds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float Walk = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float Jog = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gait", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float Sprint = 550.f;

	EZGait GetGait(float Speed) const;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UZGaitSpeedsInterface : public UInterface
{
	GENERATED_BODY()
};

/** Characters whose walking, jogging and sprinting speeds come from their own movement settings (the player); their footsteps follow them */
class IZGaitSpeedsInterface
{
	GENERATED_BODY()

public:

	virtual FZGaitSpeeds GetGaitSpeeds() const = 0;
};

/**
 *  Plays a character's footsteps: the sound matches the surface under the foot (its Physical Material) and how fast the character moves
 *  Steps come from Footstep notifies in the animations (UZAnimNotify_Footstep); starts, stops, jumps and landings from the movement
 *  Every machine plays the footsteps of the characters it shows. Their animations and movement are on every machine already,
 *  so nothing is sent over the network; a dedicated server plays none
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class UZFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZFootstepComponent();

	/** Footstep sounds. Empty = Project Settings → Game → Footsteps → Default Footstep Data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TObjectPtr<UZFootstepData> FootstepData;

	/** Plays start, stop, jump and landing sounds from the movement. Turn it off for a character whose animations have their own Footstep notifies for these */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	bool bPlayMovementSounds = true;

	/** Walking, jogging and sprinting speeds, to tell them apart. The player uses its own movement speeds instead */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	FZGaitSpeeds GaitSpeeds;

	/** Bones of the feet: sounds play there, and the ground is looked for under them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", AdvancedDisplay)
	FName LeftFootBone = TEXT("foot_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", AdvancedDisplay)
	FName RightFootBone = TEXT("foot_r");

	/** Moving faster than this counts as starting to move */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", AdvancedDisplay, meta = (ClampMin = "1", Units = "CentimetersPerSecond"))
	float StartSpeed = 50.f;

	/** Slowing below this share of the recent top speed counts as stopping (a sharp turn scuffs too) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", AdvancedDisplay, meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float StopSpeedFraction = 0.4f;

	/** Leaving the ground faster than this upward is a jump; slower is walking off a ledge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Footsteps", AdvancedDisplay, meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float JumpSpeed = 150.f;

	/** Plays a footstep sound for the surface under this foot (from a Footstep notify, or the movement) */
	UFUNCTION(BlueprintCallable, Category = "Footsteps")
	void PlayFootstep(EZFootstepEvent Event, EZFootstepFoot Foot, EZGait Gait, float VolumeScale = 1.f, USkeletalMeshComponent* Mesh = nullptr);

	/** Walk, jog or sprint, from how fast the character moves right now */
	UFUNCTION(BlueprintPure, Category = "Footsteps")
	EZGait GetCurrentGait() const;

	/** The footstep sounds in use: FootstepData or the project's default */
	const UZFootstepData* GetFootstepData() const;

	/** A surface's name from Project Settings → Physics → Physical Surface ("Default" for none) */
	static FString GetSurfaceName(EPhysicalSurface Surface);

	/** The surface with this name (not case sensitive); false if there's none */
	static bool FindSurfaceByName(const FString& Name, EPhysicalSurface& OutSurface);

#if WITH_EDITOR
	/** The animation editor: plays the default footsteps for a Footstep notify, so its timing can be heard while placing it */
	static void PlayPreviewFootstep(USkeletalMeshComponent* Mesh, EZFootstepEvent Event, EZGait Gait);
#endif

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Starts and stops: from the speed on the ground */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Jumps and landings: leaving and reaching the ground */
	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

private:

	enum class EMoveState : uint8
	{
		Resting,
		Moving,
		/** Just stopped (the stop sound played); waits to stand still before the next start */
		Stopping
	};

	FZGaitSpeeds GetActiveGaitSpeeds() const;

	/** True where this character's movement input is known: its player's own machine and the server */
	bool KnowsInput() const;

	/** The gait the character is heading for. Where the input is known it comes from that; elsewhere from the speed */
	EZGait GetIntendedGait(const UCharacterMovementComponent& Movement, float Speed) const;

	void UpdateStartAndStop(const UCharacterMovementComponent& Movement, float DeltaTime);

	/** A start or stop sound, unless one played just before */
	void PlayMovementSound(EZFootstepEvent Event, EZGait Gait);

	FVector GetFootLocation(EZFootstepFoot Foot, const USkeletalMeshComponent* Mesh) const;

	static FVector FindFootLocation(const USkeletalMeshComponent& Mesh, EZFootstepFoot Foot, FName LeftBone, FName RightBone);

	/** Finds the surface under the foot and plays a matching sound there */
	static void PlayFootstepSound(UWorld& World, const UZFootstepData& Data, EZFootstepEvent Event, EZGait Gait, const FVector& FootLocation,
		const AActor* Owner, float VolumeScale, const USoundBase*& InOutLastSound);

	/** One of the sounds at random, not the one played last time */
	static USoundBase* PickSound(const TArray<TObjectPtr<USoundBase>>& Sounds, const USoundBase* LastSound);

	/** FootstepData or the project's default, kept loaded */
	UPROPERTY(Transient)
	TObjectPtr<UZFootstepData> ResolvedData;

	EMoveState MoveState = EMoveState::Resting;

	/** The gait of the current movement, and of the last one (for other players, whose input isn't known when they start) */
	EZGait MovingGait = EZGait::Jog;
	EZGait LastMovingGait = EZGait::Jog;

	/** World time the current movement started, of the last stop, and of the last start or stop sound */
	double MoveStartTime = 0.0;
	double StopTime = -1000.0;
	double LastMovementSoundTime = -1000.0;

	/** Recent top speed, falling off slowly, so a stop knows the gait it stopped from */
	float PeakSpeed = 0.f;

	/** Slowest speed since the last stop, to notice moving off again without standing still (a sharp turn) */
	float LowestSpeedSinceStop = 0.f;

	/** Fastest falling speed of the current fall, for the landing's loudness */
	float FallSpeed = 0.f;

	/** Only compared, never used: the sound played last, so the next one differs */
	const USoundBase* LastSound = nullptr;
};
