// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/ZInteractable.h"
#include "ZCraftingStation.generated.h"

class UStaticMeshComponent;

/**
 *  A place to craft recipes that need a station, e.g. a workbench in the hub. Standing close enough is all it takes;
 *  interacting with it opens the inventory screen with crafting
 *  Make a Blueprint child per kind of station (e.g. BP_Workbench) and set its Station Tags and mesh there
 */
UCLASS()
class AZCraftingStation : public AActor, public IZInteractable
{
	GENERATED_BODY()

	/** What the station looks like; it also blocks the Interaction trace, so players can use it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

public:

	AZCraftingStation();

	/** True if it is this kind of station (or a more specific one) */
	UFUNCTION(BlueprintPure, Category = "Crafting Station")
	bool ProvidesStation(FGameplayTag StationTag) const;

	/** True if the actor is close enough to craft here; ExtraRange gives some slack (the server uses it for players who just moved) */
	UFUNCTION(BlueprintPure, Category = "Crafting Station")
	bool IsInCraftingRange(const AActor* Actor, float ExtraRange = 0.f) const;

	/** Name for the UI: Display Name, or one made from the first station tag */
	UFUNCTION(BlueprintPure, Category = "Crafting Station")
	FText GetStationName() const;

	UFUNCTION(BlueprintPure, Category = "Crafting Station")
	const FGameplayTagContainer& GetStationTags() const { return StationTags; }

	/** A readable name for a station tag, e.g. "Workbench" for Crafting.Station.Workbench */
	static FText GetStationNameFromTag(FGameplayTag StationTag);

	/** IZInteractable */
	virtual FText GetInteractionText_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:

	/** What kind of station it is, e.g. Crafting.Station.Workbench; recipes that need one of these can be crafted nearby */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting Station", meta = (Categories = "Crafting.Station"))
	FGameplayTagContainer StationTags;

	/** Name in the UI, e.g. "Workbench" (empty: made from the first station tag) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting Station")
	FText DisplayName;

	/** How close a player has to be to craft its recipes, in cm from the station's center */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting Station", meta = (ClampMin = "0", Units = "cm"))
	float CraftingRadius = 400.f;
};
