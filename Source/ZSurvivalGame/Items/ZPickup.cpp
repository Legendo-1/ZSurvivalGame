// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemInstance.h"

namespace
{
	// how high above the ground a pickup floats
	constexpr float PickupHoverHeight = 20.f;
}

AZPickup::AZPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(false);

	// players' interaction traces find it, and players walking into it trigger auto pickup; nothing else collides
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(50.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetCollisionResponseToChannel(ZTraceChannel_Interaction, ECR_Block);
	CollisionSphere->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(CollisionSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Rotation = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Rotation"));
	Rotation->RotationRate = FRotator(0.f, 90.f, 0.f);
	Rotation->SetUpdatedComponent(Mesh);

	// placeholder look until items have their own meshes
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		DefaultMesh = CubeMesh.Object;
	}
}

void AZPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AZPickup, Item);
}

AZPickup* AZPickup::SpawnPickup(UWorld* World, TSubclassOf<AZPickup> PickupClass, const FZItemStack& Stack, const FVector& Location,
	const UZItemInstance* SourceInstance, bool bInAllowAutoPickup, float LifeSpan)
{
	if (!World || World->GetNetMode() == NM_Client || !Stack.Item || Stack.Count <= 0)
	{
		return nullptr;
	}

	// on the ground below the location (static geometry only, not on bodies or other pickups)
	FVector SpawnLocation = Location;
	FHitResult Ground;
	if (World->LineTraceSingleByObjectType(Ground, Location + FVector(0.f, 0.f, 100.f), Location - FVector(0.f, 0.f, 500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		SpawnLocation = Ground.ImpactPoint + FVector(0.f, 0.f, PickupHoverHeight);
	}

	const FTransform SpawnTransform(SpawnLocation);
	UClass* ClassToSpawn = PickupClass ? PickupClass.Get() : AZPickup::StaticClass();
	AZPickup* Pickup = World->SpawnActorDeferred<AZPickup>(ClassToSpawn, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->Item = Stack;
	Pickup->bAllowAutoPickup = bInAllowAutoPickup;

	// a non-stackable item keeps its own stats on the ground
	if (SourceInstance && !Stack.Item->IsStackable())
	{
		Pickup->ItemInstance = NewObject<UZItemInstance>(Pickup);
		Pickup->ItemInstance->InitializeInstance(Stack.Item);
		Pickup->ItemInstance->CopyStatsFrom(SourceInstance);
	}

	Pickup->FinishSpawning(SpawnTransform);

	if (LifeSpan > 0.f)
	{
		Pickup->SetLifeSpan(LifeSpan);
	}

	return Pickup;
}

void AZPickup::BeginPlay()
{
	Super::BeginPlay();

	UpdateMesh();

	if (HasAuthority())
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AZPickup::OnSphereBeginOverlap);

		// a player may already stand where it appeared
		TArray<AActor*> OverlappingPawns;
		CollisionSphere->GetOverlappingActors(OverlappingPawns, APawn::StaticClass());
		for (AActor* OverlappingPawn : OverlappingPawns)
		{
			TryAutoPickup(OverlappingPawn);
		}
	}
}

void AZPickup::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryAutoPickup(OtherActor);
}

void AZPickup::TryAutoPickup(AActor* OtherActor)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!HasAuthority() || !bAllowAutoPickup || !Item.Item || !Item.Item->bAutoPickup || !Pawn || !Pawn->IsPlayerControlled() || UZCombatLibrary::IsDead(Pawn))
	{
		return;
	}

	GiveTo(Pawn);
}

bool AZPickup::GiveTo(APawn* Pawn)
{
	UZInventoryComponent* Inventory = UZInventoryComponent::FindInventoryComponent(Pawn);
	if (!HasAuthority() || !Inventory || !Item.Item || Item.Count <= 0 || IsActorBeingDestroyed())
	{
		return false;
	}

	// a dropped non-stackable item keeps its stats; anything else is added by definition
	int32 NumAdded = 0;
	if (ItemInstance && !Item.Item->IsStackable())
	{
		NumAdded = Inventory->AddItemCopy(ItemInstance) ? 1 : 0;
	}
	else
	{
		NumAdded = Inventory->AddItem(Item.Item, Item.Count);
	}

	if (NumAdded <= 0)
	{
		return false;
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' picked up %s x%d"), *GetNameSafe(Pawn), *GetNameSafe(Item.Item), NumAdded);

	// whatever didn't fit stays on the ground
	Item.Count -= NumAdded;
	if (Item.Count <= 0)
	{
		Destroy();
	}

	return true;
}

bool AZPickup::CanInteract_Implementation(APawn* Interactor) const
{
	return Item.Item && Item.Count > 0 && !IsActorBeingDestroyed();
}

FText AZPickup::GetInteractionText_Implementation(APawn* Interactor) const
{
	if (!Item.Item)
	{
		return FText::GetEmpty();
	}

	const FText ItemName = Item.Item->DisplayName.IsEmpty() ? FText::FromName(Item.Item->GetFName()) : Item.Item->DisplayName;
	if (Item.Count > 1)
	{
		return FText::Format(NSLOCTEXT("ZPickup", "PickUpStack", "Pick up {0} x{1}"), ItemName, Item.Count);
	}

	return FText::Format(NSLOCTEXT("ZPickup", "PickUpOne", "Pick up {0}"), ItemName);
}

void AZPickup::Interact_Implementation(APawn* Interactor)
{
	GiveTo(Interactor);
}

void AZPickup::OnRep_Item()
{
	UpdateMesh();
}

void AZPickup::UpdateMesh()
{
	const bool bHasItemMesh = Item.Item && Item.Item->PickupMesh;
	Mesh->SetStaticMesh(bHasItemMesh ? Item.Item->PickupMesh.Get() : DefaultMesh.Get());
	Mesh->SetRelativeScale3D(bHasItemMesh ? Item.Item->PickupMeshScale : DefaultMeshScale);
}
