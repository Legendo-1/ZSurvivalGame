// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZAnimNotifyState_WeaponHitbox.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/ZMeleeTraceComponent.h"

UZAnimNotifyState_WeaponHitbox::UZAnimNotifyState_WeaponHitbox()
{
#if WITH_EDITORONLY_DATA
	// red, like danger, in the notify track
	NotifyColor = FColor(220, 60, 50);
#endif
}

void UZAnimNotifyState_WeaponHitbox::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// in the animation editor's preview there's no character, so nothing happens
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZMeleeTraceComponent* Tracer = Owner ? Owner->FindComponentByClass<UZMeleeTraceComponent>() : nullptr)
	{
		Tracer->BeginHitbox(this, GetMontageInstanceID(EventReference), MeshComp);
	}
}

void UZAnimNotifyState_WeaponHitbox::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UZMeleeTraceComponent* Tracer = Owner ? Owner->FindComponentByClass<UZMeleeTraceComponent>() : nullptr)
	{
		Tracer->EndHitbox(this, GetMontageInstanceID(EventReference));
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UZAnimNotifyState_WeaponHitbox::GetNotifyName_Implementation() const
{
	return TEXT("Weapon Hitbox");
}

#if WITH_EDITOR
bool UZAnimNotifyState_WeaponHitbox::CanBePlaced(UAnimSequenceBase* Animation) const
{
	// attacks are montages; the server counts the hitboxes in them
	return Animation && Animation->IsA<UAnimMontage>();
}
#endif

int32 UZAnimNotifyState_WeaponHitbox::CountHitboxes(const UAnimMontage* Montage, FName Section)
{
	if (!Montage)
	{
		return 0;
	}

	// a section: only the hitboxes that start inside it
	float SectionStart = 0.f;
	float SectionEnd = Montage->GetPlayLength();
	const int32 SectionIndex = Section.IsNone() ? INDEX_NONE : Montage->GetSectionIndex(Section);
	if (SectionIndex != INDEX_NONE)
	{
		Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	}

	int32 Count = 0;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const float Start = Event.GetTriggerTime();
		if (Cast<UZAnimNotifyState_WeaponHitbox>(Event.NotifyStateClass) && Start >= SectionStart && Start < SectionEnd)
		{
			++Count;
		}
	}

	return Count;
}

int32 UZAnimNotifyState_WeaponHitbox::GetMontageInstanceID(const FAnimNotifyEventReference& EventReference)
{
	const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	return MontageContext ? MontageContext->MontageInstanceID : INDEX_NONE;
}
