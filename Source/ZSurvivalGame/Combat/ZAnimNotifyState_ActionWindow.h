// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ZAnimNotifyState_ActionWindow.generated.h"

class UAnimMontage;

/**
 *  Marks part of an action's animation (an attack, a dodge) where another action may cut it short, e.g. the next attack of a combo
 *  Only a marker: the abilities look at the montage the character plays to see whether it's inside a window (see COMBAT.md)
 *  Use Combo Window or Dodge Window; a new kind of window is a new Window tag (Window.*) that an ability names as its Interrupt Window
 */
UCLASS(Abstract, meta = (DisplayName = "Action Window"))
class UZAnimNotifyState_ActionWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:

	/** Which kind of window this is, e.g. Window.Combo */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window", meta = (Categories = "Window"))
	FGameplayTag Window;

	/** True if the montage at this position (its own time, in seconds) is inside a window of this kind */
	static bool IsWindowOpen(const UAnimMontage* Montage, float Position, const FGameplayTag& InWindow);

	/** True if the montage has a window of this kind anywhere */
	static bool HasWindow(const UAnimMontage* Montage, const FGameplayTag& InWindow);

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
};

/**
 *  The next attack of a combo (or a shot) may start here, cutting this attack short. An attack pressed earlier waits for it (input buffer)
 *  Without one, the next attack only starts once this one is over
 */
UCLASS(meta = (DisplayName = "Combo Window"))
class UZAnimNotifyState_ComboWindow : public UZAnimNotifyState_ActionWindow
{
	GENERATED_BODY()

public:

	UZAnimNotifyState_ComboWindow();
};

/**
 *  A dodge may start here, cutting this action short (usually the recovery at the end of an attack). A dodge pressed earlier waits for it
 *  Without one, a dodge only starts once the action is over
 */
UCLASS(meta = (DisplayName = "Dodge Window"))
class UZAnimNotifyState_DodgeWindow : public UZAnimNotifyState_ActionWindow
{
	GENERATED_BODY()

public:

	UZAnimNotifyState_DodgeWindow();
};

/**
 *  The attack still turns toward the camera here, quickly (usually the windup, before the hit). After it the attack keeps its direction until its Dodge Window
 *  Without one, the attack turns during the first Attack Turn Time of the montage (Project Settings -> Game -> Combat)
 */
UCLASS(meta = (DisplayName = "Turn Window"))
class UZAnimNotifyState_TurnWindow : public UZAnimNotifyState_ActionWindow
{
	GENERATED_BODY()

public:

	UZAnimNotifyState_TurnWindow();
};
