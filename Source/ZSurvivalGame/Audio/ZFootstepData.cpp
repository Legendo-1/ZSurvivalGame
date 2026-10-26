// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/ZFootstepData.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZFootsteps"

const TArray<TObjectPtr<USoundBase>>& FZSurfaceFootstepSounds::GetSounds(EZFootstepEvent Event, EZGait Gait) const
{
	const FZGaitFootstepSounds& GaitSounds = Gait == EZGait::Walk ? Walk : (Gait == EZGait::Sprint ? Sprint : Jog);

	switch (Event)
	{
	case EZFootstepEvent::Start:
		return GaitSounds.Start;
	case EZFootstepEvent::Stop:
		return GaitSounds.Stop;
	case EZFootstepEvent::Jump:
		return Jump;
	case EZFootstepEvent::Land:
		return Land;
	default:
		return GaitSounds.Steps;
	}
}

const FZSurfaceFootstepSounds* UZFootstepData::FindSounds(EPhysicalSurface Surface) const
{
	if (const FZSurfaceFootstepSounds* Sounds = Surfaces.Find(Surface))
	{
		return Sounds;
	}

	return Surfaces.Find(FallbackSurface);
}

float UZFootstepData::GetLandVolume(float FallSpeed) const
{
	if (FallSpeed < MinLandSpeed)
	{
		return 0.f;
	}

	// half volume at the slowest landing that makes a sound, full volume from FullLandSpeed on
	const float Alpha = FullLandSpeed > MinLandSpeed ? FMath::Clamp((FallSpeed - MinLandSpeed) / (FullLandSpeed - MinLandSpeed), 0.f, 1.f) : 1.f;
	return FMath::Lerp(0.5f, 1.f, Alpha);
}

#if WITH_EDITOR
EDataValidationResult UZFootstepData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!Surfaces.Contains(FallbackSurface))
	{
		Context.AddError(LOCTEXT("FootstepNoFallback", "The Fallback Surface has no sounds in Surfaces, so floors without a surface are silent."));
	}

	if (!Attenuation)
	{
		Context.AddWarning(LOCTEXT("FootstepNoAttenuation", "There's no Attenuation, so every player hears every footstep at full volume, anywhere on the map."));
	}

	for (const TPair<TEnumAsByte<EPhysicalSurface>, FZSurfaceFootstepSounds>& Entry : Surfaces)
	{
		if (Entry.Value.Jog.Steps.IsEmpty())
		{
			Context.AddWarning(FText::Format(LOCTEXT("FootstepNoJogSteps", "The surface {0} has no Jog steps."), StaticEnum<EPhysicalSurface>()->GetDisplayNameTextByValue(Entry.Key.GetValue())));
		}
	}

	if (Context.GetNumErrors() > 0)
	{
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
