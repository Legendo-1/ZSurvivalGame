// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ZAnimNotifyState_WeaponHitbox.generated.h"

class UAnimMontage;

/**
 *  Where an attack hits: while it's active, the character's melee weapon finds what it touches along its swing (the blade between the weapon's
 *  Blade Start and Blade End sockets, or a ball at the hand when the weapon has none), each character once. Put it on the frames of the swing (see COMBAT.md)
 *  The finding is done by the character's UZMeleeTraceComponent; the attack ability turns the hits into damage
 */
UCLASS(meta = (DisplayName = "Weapon Hitbox"))
class UZAnimNotifyState_WeaponHitbox : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	UZAnimNotifyState_WeaponHitbox();

	/** Hits with the weapon in the hands (its blade, or the ball at the hand if it has no blade sockets) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	bool bUseWeapon = true;

	/** Also hits with these bones or sockets of the character's mesh, e.g. foot_r for a kick or hand_l for a punch with the other hand */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox")
	TArray<FName> ExtraSockets;

	/** Size of the balls around Extra Sockets, in cm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitbox", meta = (ClampMin = "1", Units = "cm"))
	float ExtraSocketRadius = 25.f;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif

	/** How many Weapon Hitbox notifies the montage has (only those in that section, if one is given): each can hit a character once */
	static int32 CountHitboxes(const UAnimMontage* Montage, FName Section = NAME_None);

private:

	/** Tells hitboxes of the same notify apart when the same montage plays twice at once (one blending out, one starting) */
	static int32 GetMontageInstanceID(const FAnimNotifyEventReference& EventReference);
};
