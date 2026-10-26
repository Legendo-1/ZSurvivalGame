// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZSurvivalGamePlayerState.h"
#include "AbilitySystem/ZAbilitySystemComponent.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "Crafting/ZCraftingComponent.h"
#include "Items/ZInventoryComponent.h"
#include "Progression/ZProgressionComponent.h"

AZSurvivalGamePlayerState::AZSurvivalGamePlayerState()
{
	// Mixed: the owning player gets full gameplay effect info (needed for prediction), everyone else only tags and cues
	AbilitySystemComponent = CreateDefaultSubobject<UZAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// created as a subobject of the ability system's owner, so the ability system finds and replicates it
	AttributeSet = CreateDefaultSubobject<UZAttributeSet>(TEXT("AttributeSet"));

	Inventory = CreateDefaultSubobject<UZInventoryComponent>(TEXT("Inventory"));
	Crafting = CreateDefaultSubobject<UZCraftingComponent>(TEXT("Crafting"));
	Progression = CreateDefaultSubobject<UZProgressionComponent>(TEXT("Progression"));

	// the PlayerState default (1 update per second) is too slow for abilities and attributes
	SetNetUpdateFrequency(100.f);
}

UAbilitySystemComponent* AZSurvivalGamePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}
