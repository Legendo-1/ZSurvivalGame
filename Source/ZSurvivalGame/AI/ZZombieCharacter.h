// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AI/ZAICharacter.h"
#include "ZZombieCharacter.generated.h"

/**
 *  Basic zombie: chases the closest player and hits them up close
 *  Uses the Quinn mannequin, the combat template's animations and its punch until zombie models exist;
 *  make a Blueprint child to change the looks or numbers
 */
UCLASS()
class AZZombieCharacter : public AZAICharacter
{
	GENERATED_BODY()

public:

	AZZombieCharacter();
};
