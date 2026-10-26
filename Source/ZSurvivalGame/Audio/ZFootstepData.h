// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "ZFootstepData.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** How fast a character moves; each has its own footstep sounds */
UENUM(BlueprintType)
enum class EZGait : uint8
{
	Walk,
	Jog,
	Sprint
};

/** What a footstep sound is for */
UENUM(BlueprintType)
enum class EZFootstepEvent : uint8
{
	/** A foot touches the ground while moving */
	Step,
	/** Starting to move, or speeding up to a faster gait (e.g. starting to sprint) */
	Start,
	/** Coming to a stop */
	Stop,
	/** Leaving the ground in a jump */
	Jump,
	/** Landing after a jump or a fall */
	Land
};

/** One gait's sounds. Each time, one of the list is picked at random (never the same one twice in a row) */
USTRUCT(BlueprintType)
struct FZGaitFootstepSounds
{
	GENERATED_BODY()

	/** Each step */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Steps;

	/** Starting to move at this gait, or speeding up to it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Start;

	/** Coming to a stop from this gait */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Stop;
};

/** All footstep sounds on one surface. A list can hold sound waves, Sound Cues or MetaSounds */
USTRUCT(BlueprintType)
struct FZSurfaceFootstepSounds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	FZGaitFootstepSounds Walk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	FZGaitFootstepSounds Jog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	FZGaitFootstepSounds Sprint;

	/** Leaving the ground in a jump */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Jump;

	/** Landing; harder landings play louder */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footsteps")
	TArray<TObjectPtr<USoundBase>> Land;

	/** The list for this event (the gait only matters for steps, starts and stops) */
	const TArray<TObjectPtr<USoundBase>>& GetSounds(EZFootstepEvent Event, EZGait Gait) const;
};

/**
 *  Which footstep sounds play on which surface. Characters use it through their Footsteps component (UZFootstepComponent)
 *  A surface is a Physical Surface (Project Settings → Physics); floors get one from a Physical Material (see AUDIO.md)
 */
UCLASS(BlueprintType)
class UZFootstepData : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Sounds per surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sounds")
	TMap<TEnumAsByte<EPhysicalSurface>, FZSurfaceFootstepSounds> Surfaces;

	/** Floors without a surface (no Physical Material), or with one that isn't in Surfaces, sound like this one */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sounds")
	TEnumAsByte<EPhysicalSurface> FallbackSurface = SurfaceType1;

	/** How far footsteps are heard and how they fade with distance. Without one, everyone hears them at full volume anywhere */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Limits how many footsteps play at once (e.g. a crowd of zombies). Optional */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback")
	TObjectPtr<USoundConcurrency> Concurrency;

	/** Loudness of all footsteps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback", meta = (ClampMin = "0"))
	float Volume = 1.f;

	/** Random variation of each sound's volume: 0.1 = 90% to 100% */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback", meta = (ClampMin = "0", ClampMax = "1"))
	float VolumeVariation = 0.1f;

	/** Random variation of each sound's pitch: 0.05 = 95% to 105% */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Playback", meta = (ClampMin = "0", ClampMax = "0.5"))
	float PitchVariation = 0.05f;

	/** Landings slower than this make no sound, e.g. stepping off a curb */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float MinLandSpeed = 250.f;

	/** Landings this fast or faster play at full volume; slower ones are quieter (down to half) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Landing", meta = (ClampMin = "0", Units = "CentimetersPerSecond"))
	float FullLandSpeed = 900.f;

	/** The sounds for this surface, or the fallback surface's if it has none */
	const FZSurfaceFootstepSounds* FindSounds(EPhysicalSurface Surface) const;

	/** How loud a landing at this falling speed is (0 = no sound) */
	float GetLandVolume(float FallSpeed) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
