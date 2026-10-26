// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/ZFootstepComponent.h"
#include "Audio/ZFootstepSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Sound/SoundBase.h"
#include "ZSurvivalGame.h"

namespace
{
	TAutoConsoleVariable<bool> CVarFootstepDebug(
		TEXT("Z.Footsteps.Debug"),
		false,
		TEXT("Writes every footstep to the Output Log (who, what, which surface, which sound) and draws it in the world. 0 = off"));

	// how far above and below the foot the ground is looked for
	constexpr float TraceUp = 30.f;
	constexpr float TraceDown = 60.f;

	// how fast the remembered top speed falls off (cm/s per second), so a stop after slowing down uses the slower gait
	constexpr float PeakSpeedFalloff = 400.f;

	// other players' machines don't know the input: while a character speeds up from a start, its gait isn't lowered yet
	constexpr double StartSettleTime = 0.35;

	// a stop sound has the last steps in it: steps right after it would double them
	constexpr double QuietStepsAfterStop = 0.35;

	// starts, stops and speed-ups of one character come at most this often (other players' machines see their speed jump around a little)
	constexpr double MinTimeBetweenMovementSounds = 0.3;

	FColor GetDebugColor(EZFootstepEvent Event)
	{
		switch (Event)
		{
		case EZFootstepEvent::Start: return FColor::Green;
		case EZFootstepEvent::Stop: return FColor::Red;
		case EZFootstepEvent::Jump: return FColor::Cyan;
		case EZFootstepEvent::Land: return FColor::Orange;
		default: return FColor::White;
		}
	}
}

EZGait FZGaitSpeeds::GetGait(float Speed) const
{
	if (Speed >= (Jog + Sprint) * 0.5f)
	{
		return EZGait::Sprint;
	}

	return Speed >= (Walk + Jog) * 0.5f ? EZGait::Jog : EZGait::Walk;
}

UZFootstepComponent::UZFootstepComponent()
{
	// after the movement has moved the character this frame
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UZFootstepComponent::BeginPlay()
{
	Super::BeginPlay();

	// a dedicated server plays no sounds
	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// loaded now rather than at the first step
	ResolvedData = FootstepData ? FootstepData.Get() : UZFootstepSettings::Get()->DefaultFootstepData.LoadSynchronous();
	UE_CLOG(!ResolvedData, LogZSurvivalGame, Warning, TEXT("'%s' has no footstep sounds: set Footstep Data on its Footsteps component, or Project Settings > Game > Footsteps > Default Footstep Data"),
		*GetNameSafe(GetOwner()));

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->MovementModeChangedDelegate.AddDynamic(this, &UZFootstepComponent::HandleMovementModeChanged);
	}
}

void UZFootstepComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->MovementModeChangedDelegate.RemoveDynamic(this, &UZFootstepComponent::HandleMovementModeChanged);
	}

	Super::EndPlay(EndPlayReason);
}

const UZFootstepData* UZFootstepComponent::GetFootstepData() const
{
	return ResolvedData ? ResolvedData.Get() : FootstepData.Get();
}

FZGaitSpeeds UZFootstepComponent::GetActiveGaitSpeeds() const
{
	const IZGaitSpeedsInterface* SpeedsOwner = Cast<IZGaitSpeedsInterface>(GetOwner());
	return SpeedsOwner ? SpeedsOwner->GetGaitSpeeds() : GaitSpeeds;
}

EZGait UZFootstepComponent::GetCurrentGait() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const float Speed = Character ? Character->GetVelocity().Size2D() : 0.f;
	return GetActiveGaitSpeeds().GetGait(Speed);
}

bool UZFootstepComponent::KnowsInput() const
{
	// a player's own machine and the server; AI moves along paths without input, other players' machines don't get it
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsPlayerControlled() && GetOwnerRole() != ROLE_SimulatedProxy;
}

EZGait UZFootstepComponent::GetIntendedGait(const UCharacterMovementComponent& Movement, float Speed) const
{
	// the speed it's heading for: the state's top speed (jog, sprint, aim) scaled by how far the stick is pushed
	if (KnowsInput() && !Movement.GetCurrentAcceleration().IsNearlyZero())
	{
		return GetActiveGaitSpeeds().GetGait(Movement.GetMaxSpeed() * Movement.GetAnalogInputModifier());
	}

	return GetActiveGaitSpeeds().GetGait(Speed);
}

void UZFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!bPlayMovementSounds || !Movement || !GetFootstepData())
	{
		return;
	}

	if (Movement->IsFalling())
	{
		// for the landing's loudness
		FallSpeed = FMath::Max(FallSpeed, static_cast<float>(-Movement->Velocity.Z));
		return;
	}

	if (!Movement->IsMovingOnGround())
	{
		// e.g. dead (movement off): start fresh
		MoveState = EMoveState::Resting;
		PeakSpeed = 0.f;
		return;
	}

	UpdateStartAndStop(*Movement, DeltaTime);
}

void UZFootstepComponent::UpdateStartAndStop(const UCharacterMovementComponent& Movement, float DeltaTime)
{
	const float Speed = Movement.Velocity.Size2D();
	const FZGaitSpeeds Speeds = GetActiveGaitSpeeds();
	const bool bKnowsInput = KnowsInput();
	const double Now = GetWorld()->GetTimeSeconds();

	PeakSpeed = FMath::Max(Speed, PeakSpeed - PeakSpeedFalloff * DeltaTime);

	switch (MoveState)
	{
	case EMoveState::Resting:
		// where the input is known, being pushed (e.g. by a zombie) isn't starting to move
		if (Speed > StartSpeed && (!bKnowsInput || !Movement.GetCurrentAcceleration().IsNearlyZero()))
		{
			// other players' machines guess the gait from their last movement, and correct it while they speed up
			MoveState = EMoveState::Moving;
			MoveStartTime = Now;
			MovingGait = bKnowsInput ? GetIntendedGait(Movement, Speed) : LastMovingGait;
			PlayMovementSound(EZFootstepEvent::Start, MovingGait);
		}
		break;

	case EMoveState::Moving:
	{
		// a stop: well below the recent top speed (a sharp turn scuffs too)
		if (Speed < PeakSpeed * StopSpeedFraction)
		{
			MoveState = EMoveState::Stopping;
			LowestSpeedSinceStop = Speed;
			StopTime = Now;
			LastMovingGait = Speeds.GetGait(PeakSpeed);
			PlayMovementSound(EZFootstepEvent::Stop, LastMovingGait);
			break;
		}

		EZGait Gait = GetIntendedGait(Movement, Speed);
		if (!bKnowsInput && Now - MoveStartTime < StartSettleTime)
		{
			Gait = FMath::Max(Gait, MovingGait);
		}

		// speeding up to a faster gait (e.g. starting to sprint) has its start sound; slowing down is quiet
		if (Gait > MovingGait)
		{
			PlayMovementSound(EZFootstepEvent::Start, Gait);
		}

		MovingGait = Gait;
		break;
	}

	case EMoveState::Stopping:
		// the next start is after standing still; moving off again before that (a sharp turn) is covered by the stop
		LowestSpeedSinceStop = FMath::Min(LowestSpeedSinceStop, Speed);
		if (Speed < StartSpeed)
		{
			MoveState = EMoveState::Resting;
			PeakSpeed = 0.f;
		}
		else if (Speed > LowestSpeedSinceStop + StartSpeed)
		{
			MoveState = EMoveState::Moving;
			MoveStartTime = Now;
			MovingGait = Speeds.GetGait(Speed);
		}
		break;
	}
}

void UZFootstepComponent::PlayMovementSound(EZFootstepEvent Event, EZGait Gait)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastMovementSoundTime < MinTimeBetweenMovementSounds)
	{
		UE_CLOG(CVarFootstepDebug.GetValueOnGameThread(), LogZSurvivalGame, Log, TEXT("Footstep: '%s' %s left out, too soon after the last start or stop"),
			*GetNameSafe(GetOwner()), *UEnum::GetDisplayValueAsText(Event).ToString());
		return;
	}

	LastMovementSoundTime = Now;
	PlayFootstep(Event, EZFootstepFoot::Auto, Gait);
}

void UZFootstepComponent::HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	// replaying moves after a server correction changes the mode again; those aren't new jumps or landings
	if (!bPlayMovementSounds || !Character || Character->bClientUpdating)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bWasOnGround = PrevMovementMode == MOVE_Walking || PrevMovementMode == MOVE_NavWalking;

	if (Movement->IsFalling() && bWasOnGround)
	{
		FallSpeed = 0.f;

		// leaving the ground upward is a jump; walking off a ledge isn't
		if (Movement->Velocity.Z >= JumpSpeed)
		{
			PlayFootstep(EZFootstepEvent::Jump, EZFootstepFoot::Both, MovingGait);
		}
	}
	else if (Movement->IsMovingOnGround() && PrevMovementMode == MOVE_Falling)
	{
		const UZFootstepData* Data = GetFootstepData();
		const float Volume = Data ? Data->GetLandVolume(FallSpeed) : 0.f;
		if (Volume > 0.f)
		{
			PlayFootstep(EZFootstepEvent::Land, EZFootstepFoot::Both, MovingGait, Volume);
		}

		FallSpeed = 0.f;
	}
}

void UZFootstepComponent::PlayFootstep(EZFootstepEvent Event, EZFootstepFoot Foot, EZGait Gait, float VolumeScale, USkeletalMeshComponent* Mesh)
{
	const UZFootstepData* Data = GetFootstepData();
	UWorld* World = GetWorld();
	if (!Data || !World || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (Event == EZFootstepEvent::Step && World->GetTimeSeconds() - StopTime < QuietStepsAfterStop)
	{
		UE_CLOG(CVarFootstepDebug.GetValueOnGameThread(), LogZSurvivalGame, Log, TEXT("Footstep: '%s' step left out, the stop sound has it"), *GetNameSafe(GetOwner()));
		return;
	}

	PlayFootstepSound(*World, *Data, Event, Gait, GetFootLocation(Foot, Mesh), GetOwner(), VolumeScale, LastSound);
}

FVector UZFootstepComponent::GetFootLocation(EZFootstepFoot Foot, const USkeletalMeshComponent* Mesh) const
{
	if (!Mesh)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		Mesh = Character ? Character->GetMesh() : nullptr;
	}

	if (Mesh)
	{
		return FindFootLocation(*Mesh, Foot, LeftFootBone, RightFootBone);
	}

	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

FVector UZFootstepComponent::FindFootLocation(const USkeletalMeshComponent& Mesh, EZFootstepFoot Foot, FName LeftBone, FName RightBone)
{
	// without the bones (another skeleton), the mesh's origin, which is at the feet
	if (Mesh.GetBoneIndex(LeftBone) == INDEX_NONE || Mesh.GetBoneIndex(RightBone) == INDEX_NONE)
	{
		return Mesh.GetComponentLocation();
	}

	const FVector Left = Mesh.GetSocketLocation(LeftBone);
	const FVector Right = Mesh.GetSocketLocation(RightBone);

	switch (Foot)
	{
	case EZFootstepFoot::Left:
		return Left;
	case EZFootstepFoot::Right:
		return Right;
	case EZFootstepFoot::Both:
		return (Left + Right) * 0.5f;
	default:
		return Left.Z <= Right.Z ? Left : Right;
	}
}

void UZFootstepComponent::PlayFootstepSound(UWorld& World, const UZFootstepData& Data, EZFootstepEvent Event, EZGait Gait, const FVector& FootLocation,
	const AActor* Owner, float VolumeScale, const USoundBase*& InOutLastSound)
{
	// the ground under the foot; its Physical Material says the surface
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZFootstep), true, Owner);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	const bool bHitGround = World.LineTraceSingleByChannel(Hit, FootLocation + FVector(0.f, 0.f, TraceUp), FootLocation - FVector(0.f, 0.f, TraceDown), ECC_Visibility, Params);
	const EPhysicalSurface Surface = bHitGround ? UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get()) : SurfaceType_Default;

	// no ground under the foot (e.g. a step notify while in the air): nothing to hear
	USoundBase* Sound = nullptr;
	if (bHitGround)
	{
		if (const FZSurfaceFootstepSounds* Sounds = Data.FindSounds(Surface))
		{
			Sound = PickSound(Sounds->GetSounds(Event, Gait), InOutLastSound);
		}
	}

	if (Sound)
	{
		const float Volume = Data.Volume * VolumeScale * FMath::FRandRange(1.f - Data.VolumeVariation, 1.f);
		const float Pitch = FMath::FRandRange(1.f - Data.PitchVariation, 1.f + Data.PitchVariation);
		UGameplayStatics::PlaySoundAtLocation(&World, Sound, Hit.ImpactPoint, FRotator::ZeroRotator, Volume, Pitch, 0.f, Data.Attenuation, Data.Concurrency, Owner);
		InOutLastSound = Sound;
	}

#if ENABLE_DRAW_DEBUG
	if (CVarFootstepDebug.GetValueOnGameThread())
	{
		const FString EventName = Event == EZFootstepEvent::Step || Event == EZFootstepEvent::Start || Event == EZFootstepEvent::Stop
			? FString::Printf(TEXT("%s %s"), *UEnum::GetDisplayValueAsText(Gait).ToString(), *UEnum::GetDisplayValueAsText(Event).ToString())
			: UEnum::GetDisplayValueAsText(Event).ToString();
		const FString SurfaceName = bHitGround ? GetSurfaceName(Surface) : TEXT("no ground");

		UE_LOG(LogZSurvivalGame, Log, TEXT("Footstep: '%s' %s on %s at %.0f cm/s -> %s"), *GetNameSafe(Owner), *EventName, *SurfaceName,
			Owner ? Owner->GetVelocity().Size2D() : 0.0, Sound ? *Sound->GetName() : TEXT("(no sound)"));

		const FVector DebugLocation = bHitGround ? FVector(Hit.ImpactPoint) : FootLocation;
		DrawDebugSphere(&World, DebugLocation, 8.f, 8, GetDebugColor(Event), false, 1.5f);
		DrawDebugString(&World, DebugLocation + FVector(0.f, 0.f, 15.f), FString::Printf(TEXT("%s\n%s"), *EventName, *SurfaceName), nullptr, GetDebugColor(Event), 1.5f);
	}
#endif
}

USoundBase* UZFootstepComponent::PickSound(const TArray<TObjectPtr<USoundBase>>& Sounds, const USoundBase* LastSound)
{
	if (Sounds.IsEmpty())
	{
		return nullptr;
	}

	int32 Index = FMath::RandRange(0, Sounds.Num() - 1);
	if (Sounds.Num() > 1 && Sounds[Index] == LastSound)
	{
		Index = (Index + FMath::RandRange(1, Sounds.Num() - 1)) % Sounds.Num();
	}

	return Sounds[Index];
}

FString UZFootstepComponent::GetSurfaceName(EPhysicalSurface Surface)
{
	if (Surface != SurfaceType_Default)
	{
		for (const FPhysicalSurfaceName& Entry : UPhysicsSettings::Get()->PhysicalSurfaces)
		{
			if (Entry.Type == Surface)
			{
				return Entry.Name.ToString();
			}
		}
	}

	return Surface == SurfaceType_Default ? TEXT("Default") : FString::Printf(TEXT("SurfaceType%d"), static_cast<int32>(Surface));
}

bool UZFootstepComponent::FindSurfaceByName(const FString& Name, EPhysicalSurface& OutSurface)
{
	if (Name.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
	{
		OutSurface = SurfaceType_Default;
		return true;
	}

	for (const FPhysicalSurfaceName& Entry : UPhysicsSettings::Get()->PhysicalSurfaces)
	{
		if (Entry.Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
		{
			OutSurface = Entry.Type;
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
void UZFootstepComponent::PlayPreviewFootstep(USkeletalMeshComponent* Mesh, EZFootstepEvent Event, EZGait Gait)
{
	UWorld* World = Mesh ? Mesh->GetWorld() : nullptr;
	const UZFootstepData* Data = UZFootstepSettings::Get()->DefaultFootstepData.LoadSynchronous();
	const FZSurfaceFootstepSounds* Sounds = Data ? Data->FindSounds(SurfaceType_Default) : nullptr;
	if (!World || !Sounds)
	{
		return;
	}

	// heard from anywhere around the preview, like Play Sound notifies with Preview Ignore Attenuation
	static const USoundBase* LastPreviewSound = nullptr;
	if (USoundBase* Sound = PickSound(Sounds->GetSounds(Event, Gait), LastPreviewSound))
	{
		UGameplayStatics::PlaySound2D(World, Sound, Data->Volume);
		LastPreviewSound = Sound;
	}
}
#endif
