// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZZombieCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AZZombieCharacter::AZZombieCharacter()
{
	// placeholder looks from the project's content (if these assets are moved or renamed, update the paths)
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ZombieMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> ZombieAnimation(TEXT("/Game/Variant_Combat/Anims/ABP_Manny_Combat"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ZombieAttack(TEXT("/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack"));

	if (ZombieMesh.Succeeded())
	{
		GetMesh()->SetSkeletalMeshAsset(ZombieMesh.Object);
	}

	if (ZombieAnimation.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(ZombieAnimation.Class);
	}

	if (ZombieAttack.Succeeded())
	{
		AttackMontage = ZombieAttack.Object;
	}

	// line the mannequin up with the capsule
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

	MaxHealth = 60.f;
	AggroRange = 3000.f;
	AttackCooldown = 1.6f;
	MeleeDamage = 15.f;
	GetCharacterMovement()->MaxWalkSpeed = 280.f;
}
