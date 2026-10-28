// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZMeleeTraceComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "Combat/ZAnimNotifyState_WeaponHitbox.h"
#include "Weapons/ZMeleeWeaponData.h"
#include "Weapons/ZWeapon.h"

namespace
{
	TAutoConsoleVariable<bool> CVarDrawMeleeDebug(
		TEXT("Z.Melee.DrawDebug"),
		false,
		TEXT("Draws melee hitboxes as they sweep (green), and the hits (red), on the machine that finds the hits. 1 = on"));

	/** Without melee weapon data (e.g. a gun swung by its holder): a ball at the right hand */
	const FName DefaultFallbackSocket(TEXT("hand_r"));
	constexpr float DefaultFallbackRadius = 30.f;

	/** At most this many balls along a blade */
	constexpr int32 MaxBladePoints = 12;
}

UZMeleeTraceComponent::UZMeleeTraceComponent()
{
	// after the animation has moved the bones this frame, and only while a hitbox is active
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UZMeleeTraceComponent::ShouldFindHits() const
{
	// a player's own machine finds their hits (the host's too); the server finds the hits of AI characters
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return false;
	}

	return Pawn->IsLocallyControlled() || (Pawn->HasAuthority() && !Pawn->IsPlayerControlled());
}

void UZMeleeTraceComponent::BeginHitbox(const UZAnimNotifyState_WeaponHitbox* Notify, int32 MontageInstanceID, USkeletalMeshComponent* CharacterMesh)
{
	if (!Notify || !CharacterMesh || !ShouldFindHits())
	{
		return;
	}

	// the same hitbox again (e.g. the montage restarted): start it over
	EndHitbox(Notify, MontageInstanceID);

	FActiveHitbox NewHitbox;
	NewHitbox.Notify = Notify;
	NewHitbox.AnimInstance = CharacterMesh->GetAnimInstance();
	NewHitbox.MontageInstanceID = MontageInstanceID;

	// a swing that was cut short (its montage blends out under the next attack or a dodge) doesn't hit any more
	if (!IsSwingActive(NewHitbox))
	{
		return;
	}

	// off screen (e.g. the server for AI) the bones only move while something needs them to
	const bool bBonesUpToDate = CharacterMesh->bRecentlyRendered || CharacterMesh->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	KeepBonesUpdated(CharacterMesh);

	BuildPoints(Notify, CharacterMesh, NewHitbox.Points);

	// the first sweep goes from here, so a hit right where the swing starts counts too
	// (bones that weren't moving are still in an old pose: then the first sweep starts from where they are next frame)
	if (bBonesUpToDate)
	{
		for (const FHitboxPoint& Point : NewHitbox.Points)
		{
			NewHitbox.LastLocations.Add(GetPointLocation(Point));
		}

		NewHitbox.bHasStartLocations = true;
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' hitbox started: %d ball(s)%s"), *GetNameSafe(GetOwner()), NewHitbox.Points.Num(), bBonesUpToDate ? TEXT("") : TEXT(" (bones start moving next frame)"));

	ActiveHitboxes.Add(MoveTemp(NewHitbox));
	++ChangeCount;
	SetComponentTickEnabled(true);
}

void UZMeleeTraceComponent::EndHitbox(const UZAnimNotifyState_WeaponHitbox* Notify, int32 MontageInstanceID)
{
	const int32 Removed = ActiveHitboxes.RemoveAll([Notify, MontageInstanceID](const FActiveHitbox& Hitbox)
	{
		return Hitbox.Notify.Get() == Notify && Hitbox.MontageInstanceID == MontageInstanceID;
	});

	if (Removed > 0)
	{
		++ChangeCount;
	}

	if (ActiveHitboxes.IsEmpty())
	{
		OnAllHitboxesEnded();
	}
}

void UZMeleeTraceComponent::EndAllHitboxes()
{
	if (!ActiveHitboxes.IsEmpty())
	{
		ActiveHitboxes.Reset();
		++ChangeCount;
	}

	OnAllHitboxesEnded();
}

void UZMeleeTraceComponent::KeepBonesUpdated(USkeletalMeshComponent* CharacterMesh)
{
	if (UpdatedMesh.Get() == CharacterMesh)
	{
		return;
	}

	OnAllHitboxesEnded();
	UpdatedMesh = CharacterMesh;
	SavedTickOption = CharacterMesh->VisibilityBasedAnimTickOption;
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

void UZMeleeTraceComponent::OnAllHitboxesEnded()
{
	if (USkeletalMeshComponent* Mesh = UpdatedMesh.Get())
	{
		Mesh->VisibilityBasedAnimTickOption = SavedTickOption;
	}

	UpdatedMesh = nullptr;
	SetComponentTickEnabled(false);
}

void UZMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// swings cut short stop hitting
	if (ActiveHitboxes.RemoveAll([](const FActiveHitbox& Hitbox) { return !IsSwingActive(Hitbox); }) > 0)
	{
		++ChangeCount;
	}

	for (int32 Index = 0; Index < ActiveHitboxes.Num(); ++Index)
	{
		// a hit's listener may end the attack (and its hitboxes) right away; the others wait for the next frame
		const uint32 ChangeCountBefore = ChangeCount;
		SweepHitbox(Index);
		if (ChangeCount != ChangeCountBefore)
		{
			break;
		}
	}

	if (ActiveHitboxes.IsEmpty())
	{
		OnAllHitboxesEnded();
	}
}

void UZMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndAllHitboxes();

	Super::EndPlay(EndPlayReason);
}

void UZMeleeTraceComponent::BuildPoints(const UZAnimNotifyState_WeaponHitbox* Notify, USkeletalMeshComponent* CharacterMesh, TArray<FHitboxPoint>& OutPoints) const
{
	if (Notify->bUseWeapon)
	{
		const IZCombatantInterface* Combatant = Cast<IZCombatantInterface>(GetOwner());
		const AZWeapon* Weapon = Combatant ? Combatant->GetHeldWeapon() : nullptr;
		const UZMeleeWeaponData* MeleeData = Weapon ? Weapon->GetMeleeData() : nullptr;
		UMeshComponent* WeaponMesh = Weapon ? Weapon->GetWeaponMesh() : nullptr;

		if (MeleeData && WeaponMesh && WeaponMesh->DoesSocketExist(MeleeData->BladeStartSocket) && WeaponMesh->DoesSocketExist(MeleeData->BladeEndSocket))
		{
			// balls along the blade, close enough together that nothing slips between them
			const float BladeLength = FVector::Dist(WeaponMesh->GetSocketLocation(MeleeData->BladeStartSocket), WeaponMesh->GetSocketLocation(MeleeData->BladeEndSocket));
			const int32 NumPoints = FMath::Clamp(FMath::CeilToInt32(BladeLength / (MeleeData->BladeRadius * 1.5f)) + 1, 2, MaxBladePoints);
			for (int32 Index = 0; Index < NumPoints; ++Index)
			{
				FHitboxPoint& Point = OutPoints.AddDefaulted_GetRef();
				Point.Component = WeaponMesh;
				Point.SocketA = MeleeData->BladeStartSocket;
				Point.SocketB = MeleeData->BladeEndSocket;
				Point.Alpha = static_cast<float>(Index) / (NumPoints - 1);
				Point.Radius = MeleeData->BladeRadius;
			}
		}
		else
		{
			// no blade to follow (no mesh yet, or it has no blade sockets): a ball at the hand
			FHitboxPoint& Point = OutPoints.AddDefaulted_GetRef();
			Point.Component = CharacterMesh;
			Point.SocketA = MeleeData ? MeleeData->FallbackHitboxSocket : DefaultFallbackSocket;
			Point.Radius = MeleeData ? MeleeData->FallbackHitboxRadius : DefaultFallbackRadius;
		}
	}

	for (const FName& Socket : Notify->ExtraSockets)
	{
		FHitboxPoint& Point = OutPoints.AddDefaulted_GetRef();
		Point.Component = CharacterMesh;
		Point.SocketA = Socket;
		Point.Radius = Notify->ExtraSocketRadius;
	}
}

bool UZMeleeTraceComponent::IsSwingActive(const FActiveHitbox& Hitbox)
{
	UAnimInstance* AnimInstance = Hitbox.AnimInstance.Get();
	if (!AnimInstance)
	{
		return false;
	}

	// not from a montage: it counts as long as the notify lasts
	if (Hitbox.MontageInstanceID == INDEX_NONE)
	{
		return true;
	}

	const FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(Hitbox.MontageInstanceID);
	return MontageInstance && MontageInstance->IsActive();
}

FVector UZMeleeTraceComponent::GetPointLocation(const FHitboxPoint& Point)
{
	const USceneComponent* Component = Point.Component.Get();
	if (!Component)
	{
		return FVector::ZeroVector;
	}

	const FVector LocationA = Component->GetSocketLocation(Point.SocketA);
	return Point.SocketB.IsNone() ? LocationA : FMath::Lerp(LocationA, Component->GetSocketLocation(Point.SocketB), Point.Alpha);
}

void UZMeleeTraceComponent::SweepHitbox(int32 HitboxIndex)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || !ActiveHitboxes.IsValidIndex(HitboxIndex))
	{
		return;
	}

	FActiveHitbox& Hitbox = ActiveHitboxes[HitboxIndex];

	// the first frame where the bones are where the swing really is: just remember where the balls are
	if (!Hitbox.bHasStartLocations)
	{
		Hitbox.LastLocations.Reset(Hitbox.Points.Num());
		for (const FHitboxPoint& Point : Hitbox.Points)
		{
			Hitbox.LastLocations.Add(GetPointLocation(Point));
		}

		Hitbox.bHasStartLocations = true;
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZMeleeTrace), false, Owner);
	QueryParams.bReturnPhysicalMaterial = true;
	if (const IZCombatantInterface* Combatant = Cast<IZCombatantInterface>(Owner))
	{
		QueryParams.AddIgnoredActor(Combatant->GetHeldWeapon());
	}

	// characters' meshes block the weapon channel (their capsules don't); only those count, all of them along the swing, and walls don't stop it
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetAllChannels(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Overlap);

	const bool bDrawDebug = CVarDrawMeleeDebug.GetValueOnGameThread();

	// every ball first, then the listeners: one of them may end the attack (and this hitbox with it)
	TArray<FHitResult> NewHits;
	for (int32 Index = 0; Index < Hitbox.Points.Num(); ++Index)
	{
		const FHitboxPoint& Point = Hitbox.Points[Index];
		const FVector Start = Hitbox.LastLocations[Index];
		const FVector End = GetPointLocation(Point);
		Hitbox.LastLocations[Index] = End;

		TArray<FHitResult> Hits;
		World->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ZTraceChannel_Weapon, FCollisionShape::MakeSphere(Point.Radius), QueryParams, ResponseParams);

#if ENABLE_DRAW_DEBUG
		if (bDrawDebug)
		{
			DrawDebugCapsule(World, (Start + End) * 0.5f, FVector::Dist(Start, End) * 0.5f + Point.Radius, Point.Radius,
				FRotationMatrix::MakeFromZ(End - Start).ToQuat(), FColor::Green, false, 1.f);
		}
#endif

		for (FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor || HitActor == Owner || Hitbox.HitActors.Contains(HitActor) || !UZCombatLibrary::CanDamage(Owner, HitActor) || UZCombatLibrary::IsDead(HitActor))
			{
				continue;
			}

			Hitbox.HitActors.Add(HitActor);

			// a ball that starts inside the body has no impact point of its own: use the body's surface nearest to it
			if (Hit.bStartPenetrating && Hit.GetComponent())
			{
				FVector ClosestPoint;
				if (Hit.GetComponent()->GetClosestPointOnCollision(End, ClosestPoint, Hit.BoneName) > 0.f)
				{
					Hit.ImpactPoint = ClosestPoint;
				}
				else
				{
					Hit.ImpactPoint = End;
				}
			}

			// for the damage and the effects: where it hit, facing back along the swing
			Hit.bBlockingHit = true;
			Hit.TraceStart = Start;
			Hit.TraceEnd = End;
			if (Hit.ImpactNormal.IsNearlyZero())
			{
				Hit.ImpactNormal = (Start - End).GetSafeNormal();
			}

			UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' melee hitbox touched '%s' (bone %s)"), *GetNameSafe(Owner), *GetNameSafe(HitActor), *Hit.BoneName.ToString());

#if ENABLE_DRAW_DEBUG
			if (bDrawDebug)
			{
				DrawDebugPoint(World, Hit.ImpactPoint, 12.f, FColor::Red, false, 2.f);
			}
#endif

			NewHits.Add(Hit);
		}
	}

	const uint32 ChangeCountBefore = ChangeCount;
	for (const FHitResult& Hit : NewHits)
	{
		OnHit.Broadcast(Hit);

		// the attack ended (or another hitbox started or ended): the rest of this sweep doesn't count
		if (ChangeCount != ChangeCountBefore)
		{
			return;
		}
	}
}
