// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ZSurvivalGameTypes.h"
#include "ZMeleeMoveset.generated.h"

class UAnimMontage;

/** One attack of a combo: its animation and how hard it hits */
USTRUCT(BlueprintType)
struct FZMeleeAttack
{
	GENERATED_BODY()

	/**
	 *  The attack's animation, with notifies (see COMBAT.md): Weapon Hitbox where the swing hits, Combo Window where the next attack may start,
	 *  Dodge Window where a dodge may cut it short. Root motion in it moves the character
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage;

	/** Plays only this section of the montage, for a montage that holds several attacks (empty: the whole montage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName Section;

	/** Damage: the weapon's damage times this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.f;

	/** Poise damage: the weapon's poise damage times this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float PoiseDamageMultiplier = 1.f;

	/** Stamina it costs (times the weapon's Stamina Cost Multiplier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float StaminaCost = 15.f;

	/** A light attack pressed in this attack's Combo Window plays this light attack (its number in Light Attacks, from 1). 0: the next one (after a heavy attack: the first) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	int32 NextLightAttack = 0;

	/** A heavy attack pressed in this attack's Combo Window plays this heavy attack (its number in Heavy Attacks, from 1). 0: the next one (after a light attack: the first) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	int32 NextHeavyAttack = 0;
};

/**
 *  The attacks of a kind of melee weapon: a light combo (left mouse button) and a heavy combo (right mouse button)
 *  Weapons of the same kind share one (all knives, all one-handed swords, ...); their stats are in each weapon's own data
 *  Make one in Content/Weapons/Movesets: right-click → Miscellaneous → Data Asset → ZMeleeMoveset
 */
UCLASS(BlueprintType)
class UZMeleeMoveset : public UDataAsset
{
	GENERATED_BODY()

public:

	/** The light combo, in order; after the last one it starts again */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moveset")
	TArray<FZMeleeAttack> LightAttacks;

	/** The heavy combo, in order; after the last one it starts again */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Moveset")
	TArray<FZMeleeAttack> HeavyAttacks;

	const TArray<FZMeleeAttack>& GetAttacks(EZMeleeAttackType Type) const { return Type == EZMeleeAttackType::Light ? LightAttacks : HeavyAttacks; }

	/** An attack (Index from 0), or null if there's none */
	const FZMeleeAttack* GetAttack(EZMeleeAttackType Type, int32 Index) const;

	/** Which attack of NextType follows the attack (CurrentType, CurrentIndex) when it's pressed in that attack's Combo Window (Index from 0) */
	int32 GetFollowUp(EZMeleeAttackType CurrentType, int32 CurrentIndex, EZMeleeAttackType NextType) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
