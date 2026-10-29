// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/ZZombieCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Items/ZLootTable.h"

AZZombieCharacter::AZZombieCharacter()
{
	// placeholder looks from the project's content (if these assets are moved or renamed, update the paths)
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ZombieMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> ZombieAnimation(TEXT("/Game/Variant_Combat/Anims/ABP_Manny_Combat"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ZombieAttack(TEXT("/Game/Variant_Combat/Anims/AM_ComboAttack.AM_ComboAttack"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ZombieStagger(TEXT("/Game/Characters/Mannequins/Anims/Stagger/AM_Stagger_Front.AM_Stagger_Front"));
	static ConstructorHelpers::FObjectFinder<UZLootTable> ZombieLoot(TEXT("/Game/Items/LootTables/LT_Zombie.LT_Zombie"));

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

	if (ZombieStagger.Succeeded())
	{
		StaggerMontage = ZombieStagger.Object;
	}

	if (ZombieLoot.Succeeded())
	{
		LootTable = ZombieLoot.Object;
	}

	// line the mannequin up with the capsule
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

	MaxHealth = 60.f;
	// zombies see far, smell the living close by, and never circle: they walk straight in
	CombatSettings.SightRadius = 3000.f;
	CombatSettings.LoseSightRadius = 3500.f;
	CombatSettings.ProximityRadius = 500.f;
	CombatSettings.bCanStrafe = false;
	AttackCooldown = 1.6f;
	MeleeDamage = 15.f;
	MeleePoiseDamage = 15.f;
	MaxPoise = 30.f;
	GetCharacterMovement()->MaxWalkSpeed = 280.f;
}
