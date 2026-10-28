// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZAnimNotifyState_ActionWindow.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/ZGameplayTags.h"

bool UZAnimNotifyState_ActionWindow::IsWindowOpen(const UAnimMontage* Montage, float Position, const FGameplayTag& InWindow)
{
	if (!Montage || !InWindow.IsValid())
	{
		return false;
	}

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UZAnimNotifyState_ActionWindow* WindowNotify = Cast<UZAnimNotifyState_ActionWindow>(Event.NotifyStateClass);
		if (WindowNotify && WindowNotify->Window == InWindow && Position >= Event.GetTriggerTime() && Position <= Event.GetEndTriggerTime())
		{
			return true;
		}
	}

	return false;
}

bool UZAnimNotifyState_ActionWindow::HasWindow(const UAnimMontage* Montage, const FGameplayTag& InWindow)
{
	if (!Montage || !InWindow.IsValid())
	{
		return false;
	}

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UZAnimNotifyState_ActionWindow* WindowNotify = Cast<UZAnimNotifyState_ActionWindow>(Event.NotifyStateClass);
		if (WindowNotify && WindowNotify->Window == InWindow)
		{
			return true;
		}
	}

	return false;
}

FString UZAnimNotifyState_ActionWindow::GetNotifyName_Implementation() const
{
	// "Combo Window" for Window.Combo
	FString Name = Window.ToString();
	int32 LastDot = INDEX_NONE;
	if (Name.FindLastChar(TEXT('.'), LastDot))
	{
		Name.RightChopInline(LastDot + 1);
	}

	return Name.IsEmpty() ? TEXT("Action Window") : FName::NameToDisplayString(Name, false) + TEXT(" Window");
}

#if WITH_EDITOR
bool UZAnimNotifyState_ActionWindow::CanBePlaced(UAnimSequenceBase* Animation) const
{
	// the abilities read windows from the montage they play
	return Animation && Animation->IsA<UAnimMontage>();
}
#endif

UZAnimNotifyState_ComboWindow::UZAnimNotifyState_ComboWindow()
{
	Window = ZGameplayTags::Window_Combo;
}

UZAnimNotifyState_DodgeWindow::UZAnimNotifyState_DodgeWindow()
{
	Window = ZGameplayTags::Window_Dodge;
}

UZAnimNotifyState_TurnWindow::UZAnimNotifyState_TurnWindow()
{
	Window = ZGameplayTags::Window_Turn;
}
