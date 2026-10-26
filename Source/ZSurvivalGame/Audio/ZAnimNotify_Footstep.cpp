// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/ZAnimNotify_Footstep.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UZAnimNotify_Footstep::UZAnimNotify_Footstep()
{
#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(196, 142, 64);
#endif
}

FString UZAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	FString Name = TEXT("Footstep");

	if (Foot == EZFootstepFoot::Left)
	{
		Name += TEXT(" L");
	}
	else if (Foot == EZFootstepFoot::Right)
	{
		Name += TEXT(" R");
	}

	if (Event != EZFootstepEvent::Step)
	{
		Name += FString::Printf(TEXT(" (%s)"), *UEnum::GetDisplayValueAsText(Event).ToString());
	}

	return Name;
}

void UZAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZFootstepComponent* Footsteps = Owner ? Owner->FindComponentByClass<UZFootstepComponent>() : nullptr)
	{
		Footsteps->PlayFootstep(Event, Foot, bGaitFromMovement ? Footsteps->GetCurrentGait() : Gait, 1.f, MeshComp);
		return;
	}

#if WITH_EDITOR
	// the animation editor has no character: play the project's default footsteps, so the timing can be heard
	const UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (World && World->WorldType == EWorldType::EditorPreview)
	{
		UZFootstepComponent::PlayPreviewFootstep(MeshComp, Event, Gait);
	}
#endif
}
