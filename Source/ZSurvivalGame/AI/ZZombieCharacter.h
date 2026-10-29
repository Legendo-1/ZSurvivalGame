// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/ZAIBaseCharacter.h"
#include "ZZombieCharacter.generated.h"

/**
 *  Basic zombie: goes for the player it sees (or smells close by, or who hurt it most) and hits them up close; never circles
 *  Uses the Quinn mannequin, the combat template's animations and its punch until zombie models exist;
 *  make a Blueprint child to change the looks or numbers
 */
UCLASS()
class AZZombieCharacter : public AZAIBaseCharacter
{
	GENERATED_BODY()

public:

	AZZombieCharacter();
};
