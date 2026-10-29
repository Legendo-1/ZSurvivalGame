// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ZHitFlashComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

/**
 *  Makes its character flash when it's hit: the Hit Flash Material (Project Settings → Game → Combat) lies over the mesh for Hit Flash Time and fades out
 *  Every machine plays it on its own: the character calls Flash when its Hit Cue arrives (the server sends that cue to everyone with every hit)
 *  The material gets two parameters if it has them: FlashAmount (1 → 0 while it fades) and FlashColor
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class UZHitFlashComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZHitFlashComponent();

	/** Starts (or restarts) the flash */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Flash();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	/** Loads the flash material ahead of the first hit */
	virtual void BeginPlay() override;

	/** Takes the flash off the mesh */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** The character's mesh (the first skeletal mesh of other actors) */
	UMeshComponent* GetFlashMesh() const;

	/** Puts back what the mesh had over it before the flash */
	void StopFlash();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FlashMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OverlayBeforeFlash;

	float FlashTimeLeft = 0.f;
};
