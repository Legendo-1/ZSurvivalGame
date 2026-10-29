// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZMeleeTraceComponent.generated.h"

class UAnimInstance;
class USceneComponent;
enum class EVisibilityBasedAnimTickOption : uint8;
class USkeletalMeshComponent;
class UZAnimNotifyState_WeaponHitbox;
struct FZMeleeBlade;

DECLARE_MULTICAST_DELEGATE_OneParam(FZOnMeleeHit, const FHitResult& /*Hit*/);

/**
 *  Finds what a melee attack hits. While an attack's Weapon Hitbox notify is active, it sweeps the weapon's blade (or a ball at the hand) along the swing:
 *  a player's held weapon, or a weapon of the character's own (IZCombatantInterface::GetMeleeBlade, e.g. an enemy's hammer)
 *  every frame, and reports each character it touches once per hitbox (characters' meshes, through each other; walls don't stop it)
 *  A swing cut short (its montage blending out under the next action) stops hitting at once. While a hitbox is active the character's bones keep
 *  moving even off screen (the engine only updates them when the character is drawn), so the server finds AI hits too
 *  It only looks on the machine where hits are found first: a player's own machine (they see their swing without delay), or the server for AI.
 *  The attack ability turns the hits into damage: a player's machine sends them to the server, which checks them first (see COMBAT.md)
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZMeleeTraceComponent();

	/** A Weapon Hitbox notify started (MontageInstanceID tells two plays of the same montage apart) */
	void BeginHitbox(const UZAnimNotifyState_WeaponHitbox* Notify, int32 MontageInstanceID, USkeletalMeshComponent* CharacterMesh);

	/** A Weapon Hitbox notify ended */
	void EndHitbox(const UZAnimNotifyState_WeaponHitbox* Notify, int32 MontageInstanceID);

	/** Stops every hitbox, e.g. when the attack is cut short */
	void EndAllHitboxes();

	/** True while a hitbox looks for hits */
	bool IsHitboxActive() const { return !ActiveHitboxes.IsEmpty(); }

	/** True on the machine that finds this character's hits: its player's own machine, or the server for AI */
	bool ShouldFindHits() const;

	/** Something was hit (a character, the first time this hitbox touched it). Its ImpactPoint is where, its BoneName which bone */
	FZOnMeleeHit OnHit;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** One ball swept along the swing: at a spot, or part way (Alpha) along a line between two spots (the blade).
	 *  A spot is a socket of the component (None: the component itself), moved by an offset in its space */
	struct FHitboxPoint
	{
		TWeakObjectPtr<USceneComponent> Component;
		FName StartSocket;
		FVector StartOffset = FVector::ZeroVector;
		FName EndSocket;
		FVector EndOffset = FVector::ZeroVector;
		bool bAlongLine = false;
		float Alpha = 0.f;
		float Radius = 10.f;
	};

	/** Balls along a blade, close enough together that nothing slips between them */
	static void AddBladePoints(const FZMeleeBlade& Blade, TArray<FHitboxPoint>& OutPoints);

	struct FActiveHitbox
	{
		TWeakObjectPtr<const UZAnimNotifyState_WeaponHitbox> Notify;
		TWeakObjectPtr<UAnimInstance> AnimInstance;
		int32 MontageInstanceID = INDEX_NONE;
		TArray<FHitboxPoint> Points;
		TArray<FVector> LastLocations;

		/** False until LastLocations hold where the balls were (the bones of a character off screen only move from the next frame) */
		bool bHasStartLocations = false;
		TSet<TWeakObjectPtr<AActor>> HitActors;
	};

	/** The balls for a hitbox: along the weapon's blade (or at the hand), and at the notify's extra sockets */
	void BuildPoints(const UZAnimNotifyState_WeaponHitbox* Notify, USkeletalMeshComponent* CharacterMesh, TArray<FHitboxPoint>& OutPoints) const;

	static FVector GetPointLocation(const FHitboxPoint& Point);

	/** True while the hitbox's montage still plays (not stopped or blending out under another one) */
	static bool IsSwingActive(const FActiveHitbox& Hitbox);

	/** Sweeps every ball of this hitbox from where it was to where it is now, and reports what's new */
	void SweepHitbox(int32 HitboxIndex);

	TArray<FActiveHitbox> ActiveHitboxes;

	/** Goes up whenever hitboxes start or end, so a sweep notices when a listener changed them (e.g. ended the attack) */
	uint32 ChangeCount = 0;

	/** Keeps the character mesh's bones moving while hitboxes are active, also off screen */
	void KeepBonesUpdated(USkeletalMeshComponent* CharacterMesh);

	/** No hitbox active any more: the mesh updates its bones as before, and this stops ticking */
	void OnAllHitboxesEnded();

	/** The mesh whose bones are kept moving, and how it updated them before */
	TWeakObjectPtr<USkeletalMeshComponent> UpdatedMesh;
	EVisibilityBasedAnimTickOption SavedTickOption{};
};
