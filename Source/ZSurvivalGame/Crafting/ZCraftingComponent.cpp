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
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZCraftingStation.h"
#include "Crafting/ZCraftingSubsystem.h"
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
		return Recipe ? FText::Format(LOCTEXT("Crafted", "Crafted {0}."), GetResultsText(*Recipe, 1)) : FText::GetEmpty();

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

	for (UZCraftingRecipe* Recipe : Subsystem->FindRecipesForCombination(ChosenKinds))
	{
		const EZCraftingResult Result = CheckRecipe(*Recipe, PreferredItems);
		if (Result == EZCraftingResult::Success)
		{
			StartCrafting(Recipe, FMath::Clamp(Count, 1, MaxCraftsPerRequest), ChosenItems);
			return;
		}

		if (GetFailurePriority(Result) > GetFailurePriority(Failure))
		{
			Failure = Result;
			FailedRecipe = Recipe;
		}
	}

	SendResult(Failure, FailedRecipe, GetResultText(Failure, FailedRecipe));
}

void UZCraftingComponent::ServerCancelCrafting_Implementation()
{
	if (IsCrafting())
	{
		FinishCrafting(CraftingProgress.Recipe, EZCraftingResult::Cancelled);
	}
}

void UZCraftingComponent::ClientCraftingResult_Implementation(EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message)
{
	OnCraftingResult.Broadcast(this, Result, Recipe, Message);
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

EZCraftingResult UZCraftingComponent::CheckRecipe(const UZCraftingRecipe& Recipe, TConstArrayView<const UZItemInstance*> PreferredItems) const
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

	FCraftPlan Plan;
	return BuildCraftPlan(Recipe, PreferredItems, Plan) ? EZCraftingResult::Success : EZCraftingResult::MissingItems;
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

		if (!Input.IsTool())
		{
			OutPlan.ToRemove.Append(Used);
		}
		else if (Input.DurabilityCost > 0)
		{
			for (const TPair<UZItemInstance*, int32>& UsedTool : Used)
			{
				if (UsedTool.Key->HasDurability())
				{
					OutPlan.ToWear.Emplace(UsedTool.Key, Input.DurabilityCost);
				}
			}
		}
	}

	return bHasEverything;
}

void UZCraftingComponent::ApplyCraftPlan(const UZCraftingRecipe& Recipe, const FCraftPlan& Plan)
{
	UZInventoryComponent* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}

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

	// the ingredients are gone first, so their room can hold the results; what still doesn't fit goes on the ground
	for (const FZItemStack& Result : Recipe.Results)
	{
		if (!Result.Item || Result.Count <= 0)
		{
			continue;
		}

		const int32 Leftover = Result.Count - Inventory->AddItem(Result.Item, Result.Count);
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
	for (const FZSkillRequirement& Requirement : Recipe.SkillRequirements)
	{
		if (!Requirement.Skill.IsValid())
		{
			continue;
		}

		// easy recipes teach less and less the further above them the player is
		const int32 Level = GetSkillLevel(Requirement.Skill);
		int32 Experience = FMath::RoundToInt32(Recipe.SkillExperience * Settings->GetTradeskillExperienceScale(Level, Requirement.MinLevel));
		if (bFirstDiscovery)
		{
			Experience += Recipe.DiscoveryExperience;
		}

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
	bRequestDiscovered = false;
	RequestNotes.Reset();
	RequestExperience.Reset();
	RequestStartLevels.Reset();

	// no crafting time: all at once
	if (Recipe->CraftTime <= 0.f)
	{
		EZCraftingResult Result = EZCraftingResult::Success;
		for (int32 Index = 0; Index < Count && Result == EZCraftingResult::Success; ++Index)
		{
			Result = CraftOnce(Recipe);
		}

		FinishCrafting(Recipe, Result);
		return;
	}

	CraftingProgress.Recipe = Recipe;
	CraftingProgress.RemainingCount = Count;
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
	const EZCraftingResult Result = CraftOnce(Recipe);

	if (Result == EZCraftingResult::Success && --CraftingProgress.RemainingCount > 0)
	{
		StartNextCraft();
		return;
	}

	FinishCrafting(Recipe, Result);
}

EZCraftingResult UZCraftingComponent::CraftOnce(UZCraftingRecipe* Recipe)
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

	const EZCraftingResult Check = CheckRecipe(*Recipe, PreferredItems);
	FCraftPlan Plan;
	if (Check != EZCraftingResult::Success || !BuildCraftPlan(*Recipe, PreferredItems, Plan))
	{
		return Check != EZCraftingResult::Success ? Check : EZCraftingResult::MissingItems;
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

void UZCraftingComponent::FinishCrafting(UZCraftingRecipe* Recipe, EZCraftingResult LastResult)
{
	GetWorld()->GetTimerManager().ClearTimer(CraftTimer);

	if (IsCrafting())
	{
		CraftingProgress = FZCraftingProgress();
		BroadcastProgressChanged();
	}

	TArray<FText> MessageParts;
	EZCraftingResult Result = LastResult;

	if (NumCrafted > 0 && Recipe)
	{
		// something was made, even if it stopped early
		Result = EZCraftingResult::Success;

		if (bRequestDiscovered)
		{
			MessageParts.Add(LOCTEXT("NewDiscovery", "New discovery!"));
		}

		MessageParts.Add(FText::Format(LOCTEXT("CraftedResults", "Crafted {0}."), GetResultsText(*Recipe, NumCrafted)));

		if (LastResult != EZCraftingResult::Success)
		{
			MessageParts.Add(LastResult == EZCraftingResult::Cancelled ? LOCTEXT("Stopped", "Stopped.") : GetResultText(LastResult, Recipe));
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
		MessageParts.Add(GetResultText(LastResult, Recipe));
	}

	MessageParts.Append(RequestNotes);

	RequestItems.Reset();
	RequestNotes.Reset();
	RequestExperience.Reset();
	RequestStartLevels.Reset();
	bRequestDiscovered = false;
	NumCrafted = 0;

	SendResult(Result, Recipe, FText::Join(FText::FromString(TEXT(" ")), MessageParts));
}

void UZCraftingComponent::SendResult(EZCraftingResult Result, UZCraftingRecipe* Recipe, const FText& Message)
{
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("Crafting for '%s': %s (%s) %s"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Result), *GetNameSafe(Recipe), *Message.ToString());

	// for the host's own player this runs right here
	ClientCraftingResult(Result, Recipe, Message);
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
