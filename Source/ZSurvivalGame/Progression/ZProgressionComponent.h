// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ZProgressionComponent.generated.h"

class UAbilitySystemComponent;
class UZProgressionComponent;
class UZTraitDefinition;

/** How many ranks a player put into one trait */
USTRUCT()
struct FZTraitRank : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UZTraitDefinition> Trait;

	UPROPERTY()
	int32 Rank = 0;
};

/** The player's trait ranks (a fast array: only changed ones are sent) */
USTRUCT()
struct FZTraitRankList : public FFastArraySerializer
{
	GENERATED_BODY()

	FZTraitRankList() {}

	explicit FZTraitRankList(UZProgressionComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	/** Owner's machine: tells the progression component the traits changed */
	void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FZTraitRank, FZTraitRankList>(Entries, DeltaParms, *this);
	}

private:

	friend class UZProgressionComponent;

	UPROPERTY()
	TArray<FZTraitRank> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UZProgressionComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FZTraitRankList> : public TStructOpsTypeTraitsBase2<FZTraitRankList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnProgressionChanged, UZProgressionComponent*, Progression);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZOnCharacterLevelUp, UZProgressionComponent*, Progression, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZOnBonusTraitPoints, UZProgressionComponent*, Progression, int32, Points);

/**
 *  A player's character progression: experience and level, trait points and the traits they're spent on. On the PlayerState, so it survives dying
 *  Trait points come from levels (Project Settings → Game → Progression), bosses (their Trait Point Reward) and items like the Tome of Knowledge
 *  The server applies every trait's bonus as one gameplay effect on the player's ability system (UZGameplayEffect_Traits)
 *  Experience, points and traits replicate to the owner only
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZProgressionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The progression component of an actor: its own, or for players the one on their PlayerState */
	UFUNCTION(BlueprintPure, Category = "Progression")
	static UZProgressionComponent* FindProgressionComponent(const AActor* Actor);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Owner's machine: remembers the current level and points, so later changes can be told apart from what arrived at the start */
	virtual void BeginPlay() override;

	/** --- LEVEL (the server, and the owner's copy on their machine) --- */

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetLevel() const;

	/** Total experience */
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetExperience() const { return Experience; }

	/** 0..1 of the way to the next level (1 at the highest level) */
	UFUNCTION(BlueprintPure, Category = "Progression")
	float GetLevelProgress() const;

	/** Server: adds experience, e.g. for a kill */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression")
	void AddExperience(int32 Amount);

	/** --- TRAIT POINTS --- */

	/** Points from levels, bosses and items together */
	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetEarnedTraitPoints() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetSpentTraitPoints() const;

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetUnspentTraitPoints() const;

	/** Server: extra points, e.g. from a boss or a Tome of Knowledge */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression")
	void AddTraitPoints(int32 Points);

	/** --- TRAITS --- */

	UFUNCTION(BlueprintPure, Category = "Progression")
	int32 GetTraitRank(const UZTraitDefinition* Trait) const;

	/** True if the trait isn't at its highest rank and there are enough points for one more */
	UFUNCTION(BlueprintPure, Category = "Progression")
	bool CanRaiseTrait(const UZTraitDefinition* Trait) const;

	/** Owning player: spends points on one more rank */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void RaiseTrait(UZTraitDefinition* Trait);

	/** True if the traits can be reset now (Project Settings → Progression) and some points are spent */
	UFUNCTION(BlueprintPure, Category = "Progression")
	bool CanResetTraits() const;

	/** Owning player: takes back every spent point, to spend them again */
	UFUNCTION(BlueprintCallable, Category = "Progression")
	void ResetTraits();

	/** Every trait in the game */
	UFUNCTION(BlueprintPure, Category = "Progression")
	TArray<UZTraitDefinition*> GetAllTraits() const;

	/** --- EVENTS (the server, and the owner's machine) --- */

	/** Experience, points or traits changed */
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FZOnProgressionChanged OnProgressionChanged;

	/** The character reached a new level */
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FZOnCharacterLevelUp OnLevelUp;

	/** Extra trait points arrived (from a boss or an item) */
	UPROPERTY(BlueprintAssignable, Category = "Progression")
	FZOnBonusTraitPoints OnBonusTraitPoints;

	/** "Level 3 (450 xp), 2 of 3 points spent; Vigor 2", for logs */
	FString GetDebugString() const;

protected:

	UFUNCTION(Server, Reliable)
	void ServerRaiseTrait(UZTraitDefinition* Trait);

	UFUNCTION(Server, Reliable)
	void ServerResetTraits();

	UFUNCTION()
	void OnRep_Experience();

	UFUNCTION()
	void OnRep_BonusTraitPoints();

private:

	friend struct FZTraitRankList;

	/** Server: puts the traits' bonuses on the ability system (replacing the old ones) */
	void ApplyTraitEffects();

	/** Owner's machine: the traits arrived */
	void HandleTraitsReplicated();

	UAbilitySystemComponent* GetAbilitySystem() const;

	UPROPERTY(ReplicatedUsing = OnRep_Experience)
	int32 Experience = 0;

	/** Points from bosses and items (points from levels come from the experience) */
	UPROPERTY(ReplicatedUsing = OnRep_BonusTraitPoints)
	int32 BonusTraitPoints = 0;

	UPROPERTY(Replicated)
	FZTraitRankList TraitRanks;

	/** Server: the effect holding the traits' bonuses */
	FActiveGameplayEffectHandle TraitEffectHandle;

	/** Owner's machine: what it saw last, to notice level-ups and new points */
	int32 SeenLevel = INDEX_NONE;
	int32 SeenBonusTraitPoints = INDEX_NONE;
};
