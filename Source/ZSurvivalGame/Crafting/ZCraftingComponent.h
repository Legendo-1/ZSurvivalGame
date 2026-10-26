// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Items/ZTagStack.h"
#include "ZCraftingComponent.generated.h"

class AZCraftingStation;
class UZCraftingComponent;
class UZCraftingRecipe;
class UZInventoryComponent;
class UZItemInstance;
struct FZRecipeInput;

/** Whether a craft happened, and if not, why */
UENUM(BlueprintType)
enum class EZCraftingResult : uint8
{
	Success,
	/** The items don't make anything together */
	NoMatch,
	/** The right kinds of items, but not enough of something */
	MissingItems,
	/** A tool isn't good enough (its quality) or it's broken */
	ToolNotGoodEnough,
	/** A tradeskill is too low */
	SkillTooLow,
	/** A crafting station has to be nearby */
	NeedsStation,
	/** Already crafting something */
	Busy,
	Cancelled,
	/** No character, or it's dead */
	CantCraftNow,
	/** Bad request, e.g. items that aren't in the inventory */
	Invalid
};

/** A recipe the player has found */
USTRUCT()
struct FZDiscoveredRecipe : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UZCraftingRecipe> Recipe;
};

/** The recipes a player has found (a fast array: only new ones are sent) */
USTRUCT()
struct FZDiscoveredRecipeList : public FFastArraySerializer
{
	GENERATED_BODY()

	FZDiscoveredRecipeList() {}

	explicit FZDiscoveredRecipeList(UZCraftingComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	/** Owner's machine: tells the crafting component what was found */
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FZDiscoveredRecipe, FZDiscoveredRecipeList>(Entries, DeltaParms, *this);
	}

private:

	friend class UZCraftingComponent;

	UPROPERTY()
	TArray<FZDiscoveredRecipe> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UZCraftingComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FZDiscoveredRecipeList> : public TStructOpsTypeTraitsBase2<FZDiscoveredRecipeList>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};

/** What's being crafted right now (sent to the owner, for a progress bar) */
USTRUCT(BlueprintType)
struct FZCraftingProgress
{
	GENERATED_BODY()

	/** The recipe being crafted, or null when idle */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	TObjectPtr<UZCraftingRecipe> Recipe;

	/** Crafts left, counting the current one */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	int32 RemainingCount = 0;

	/** Seconds the current craft takes */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	float Duration = 0.f;

	/** Server time the current craft started */
	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	double StartTime = 0.0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FZOnRecipeDiscovered, UZCraftingComponent*, Crafting, UZCraftingRecipe*, Recipe);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnTradeskillsChanged, UZCraftingComponent*, Crafting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FZOnTradeskillLevelUp, UZCraftingComponent*, Crafting, FGameplayTag, Skill, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnCraftingProgressChanged, UZCraftingComponent*, Crafting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FZOnCraftingResult, UZCraftingComponent*, Crafting, EZCraftingResult, Result, UZCraftingRecipe*, Recipe, const FText&, Message);

/**
 *  A player's crafting: combining items into new ones, their tradeskills and the recipes they found. On the PlayerState, so it survives dying
 *  Recipes are hidden: the player combines items (CombineItems), and if they're exactly a recipe's inputs, the right tools are there,
 *  a station is nearby if needed and the tradeskills are high enough, the server crafts it and the tradeskills gain experience
 *  Tradeskills, discoveries and the current craft replicate to the owner only
 */
UCLASS(ClassGroup = (Survival), meta = (BlueprintSpawnableComponent))
class UZCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UZCraftingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The crafting component of an actor: its own, or for players the one on their PlayerState */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	static UZCraftingComponent* FindCraftingComponent(const AActor* Actor);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** --- CRAFTING --- */

	/** Owning player: tries to make something from exactly these items, Count times (one after another) */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CombineItems(const TArray<UZItemInstance*>& Items, int32 Count = 1);

	/** Owning player: stops crafting (what's already made stays) */
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CancelCrafting();

	UFUNCTION(BlueprintPure, Category = "Crafting")
	bool IsCrafting() const { return CraftingProgress.Recipe != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Crafting")
	const FZCraftingProgress& GetCraftingProgress() const { return CraftingProgress; }

	/** 0..1 through the current craft (0 when idle) */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	float GetCraftingProgressPercent() const;

	/** A crafting station of this kind near the player's character, or null */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	AZCraftingStation* FindNearbyStation(FGameplayTag StationTag) const;

	/** Every crafting station near the player's character, e.g. to show "At the Workbench" */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	TArray<AZCraftingStation*> GetNearbyStations() const;

	/** The message for a result, e.g. "This needs a Workbench nearby." (never gives away a recipe beyond what the player tried) */
	UFUNCTION(BlueprintPure, Category = "Crafting")
	FText GetResultText(EZCraftingResult Result, const UZCraftingRecipe* Recipe) const;

	/** --- TRADESKILLS (the server, and the owner's copy on their machine) --- */

	UFUNCTION(BlueprintPure, Category = "Crafting|Tradeskills")
	int32 GetSkillLevel(FGameplayTag Skill) const;

	/** Total experience in the skill */
	UFUNCTION(BlueprintPure, Category = "Crafting|Tradeskills")
	int32 GetSkillExperience(FGameplayTag Skill) const;

	/** 0..1 of the way to the next level (1 at the highest level) */
	UFUNCTION(BlueprintPure, Category = "Crafting|Tradeskills")
	float GetSkillLevelProgress(FGameplayTag Skill) const;

	/** Every tradeskill: the gameplay tags under Tradeskill */
	UFUNCTION(BlueprintPure, Category = "Crafting|Tradeskills")
	static TArray<FGameplayTag> GetAllTradeskills();

	/** "Electronics" for Tradeskill.Electronics */
	UFUNCTION(BlueprintPure, Category = "Crafting|Tradeskills")
	static FText GetSkillName(FGameplayTag Skill);

	/** Server: adds tradeskill experience. Returns the new level */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Tradeskills")
	int32 AddSkillExperience(FGameplayTag Skill, int32 Experience);

	/** Server: sets a tradeskill to the start of a level (e.g. for testing) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Tradeskills")
	void SetSkillLevel(FGameplayTag Skill, int32 Level);

	/** --- DISCOVERIES --- */

	/** True if the player has made this recipe before */
	UFUNCTION(BlueprintPure, Category = "Crafting|Discoveries")
	bool HasDiscovered(const UZCraftingRecipe* Recipe) const;

	UFUNCTION(BlueprintPure, Category = "Crafting|Discoveries")
	int32 GetNumDiscovered() const { return Discoveries.Entries.Num(); }

	/** How many recipes there are to find */
	UFUNCTION(BlueprintPure, Category = "Crafting|Discoveries")
	int32 GetNumRecipesInGame() const;

	/** Server: forgets every discovery (e.g. for testing) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Crafting|Discoveries")
	void ForgetDiscoveries();

	/** --- EVENTS (the server, and the owner's machine) --- */

	/** A recipe was made for the first time */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FZOnRecipeDiscovered OnRecipeDiscovered;

	/** Tradeskill experience changed */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FZOnTradeskillsChanged OnTradeskillsChanged;

	/** A tradeskill reached a new level */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FZOnTradeskillLevelUp OnTradeskillLevelUp;

	/** Crafting started, moved on to the next one, or stopped */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FZOnCraftingProgressChanged OnCraftingProgressChanged;

	/** A combine finished or failed (on the owner's machine), with a message for the player */
	UPROPERTY(BlueprintAssignable, Category = "Crafting")
	FZOnCraftingResult OnCraftingResult;

	/** "Electronics 3 (40 xp), Medicine 0; found 2 of 10 recipes; crafting: nothing", for logs */
	FString GetDebugString() const;

protected:

	UFUNCTION(Server, Reliable)
	void ServerCombineItems(const TArray<UZItemInstance*>& Items, int32 Count);

	UFUNCTION(Server, Reliable)
	void ServerCancelCrafting();

	/** Server → owner: how a combine went */
	UFUNCTION(Client, Reliable)
	void ClientCraftingResult(EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message);

	UFUNCTION()
	void OnRep_CraftingProgress();

	UFUNCTION()
	void OnRep_SkillExperience();

	/** Most crafts one combine can ask for */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 MaxCraftsPerRequest = 50;

	/** Most items one combine can use */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Crafting", meta = (ClampMin = "1"))
	int32 MaxCombineItems = 6;

private:

	/** Which item instances one craft takes and wears down */
	struct FCraftPlan
	{
		TArray<TPair<UZItemInstance*, int32>> ToRemove;
		TArray<TPair<UZItemInstance*, int32>> ToWear;
	};

	/** Everything but the items: station, tradeskills, tools good enough */
	EZCraftingResult CheckRecipe(const UZCraftingRecipe& Recipe, TConstArrayView<const UZItemInstance*> PreferredItems) const;

	/** Picks the stacks one craft uses, the chosen items first. Returns false if something is missing */
	bool BuildCraftPlan(const UZCraftingRecipe& Recipe, TConstArrayView<const UZItemInstance*> PreferredItems, FCraftPlan& OutPlan) const;

	/** Server: takes the ingredients, wears the tools and gives the results */
	void ApplyCraftPlan(const UZCraftingRecipe& Recipe, const FCraftPlan& Plan);

	/** Server: tradeskill experience for one craft (and the bonus for a first discovery) */
	void GiveCraftExperience(const UZCraftingRecipe& Recipe, bool bFirstDiscovery);

	/** Server: crafts the recipe Count times (on a timer if it takes time) */
	void StartCrafting(UZCraftingRecipe* Recipe, int32 Count, TConstArrayView<UZItemInstance*> PreferredItems);

	/** Server: starts the timer for the next craft of the current recipe */
	void StartNextCraft();

	/** Server: the current craft's time is up */
	void OnCraftTimerDone();

	/** Server: checks everything again and crafts the recipe once */
	EZCraftingResult CraftOnce(UZCraftingRecipe* Recipe);

	/** Server: stops crafting and tells the owner what was made */
	void FinishCrafting(UZCraftingRecipe* Recipe, EZCraftingResult LastResult);

	/** Server: tells the owner how a combine went (and logs it) */
	void SendResult(EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message);

	/** True if the character is there and alive */
	bool CanCraftNow() const;

	/** Tools of this kind, good enough and not broken */
	int32 GetUsableToolCount(const FZRecipeInput& Tool) const;

	void BroadcastProgressChanged();

	double GetServerTime() const;

	UZInventoryComponent* GetInventory() const;

	/** The player's character (the PlayerState's pawn) */
	APawn* GetPawn() const;

	UPROPERTY(Replicated)
	FZDiscoveredRecipeList Discoveries;

	/** Total experience per tradeskill tag */
	UPROPERTY(ReplicatedUsing = OnRep_SkillExperience)
	FZTagStackContainer SkillExperience;

	UPROPERTY(ReplicatedUsing = OnRep_CraftingProgress)
	FZCraftingProgress CraftingProgress;

	/** Owner's machine: the levels last seen, to notice level-ups when experience arrives */
	TMap<FGameplayTag, int32> SeenSkillLevels;

	/** Server: the current combine, for its message at the end */
	TArray<TWeakObjectPtr<UZItemInstance>> RequestItems;
	int32 NumCrafted = 0;
	int32 NumLuckyCrafts = 0;
	bool bRequestDiscovered = false;
	TArray<FText> RequestNotes;
	TMap<FGameplayTag, int32> RequestExperience;
	TMap<FGameplayTag, int32> RequestStartLevels;
	FTimerHandle CraftTimer;
};
