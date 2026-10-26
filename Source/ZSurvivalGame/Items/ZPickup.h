// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/ZInteractable.h"
#include "Items/ZItemDefinition.h"
#include "ZPickup.generated.h"

class URotatingMovementComponent;
class USphereComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UZItemInstance;

/**
 *  Items lying in the world: loot from enemies, dropped items, or placed in a level (set Item in its Details)
 *  Picked up with the interact key, or by walking over it if the item allows auto pickup
 */
UCLASS()
class AZPickup : public AActor, public IZInteractable
{
	GENERATED_BODY()

	/** Found by the interaction trace, and overlaps players for auto pickup */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Slowly turns the mesh, so items stand out (on each machine) */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<URotatingMovementComponent> Rotation;

public:

	AZPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 *  Server: spawns a pickup on the ground near Location
	 *  SourceInstance keeps a non-stackable item's stats (e.g. a dropped weapon's ammo); LifeSpan 0 = stays until picked up
	 */
	static AZPickup* SpawnPickup(UWorld* World, TSubclassOf<AZPickup> PickupClass, const FZItemStack& Stack, const FVector& Location,
		const UZItemInstance* SourceInstance = nullptr, bool bInAllowAutoPickup = true, float LifeSpan = 0.f);

	/** Server: puts the items into the pawn's inventory. The pickup disappears once everything fit */
	bool GiveTo(APawn* Pawn);

	/** IZInteractable */
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual FText GetInteractionText_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:

	/** Picks up for players already standing on it, and shows the item's mesh */
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Server: picks up for a player walking over it, if allowed */
	void TryAutoPickup(AActor* OtherActor);

	UFUNCTION()
	void OnRep_Item();

	/** Shows the item's pickup mesh, or the default cube */
	void UpdateMesh();

	/** What lies here (replicated, so every player sees it and the prompt can name it) */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Item, Category = "Pickup")
	FZItemStack Item;

	/** Picked up by walking over it, if the item allows it (off for items dropped by players) */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	bool bAllowAutoPickup = true;

	/** Mesh used when the item doesn't have a pickup mesh */
	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	TObjectPtr<UStaticMesh> DefaultMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	FVector DefaultMeshScale = FVector(0.25f);

	/** Stats of the item it was dropped as (server only), e.g. a weapon's ammo */
	UPROPERTY()
	TObjectPtr<UZItemInstance> ItemInstance;
};
