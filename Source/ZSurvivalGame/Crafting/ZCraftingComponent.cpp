// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/ZCraftingComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZCraftingStation.h"
#include "Crafting/ZCraftingSubsystem.h"
#include "Crafting/ZRecipeOutcomes.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemInstance.h"
#include "Progression/ZProgressionSettings.h"

#define LOCTEXT_NAMESPACE "ZCrafting"

namespace
{
	// the server allows a bit more distance to a station, since the player has moved on by the time a request arrives
	constexpr float ServerStationRangeSlack = 100.f;

	/** Which failure tells the player the most, when several recipes use the same kinds of items */
	int32 GetFailurePriority(EZCraftingResult Result)
	{
		switch (Result)
		{
		case EZCraftingResult::CannotApply:			return 5;
		case EZCraftingResult::SkillTooLow:			return 4;
		case EZCraftingResult::ToolNotGoodEnough:	return 3;
		case EZCraftingResult::MissingItems:		return 2;
		case EZCraftingResult::NeedsStation:		return 1;
		default:									return 0;
		}
	}

	/** "Electronic Parts x2, Scrap x1" for Times crafts of the recipe */
	FText GetResultsText(const UZCraftingRecipe& Recipe, int32 Times)
	{
		TArray<FText> Parts;
		for (const FZItemStack& Result : Recipe.Results)
		{
			if (Result.Item && Result.Count > 0)
			{
				Parts.Add(FText::Format(LOCTEXT("ResultStack", "{0} x{1}"), Result.Item->GetItemName(), Result.Count * Times));
			}
		}

		return FText::Join(LOCTEXT("ListSeparator", ", "), Parts);
	}
}

void FZDiscoveredRecipeList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	if (!OwnerComponent)
	{
		return;
	}

	for (const int32 Index : AddedIndices)
	{
		if (UZCraftingRecipe* Recipe = Entries[Index].Recipe)
		{
			OwnerComponent->OnRecipeDiscovered.Broadcast(OwnerComponent, Recipe);
		}
	}
}

UZCraftingComponent::UZCraftingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Discoveries(this)
{
	SetIsReplicatedByDefault(true);
}

UZCraftingComponent* UZCraftingComponent::FindCraftingComponent(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UZCraftingComponent* OwnCrafting = Actor->FindComponentByClass<UZCraftingComponent>())
	{
		return OwnCrafting;
	}

	// players keep it on the PlayerState, so it survives respawns
	const APlayerState* PlayerState = nullptr;
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		PlayerState = Pawn->GetPlayerState();
	}
	else if (const AController* Controller = Cast<AController>(Actor))
	{
		PlayerState = Controller->GetPlayerState<APlayerState>();
	}

	return PlayerState ? PlayerState->FindComponentByClass<UZCraftingComponent>() : nullptr;
}

void UZCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only the player themselves needs their skills, discoveries and progress
	DOREPLIFETIME_CONDITION(UZCraftingComponent, Discoveries, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UZCraftingComponent, SkillExperience, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UZCraftingComponent, CraftingProgress, COND_OwnerOnly);
}

void UZCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CraftTimer);
	}

	StopWorkingCue();

	Super::EndPlay(EndPlayReason);
}

void UZCraftingComponent::CombineItems(const TArray<UZItemInstance*>& Items, int32 Count)
{
	// on the server this runs right away
	ServerCombineItems(Items, Count);
}

void UZCraftingComponent::CancelCrafting()
{
	ServerCancelCrafting();
}

float UZCraftingComponent::GetCraftingProgressPercent() const
{
	if (!IsCrafting() || CraftingProgress.Duration <= 0.f)
	{
		return 0.f;
	}

	return FMath::Clamp(static_cast<float>((GetServerTime() - CraftingProgress.StartTime) / CraftingProgress.Duration), 0.f, 1.f);
}

AZCraftingStation* UZCraftingComponent::FindNearbyStation(FGameplayTag StationTag) const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn || !StationTag.IsValid())
	{
		return nullptr;
	}

	// the server is a bit more forgiving with remote players, who have moved on by the time their request arrives
	const float ExtraRange = GetOwner()->HasAuthority() && !Pawn->IsLocallyControlled() ? ServerStationRangeSlack : 0.f;

	for (TActorIterator<AZCraftingStation> It(GetWorld()); It; ++It)
	{
		if (It->ProvidesStation(StationTag) && It->IsInCraftingRange(Pawn, ExtraRange))
		{
			return *It;
		}
	}

	return nullptr;
}

TArray<AZCraftingStation*> UZCraftingComponent::GetNearbyStations() const
{
	TArray<AZCraftingStation*> Stations;
	if (const APawn* Pawn = GetPawn())
	{
		for (TActorIterator<AZCraftingStation> It(GetWorld()); It; ++It)
		{
			if (It->IsInCraftingRange(Pawn))
			{
				Stations.Add(*It);
			}
		}
	}

	return Stations;
}

FText UZCraftingComponent::GetResultText(EZCraftingResult Result, const UZCraftingRecipe* Recipe) const
{
	switch (Result)
	{
	case EZCraftingResult::Success:
		if (!Recipe)
		{
			return FText::GetEmpty();
		}

		// a recipe that only changes an item (e.g. an upgrade) makes nothing to list
		return Recipe->GetMainResult()
			? FText::Format(LOCTEXT("Crafted", "Crafted {0}."), GetResultsText(*Recipe, 1))
			: FText::Format(LOCTEXT("Done", "{0}: done."), Recipe->GetRecipeName());

	case EZCraftingResult::NoMatch:
		return LOCTEXT("NoMatch", "Nothing comes of combining these.");

	case EZCraftingResult::MissingItems:
		return LOCTEXT("MissingItems", "Something could come of this, but you need more.");

	case EZCraftingResult::ToolNotGoodEnough:
		return LOCTEXT("ToolNotGoodEnough", "Your tools aren't good enough for this.");

	case EZCraftingResult::SkillTooLow:
	{
		// the first tradeskill that's too low
		if (Recipe)
		{
			for (const FZSkillRequirement& Requirement : Recipe->SkillRequirements)
			{
				const int32 Level = GetSkillLevel(Requirement.Skill);
				if (Requirement.Skill.IsValid() && Level < Requirement.MinLevel)
				{
					return FText::Format(LOCTEXT("SkillTooLow", "You need {0} {1} for this. You have {2}."), GetSkillName(Requirement.Skill), Requirement.MinLevel, Level);
				}
			}
		}

		return LOCTEXT("SkillTooLowGeneric", "You aren't skilled enough for this yet.");
	}

	case EZCraftingResult::CannotApply:
		// the outcome usually says why itself, e.g. "Your Rifle can't be upgraded any further."
		return LOCTEXT("CannotApply", "That won't work on it.");

	case EZCraftingResult::NeedsStation:
		return FText::Format(LOCTEXT("NeedsStation", "This needs a {0} nearby."), Recipe ? AZCraftingStation::GetStationNameFromTag(Recipe->RequiredStation) : FText::GetEmpty());

	case EZCraftingResult::Busy:
		return LOCTEXT("Busy", "Already crafting something.");

	case EZCraftingResult::Cancelled:
		return LOCTEXT("Cancelled", "Crafting stopped.");

	case EZCraftingResult::CantCraftNow:
		return LOCTEXT("CantCraftNow", "You can't craft right now.");

	case EZCraftingResult::Invalid:
	default:
		return LOCTEXT("Invalid", "That can't be combined.");
	}
}

int32 UZCraftingComponent::GetSkillLevel(FGameplayTag Skill) const
{
	return UZProgressionSettings::Get()->GetTradeskillLevelForExperience(GetSkillExperience(Skill));
}

int32 UZCraftingComponent::GetSkillExperience(FGameplayTag Skill) const
{
	return Skill.IsValid() ? SkillExperience.GetStackCount(Skill) : 0;
}

float UZCraftingComponent::GetSkillLevelProgress(FGameplayTag Skill) const
{
	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 Level = GetSkillLevel(Skill);
	if (Level >= Settings->MaxTradeskillLevel)
	{
		return 1.f;
	}

	const int32 LevelStart = Settings->GetTradeskillExperienceForLevel(Level);
	const int32 LevelEnd = Settings->GetTradeskillExperienceForLevel(Level + 1);
	return LevelEnd > LevelStart ? FMath::Clamp(static_cast<float>(GetSkillExperience(Skill) - LevelStart) / (LevelEnd - LevelStart), 0.f, 1.f) : 1.f;
}

TArray<FGameplayTag> UZCraftingComponent::GetAllTradeskills()
{
	TArray<FGameplayTag> Skills;
	UGameplayTagsManager::Get().RequestGameplayTagChildren(ZGameplayTags::Tradeskill).GetGameplayTagArray(Skills);
	Skills.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });
	return Skills;
}

FText UZCraftingComponent::GetSkillName(FGameplayTag Skill)
{
	FString TagName = Skill.ToString();
	int32 LastDot = INDEX_NONE;
	if (TagName.FindLastChar(TEXT('.'), LastDot))
	{
		TagName.RightChopInline(LastDot + 1);
	}

	return FText::FromString(FName::NameToDisplayString(TagName, false));
}

int32 UZCraftingComponent::AddSkillExperience(FGameplayTag Skill, int32 Experience)
{
	if (!GetOwner()->HasAuthority() || !Skill.IsValid() || Experience <= 0)
	{
		return GetSkillLevel(Skill);
	}

	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 OldLevel = GetSkillLevel(Skill);
	const int64 MaxExperience = Settings->GetTradeskillExperienceForLevel(Settings->MaxTradeskillLevel);
	const int32 NewExperience = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(GetSkillExperience(Skill)) + Experience, MaxExperience));
	SkillExperience.SetStack(Skill, NewExperience);

	const int32 NewLevel = GetSkillLevel(Skill);
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' got %d %s experience (%d total, level %d)"), *GetNameSafe(GetOwner()), Experience, *Skill.ToString(), NewExperience, NewLevel);

	// the owner's machine finds out in OnRep_SkillExperience
	OnTradeskillsChanged.Broadcast(this);
	if (NewLevel > OldLevel)
	{
		OnTradeskillLevelUp.Broadcast(this, Skill, NewLevel);
	}

	return NewLevel;
}

void UZCraftingComponent::SetSkillLevel(FGameplayTag Skill, int32 Level)
{
	if (!GetOwner()->HasAuthority() || !Skill.IsValid())
	{
		return;
	}

	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	SkillExperience.SetStack(Skill, Settings->GetTradeskillExperienceForLevel(FMath::Clamp(Level, 0, Settings->MaxTradeskillLevel)));
	OnTradeskillsChanged.Broadcast(this);
}

bool UZCraftingComponent::HasDiscovered(const UZCraftingRecipe* Recipe) const
{
	return Recipe && Discoveries.Entries.ContainsByPredicate([Recipe](const FZDiscoveredRecipe& Entry) { return Entry.Recipe == Recipe; });
}

int32 UZCraftingComponent::GetNumRecipesInGame() const
{
	const UZCraftingSubsystem* Subsystem = UZCraftingSubsystem::Get(this);
	return Subsystem ? Subsystem->GetRecipes().Num() : 0;
}

void UZCraftingComponent::ForgetDiscoveries()
{
	if (GetOwner()->HasAuthority() && !Discoveries.Entries.IsEmpty())
	{
		Discoveries.Entries.Reset();
		Discoveries.MarkArrayDirty();
	}
}

TArray<UZCraftingRecipe*> UZCraftingComponent::GetDiscoveredRecipes() const
{
	TArray<UZCraftingRecipe*> Recipes;
	for (const FZDiscoveredRecipe& Entry : Discoveries.Entries)
	{
		if (Entry.Recipe)
		{
			Recipes.Add(Entry.Recipe);
		}
	}

	return Recipes;
}

void UZCraftingComponent::RestoreCrafting(const TArray<TPair<FGameplayTag, int32>>& InSkillExperience, const TArray<UZCraftingRecipe*>& InDiscoveries, TArray<FString>& OutFixes)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 MaxExperience = Settings->GetTradeskillExperienceForLevel(Settings->MaxTradeskillLevel);

	FZTagStackContainer Skills;
	for (const TPair<FGameplayTag, int32>& Saved : InSkillExperience)
	{
		if (!Saved.Key.MatchesTag(ZGameplayTags::Tradeskill) || Saved.Key == ZGameplayTags::Tradeskill.GetTag())
		{
			OutFixes.Add(FString::Printf(TEXT("'%s' isn't a tradeskill"), *Saved.Key.ToString()));
			continue;
		}

		const int32 Experience = FMath::Clamp(Saved.Value, 0, MaxExperience);
		if (Experience != Saved.Value)
		{
			OutFixes.Add(FString::Printf(TEXT("%s experience %d is outside 0..%d (the highest level)"), *Saved.Key.ToString(), Saved.Value, MaxExperience));
		}

		Skills.SetStack(Saved.Key, Experience);
	}

	SkillExperience.CopyFrom(Skills);

	Discoveries.Entries.Reset();
	for (UZCraftingRecipe* Recipe : InDiscoveries)
	{
		if (Recipe && !HasDiscovered(Recipe))
		{
			Discoveries.Entries.AddDefaulted_GetRef().Recipe = Recipe;
		}
	}

	Discoveries.MarkArrayDirty();
	OnTradeskillsChanged.Broadcast(this);
}

void UZCraftingComponent::ServerCombineItems_Implementation(const TArray<UZItemInstance*>& Items, int32 Count)
{
	if (!CanCraftNow() || IsCrafting())
	{
		const EZCraftingResult Result = IsCrafting() ? EZCraftingResult::Busy : EZCraftingResult::CantCraftNow;
		SendResult(Result, nullptr, GetResultText(Result, nullptr));
		return;
	}

	// only items from this player's inventory, each once
	const UZInventoryComponent* Inventory = GetInventory();
	TArray<UZItemInstance*> ChosenItems;
	TArray<const UZItemDefinition*> ChosenKinds;
	for (UZItemInstance* Item : Items)
	{
		if (ChosenItems.Num() < MaxCombineItems && Inventory && Item && Item->GetItemDefinition() && !ChosenItems.Contains(Item) && Inventory->GetStackCount(Item) > 0)
		{
			ChosenItems.Add(Item);
			ChosenKinds.AddUnique(Item->GetItemDefinition());
		}
	}

	const UZCraftingSubsystem* Subsystem = UZCraftingSubsystem::Get(this);
	if (ChosenItems.IsEmpty() || !Subsystem)
	{
		SendResult(EZCraftingResult::Invalid, nullptr, GetResultText(EZCraftingResult::Invalid, nullptr));
		return;
	}

	// the first recipe these items make that can be crafted right now wins; otherwise tell the player what's closest
	const TArray<const UZItemInstance*> PreferredItems(ChosenItems);
	EZCraftingResult Failure = EZCraftingResult::NoMatch;
	UZCraftingRecipe* FailedRecipe = nullptr;
	FText FailureReason;

	for (UZCraftingRecipe* Recipe : Subsystem->FindRecipesForCombination(ChosenKinds))
	{
		FCraftPlan Plan;
		FText Reason;
		const EZCraftingResult Result = CheckRecipe(*Recipe, PreferredItems, Plan, Reason);
		if (Result == EZCraftingResult::Success)
		{
			// a recipe that changes an item (an upgrade, a mod) does it once per combine, so each change gets its own message
			const int32 MaxCount = Recipe->Outcomes.IsEmpty() ? MaxCraftsPerRequest : 1;
			StartCrafting(Recipe, FMath::Clamp(Count, 1, MaxCount), ChosenItems);
			return;
		}

		if (GetFailurePriority(Result) > GetFailurePriority(Failure))
		{
			Failure = Result;
			FailedRecipe = Recipe;
			FailureReason = Reason;
		}
	}

	SendResult(Failure, FailedRecipe, FailureReason.IsEmpty() ? GetResultText(Failure, FailedRecipe) : FailureReason);
}

void UZCraftingComponent::ServerCancelCrafting_Implementation()
{
	if (IsCrafting())
	{
		FinishCrafting(CraftingProgress.Recipe, EZCraftingResult::Cancelled);
	}
}

void UZCraftingComponent::ClientCraftingResult_Implementation(EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message, bool bNewDiscovery)
{
	OnCraftingResult.Broadcast(this, Result, Recipe, Message, bNewDiscovery);
}

void UZCraftingComponent::OnRep_CraftingProgress()
{
	OnCraftingProgressChanged.Broadcast(this);
}

void UZCraftingComponent::OnRep_SkillExperience()
{
	// level-ups, compared with what this machine saw before (nothing to compare with the first time)
	for (const FGameplayTag& Skill : GetAllTradeskills())
	{
		const int32 Level = GetSkillLevel(Skill);
		const int32* SeenLevel = SeenSkillLevels.Find(Skill);
		if (SeenLevel && Level > *SeenLevel)
		{
			OnTradeskillLevelUp.Broadcast(this, Skill, Level);
		}

		SeenSkillLevels.Add(Skill, Level);
	}

	OnTradeskillsChanged.Broadcast(this);
}

EZCraftingResult UZCraftingComponent::CheckRecipe(const UZCraftingRecipe& Recipe, TConstArrayView<const UZItemInstance*> PreferredItems, FCraftPlan& OutPlan, FText& OutReason) const
{
	if (Recipe.RequiredStation.IsValid() && !FindNearbyStation(Recipe.RequiredStation))
	{
		return EZCraftingResult::NeedsStation;
	}

	// a hard limit: no tradeskill, no craft
	for (const FZSkillRequirement& Requirement : Recipe.SkillRequirements)
	{
		if (Requirement.Skill.IsValid() && GetSkillLevel(Requirement.Skill) < Requirement.MinLevel)
		{
			return EZCraftingResult::SkillTooLow;
		}
	}

	for (const FZRecipeInput& Input : Recipe.Inputs)
	{
		if (Input.IsValid() && Input.IsTool() && GetUsableToolCount(Input) < Input.Count)
		{
			return EZCraftingResult::ToolNotGoodEnough;
		}
	}

	if (!BuildCraftPlan(Recipe, PreferredItems, OutPlan))
	{
		return EZCraftingResult::MissingItems;
	}

	// last, whether it can happen to the target: e.g. a weapon can't go past its highest upgrade, and a mod has to fit
	if (!Recipe.Outcomes.IsEmpty())
	{
		const FZRecipeOutcomeContext OutcomeContext = MakeOutcomeContext(OutPlan);
		for (const UZRecipeOutcome* Outcome : Recipe.Outcomes)
		{
			if (Outcome && !Outcome->CanApply(OutcomeContext, OutReason))
			{
				return EZCraftingResult::CannotApply;
			}
		}
	}

	return EZCraftingResult::Success;
}

bool UZCraftingComponent::BuildCraftPlan(const UZCraftingRecipe& Recipe, TConstArrayView<const UZItemInstance*> PreferredItems, FCraftPlan& OutPlan) const
{
	const UZInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return false;
	}

	// every stack once: the chosen items first, then the newest stacks (like RemoveItem)
	struct FAvailableStack
	{
		UZItemInstance* Instance;
		int32 Count;
	};

	const TArray<FZInventoryItem> Stacks = Inventory->GetItems();
	TArray<FAvailableStack> Available;
	for (const FZInventoryItem& Stack : Stacks)
	{
		if (Stack.Instance && PreferredItems.Contains(Stack.Instance.Get()))
		{
			Available.Add({ Stack.Instance, Stack.Count });
		}
	}

	for (int32 Index = Stacks.Num() - 1; Index >= 0; --Index)
	{
		if (Stacks[Index].Instance && !PreferredItems.Contains(Stacks[Index].Instance.Get()))
		{
			Available.Add({ Stacks[Index].Instance, Stacks[Index].Count });
		}
	}

	// takes what an input needs from what's still available, so no item counts twice
	auto Reserve = [&Available](const FZRecipeInput& Input, TArray<TPair<UZItemInstance*, int32>>& OutUsed)
	{
		int32 Needed = Input.Count;
		for (FAvailableStack& Stack : Available)
		{
			if (Needed <= 0)
			{
				break;
			}

			// broken tools can't be used
			if (Stack.Count <= 0 || !Input.Matches(Stack.Instance->GetItemDefinition()) || (Input.IsTool() && Stack.Instance->IsBroken()))
			{
				continue;
			}

			const int32 Taken = FMath::Min(Stack.Count, Needed);
			Stack.Count -= Taken;
			Needed -= Taken;
			OutUsed.Emplace(Stack.Instance, Taken);
		}

		return Needed <= 0;
	};

	bool bHasEverything = true;
	for (const FZRecipeInput& Input : Recipe.Inputs)
	{
		if (!Input.IsValid())
		{
			continue;
		}

		TArray<TPair<UZItemInstance*, int32>> Used;
		bHasEverything &= Reserve(Input, Used);

		switch (Input.Usage)
		{
		case EZRecipeInputUsage::Consumed:
			OutPlan.ToRemove.Append(Used);
			break;

		case EZRecipeInputUsage::Tool:
			for (const TPair<UZItemInstance*, int32>& UsedTool : Used)
			{
				if (Input.DurabilityCost > 0 && UsedTool.Key->HasDurability())
				{
					OutPlan.ToWear.Emplace(UsedTool.Key, Input.DurabilityCost);
				}
			}
			break;

		case EZRecipeInputUsage::Target:
			// the outcomes change one item: the chosen one, if the player picked one
			if (!OutPlan.Target && !Used.IsEmpty())
			{
				OutPlan.Target = Used[0].Key;
			}
			break;
		}
	}

	return bHasEverything;
}

FZRecipeOutcomeContext UZCraftingComponent::MakeOutcomeContext(const FCraftPlan& Plan) const
{
	FZRecipeOutcomeContext Context;
	Context.Crafting = this;
	Context.Inventory = GetInventory();
	Context.Target = Plan.Target;

	for (const TPair<UZItemInstance*, int32>& Ingredient : Plan.ToRemove)
	{
		if (UZItemDefinition* Item = Ingredient.Key->GetItemDefinition())
		{
			Context.ConsumedItems.AddUnique(Item);
		}
	}

	return Context;
}

void UZCraftingComponent::ApplyCraftPlan(const UZCraftingRecipe& Recipe, const FCraftPlan& Plan)
{
	UZInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}

	// worked out while the ingredients are still there (e.g. which mod is being installed); the outcomes add their lines to the message
	FZRecipeOutcomeContext OutcomeContext = MakeOutcomeContext(Plan);
	OutcomeContext.Notes = &RequestNotes;

	for (const TPair<UZItemInstance*, int32>& Ingredient : Plan.ToRemove)
	{
		Inventory->RemoveItemInstance(Ingredient.Key, Ingredient.Value);
	}

	for (const TPair<UZItemInstance*, int32>& Tool : Plan.ToWear)
	{
		const FText ToolName = Tool.Key->GetItemDefinition() ? Tool.Key->GetItemDefinition()->GetItemName() : FText::GetEmpty();
		if (Inventory->ReduceDurability(Tool.Key, Tool.Value))
		{
			RequestNotes.Add(FText::Format(LOCTEXT("ToolBroke", "Your {0} broke."), ToolName));
		}
	}

	// after the ingredients are gone, so e.g. a mod that comes off has room
	for (const UZRecipeOutcome* Outcome : Recipe.Outcomes)
	{
		if (Outcome)
		{
			Outcome->Apply(OutcomeContext);
		}
	}

	// a lucky craft gives the results twice (the player's crafting yield chance, from traits); a recipe that makes nothing can't be lucky
	const float YieldChance = Recipe.GetMainResult() ? UZAttributeSet::GetAttributeValue(GetOwner(), UZAttributeSet::GetCraftingYieldChanceAttribute(), 0.f) : 0.f;
	const int32 Times = YieldChance > 0.f && FMath::FRand() < YieldChance ? 2 : 1;
	NumLuckyCrafts += Times - 1;

	// the ingredients are gone first, so their room can hold the results; what still doesn't fit goes on the ground
	for (const FZItemStack& Result : Recipe.Results)
	{
		if (!Result.Item || Result.Count <= 0)
		{
			continue;
		}

		const int32 Count = Result.Count * Times;
		const int32 Leftover = Count - Inventory->AddItem(Result.Item, Count);
		if (Leftover > 0 && Inventory->DropOnGround(Result.Item, Leftover))
		{
			RequestNotes.Add(FText::Format(LOCTEXT("NoRoom", "No room for {0} x{1}, it's on the ground."), Result.Item->GetItemName(), Leftover));
		}
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' crafted '%s'"), *GetNameSafe(GetOwner()), *GetNameSafe(&Recipe));
}

void UZCraftingComponent::GiveCraftExperience(const UZCraftingRecipe& Recipe, bool bFirstDiscovery)
{
	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const float ExperienceMultiplier = UZAttributeSet::GetAttributeValue(GetOwner(), UZAttributeSet::GetTradeskillExperienceMultiplierAttribute(), 1.f);
	for (const FZSkillRequirement& Requirement : Recipe.SkillRequirements)
	{
		if (!Requirement.Skill.IsValid())
		{
			continue;
		}

		// easy recipes teach less and less the further above them the player is; traits can speed learning up
		const int32 Level = GetSkillLevel(Requirement.Skill);
		const float BaseExperience = Recipe.SkillExperience * Settings->GetTradeskillExperienceScale(Level, Requirement.MinLevel) + (bFirstDiscovery ? Recipe.DiscoveryExperience : 0);
		const int32 Experience = FMath::RoundToInt32(BaseExperience * ExperienceMultiplier);

		RequestStartLevels.FindOrAdd(Requirement.Skill, Level);
		RequestExperience.FindOrAdd(Requirement.Skill) += FMath::Max(Experience, 0);
		AddSkillExperience(Requirement.Skill, Experience);
	}
}

void UZCraftingComponent::StartCrafting(UZCraftingRecipe* Recipe, int32 Count, TConstArrayView<UZItemInstance*> PreferredItems)
{
	RequestItems.Reset();
	for (UZItemInstance* Item : PreferredItems)
	{
		RequestItems.Add(Item);
	}

	NumCrafted = 0;
	NumLuckyCrafts = 0;
	bRequestDiscovered = false;
	RequestNotes.Reset();
	RequestExperience.Reset();
	RequestStartLevels.Reset();

	// no crafting time: all at once
	if (Recipe->CraftTime <= 0.f)
	{
		EZCraftingResult Result = EZCraftingResult::Success;
		FText Reason;
		for (int32 Index = 0; Index < Count && Result == EZCraftingResult::Success; ++Index)
		{
			Result = CraftOnce(Recipe, Reason);
		}

		FinishCrafting(Recipe, Result, Reason);
		return;
	}

	CraftingProgress.Recipe = Recipe;
	CraftingProgress.RemainingCount = Count;
	StartWorkingCue(*Recipe);
	StartNextCraft();
}

void UZCraftingComponent::StartNextCraft()
{
	CraftingProgress.Duration = CraftingProgress.Recipe->CraftTime;
	CraftingProgress.StartTime = GetServerTime();
	GetWorld()->GetTimerManager().SetTimer(CraftTimer, this, &UZCraftingComponent::OnCraftTimerDone, CraftingProgress.Duration, false);

	// the owner's machine gets it through OnRep_CraftingProgress
	BroadcastProgressChanged();
}

void UZCraftingComponent::OnCraftTimerDone()
{
	UZCraftingRecipe* Recipe = CraftingProgress.Recipe;
	FText Reason;
	const EZCraftingResult Result = CraftOnce(Recipe, Reason);

	if (Result == EZCraftingResult::Success && --CraftingProgress.RemainingCount > 0)
	{
		StartNextCraft();
		return;
	}

	FinishCrafting(Recipe, Result, Reason);
}

EZCraftingResult UZCraftingComponent::CraftOnce(UZCraftingRecipe* Recipe, FText& OutReason)
{
	if (!Recipe)
	{
		return EZCraftingResult::Invalid;
	}

	// things may have changed while it was being made (items dropped, walked away from the station, died)
	if (!CanCraftNow())
	{
		return EZCraftingResult::CantCraftNow;
	}

	TArray<const UZItemInstance*> PreferredItems;
	for (const TWeakObjectPtr<UZItemInstance>& Item : RequestItems)
	{
		if (Item.IsValid())
		{
			PreferredItems.Add(Item.Get());
		}
	}

	FCraftPlan Plan;
	const EZCraftingResult Check = CheckRecipe(*Recipe, PreferredItems, Plan, OutReason);
	if (Check != EZCraftingResult::Success)
	{
		return Check;
	}

	ApplyCraftPlan(*Recipe, Plan);
	++NumCrafted;

	// found it: remembered, with extra experience the first time
	const bool bFirstDiscovery = !HasDiscovered(Recipe);
	if (bFirstDiscovery)
	{
		FZDiscoveredRecipe& Entry = Discoveries.Entries.AddDefaulted_GetRef();
		Entry.Recipe = Recipe;
		Discoveries.MarkItemDirty(Entry);
		bRequestDiscovered = true;

		UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' discovered the recipe '%s'"), *GetNameSafe(GetOwner()), *GetNameSafe(Recipe));
		OnRecipeDiscovered.Broadcast(this, Recipe);
	}

	GiveCraftExperience(*Recipe, bFirstDiscovery);
	return EZCraftingResult::Success;
}

void UZCraftingComponent::FinishCrafting(UZCraftingRecipe* Recipe, EZCraftingResult LastResult, const FText& Reason)
{
	GetWorld()->GetTimerManager().ClearTimer(CraftTimer);
	StopWorkingCue();

	if (IsCrafting())
	{
		CraftingProgress = FZCraftingProgress();
		BroadcastProgressChanged();
	}

	TArray<FText> MessageParts;
	EZCraftingResult Result = LastResult;
	const FText LastResultText = Reason.IsEmpty() ? GetResultText(LastResult, Recipe) : Reason;

	if (NumCrafted > 0 && Recipe)
	{
		// something was made, even if it stopped early
		Result = EZCraftingResult::Success;

		if (bRequestDiscovered)
		{
			MessageParts.Add(LOCTEXT("NewDiscovery", "New discovery!"));
		}

		// a recipe that only changes an item (an upgrade, a mod) says what it did in its notes instead
		if (Recipe->GetMainResult())
		{
			MessageParts.Add(FText::Format(LOCTEXT("CraftedResults", "Crafted {0}."), GetResultsText(*Recipe, NumCrafted + NumLuckyCrafts)));
			if (NumLuckyCrafts > 0)
			{
				MessageParts.Add(LOCTEXT("LuckyCraft", "Lucky: you made extra."));
			}
		}

		// what happened along the way: what the outcomes did, tools that broke, results that didn't fit
		MessageParts.Append(RequestNotes);

		if (LastResult != EZCraftingResult::Success)
		{
			MessageParts.Add(LastResult == EZCraftingResult::Cancelled ? LOCTEXT("Stopped", "Stopped.") : LastResultText);
		}

		// what the tradeskills got out of it
		bool bLearnedAnything = false;
		for (const TPair<FGameplayTag, int32>& Gained : RequestExperience)
		{
			if (Gained.Value <= 0)
			{
				continue;
			}

			bLearnedAnything = true;
			MessageParts.Add(FText::Format(LOCTEXT("SkillGained", "{0} +{1}."), GetSkillName(Gained.Key), Gained.Value));

			const int32 NewLevel = GetSkillLevel(Gained.Key);
			if (NewLevel > RequestStartLevels.FindRef(Gained.Key))
			{
				MessageParts.Add(FText::Format(LOCTEXT("SkillLevelUp", "{0} is now level {1}!"), GetSkillName(Gained.Key), NewLevel));
			}
		}

		if (!bLearnedAnything && Recipe->SkillRequirements.Num() > 0)
		{
			MessageParts.Add(LOCTEXT("NothingToLearn", "There's nothing more to learn from making this."));
		}
	}
	else
	{
		MessageParts.Add(LastResultText);
	}

	// the recipe's done cue, e.g. a finishing clank, for everyone near the player
	if (NumCrafted > 0 && Recipe)
	{
		FGameplayCueParameters DoneCue;
		DoneCue.SourceObject = Recipe;
		DoneCue.RawMagnitude = NumCrafted;
		UZGameplayCueLibrary::ExecuteCue(GetOwner(), Recipe->DoneCue, DoneCue);
	}

	const bool bNewDiscovery = NumCrafted > 0 && bRequestDiscovered;

	RequestItems.Reset();
	RequestNotes.Reset();
	RequestExperience.Reset();
	RequestStartLevels.Reset();
	bRequestDiscovered = false;
	NumCrafted = 0;
	NumLuckyCrafts = 0;

	SendResult(Result, Recipe, FText::Join(FText::FromString(TEXT(" ")), MessageParts), bNewDiscovery);
}

void UZCraftingComponent::SendResult(EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message, bool bNewDiscovery)
{
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("Crafting for '%s': %s (%s) %s"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Result), *GetNameSafe(Recipe), *Message.ToString());

	// for the host's own player this runs right here
	ClientCraftingResult(Result, Recipe, Message, bNewDiscovery);
}

void UZCraftingComponent::StartWorkingCue(const UZCraftingRecipe& Recipe)
{
	StopWorkingCue();

	// at the station the recipe needs (e.g. hammering at the workbench), otherwise on the player
	FGameplayCueParameters WorkingCue;
	WorkingCue.SourceObject = &Recipe;
	if (AZCraftingStation* Station = FindNearbyStation(Recipe.RequiredStation))
	{
		WorkingCue.Location = Station->GetActorLocation();
		WorkingCue.EffectCauser = Station;
	}

	// a lasting cue, so players who come near while it's being made hear it too
	if (UZGameplayCueLibrary::AddCue(GetOwner(), Recipe.WorkingCue, WorkingCue))
	{
		ActiveWorkingCue = Recipe.WorkingCue;
	}
}

void UZCraftingComponent::StopWorkingCue()
{
	if (ActiveWorkingCue.IsValid())
	{
		UZGameplayCueLibrary::RemoveCue(GetOwner(), ActiveWorkingCue);
		ActiveWorkingCue = FGameplayTag();
	}
}

bool UZCraftingComponent::CanCraftNow() const
{
	const APawn* Pawn = GetPawn();
	return Pawn && !UZCombatLibrary::IsDead(Pawn);
}

int32 UZCraftingComponent::GetUsableToolCount(const FZRecipeInput& Tool) const
{
	const UZInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FZInventoryItem& Stack : Inventory->GetItems())
	{
		if (Stack.Instance && Tool.Matches(Stack.Instance->GetItemDefinition()) && !Stack.Instance->IsBroken())
		{
			Count += Stack.Count;
		}
	}

	return Count;
}

void UZCraftingComponent::BroadcastProgressChanged()
{
	OnCraftingProgressChanged.Broadcast(this);
}

double UZCraftingComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

UZInventoryComponent* UZCraftingComponent::GetInventory() const
{
	return UZInventoryComponent::FindInventoryComponent(GetOwner());
}

APawn* UZCraftingComponent::GetPawn() const
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		return Pawn;
	}

	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	return PlayerState ? PlayerState->GetPawn() : nullptr;
}

FString UZCraftingComponent::GetDebugString() const
{
	TArray<FString> Skills;
	for (const FGameplayTag& Skill : GetAllTradeskills())
	{
		Skills.Add(FString::Printf(TEXT("%s %d (%d xp)"), *GetSkillName(Skill).ToString(), GetSkillLevel(Skill), GetSkillExperience(Skill)));
	}

	TArray<FString> Found;
	for (const FZDiscoveredRecipe& Entry : Discoveries.Entries)
	{
		Found.Add(GetNameSafe(Entry.Recipe));
	}

	const FString Crafting = IsCrafting()
		? FString::Printf(TEXT("%s (%d left, %.0f%%)"), *GetNameSafe(CraftingProgress.Recipe), CraftingProgress.RemainingCount, GetCraftingProgressPercent() * 100.f)
		: FString(TEXT("nothing"));

	return FString::Printf(TEXT("%s; found %d of %d recipes (%s); crafting: %s"), *FString::Join(Skills, TEXT(", ")), GetNumDiscovered(), GetNumRecipesInGame(),
		Found.Num() > 0 ? *FString::Join(Found, TEXT(", ")) : TEXT("none"), *Crafting);
}

#undef LOCTEXT_NAMESPACE
