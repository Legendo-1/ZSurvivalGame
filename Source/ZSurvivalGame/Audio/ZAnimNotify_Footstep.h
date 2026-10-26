// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Audio/ZFootstepComponent.h"
#include "ZAnimNotify_Footstep.generated.h"

/**
 *  A footstep in an animation: place it where a foot touches the ground. The character's Footsteps component plays the sound
 *  for the surface under that foot and the character's gait (walk, jog, sprint); see AUDIO.md
 */
UCLASS(meta = (DisplayName = "Footstep"))
class UZAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:

	UZAnimNotify_Footstep();

	/** What the sound is for: a step (almost always), or a start, stop, jump or landing for an animation that shows one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	EZFootstepEvent Event = EZFootstepEvent::Step;

	/** The foot touching the ground; Auto takes the lower one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	EZFootstepFoot Foot = EZFootstepFoot::Auto;

	/** Walk, jog or sprint sounds from how fast the character moves (almost always). Off: always the Gait below */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	bool bGaitFromMovement = true;

	/** The gait's sounds when Gait From Movement is off, and in the animation editor's preview */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	EZGait Gait = EZGait::Jog;

	/** The name on the timeline, e.g. "Footstep L" */
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
