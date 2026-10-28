// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveGame/ZPlayerSaveComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameCharacter.h"
#include "ZSurvivalGamePlayerController.h"
#include "ZSurvivalGamePlayerState.h"
#include "ZSurvivalGameState.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Crafting/ZCraftingComponent.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZCraftingSubsystem.h"
#include "Items/ZEquipmentComponent.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemFragments.h"
#include "Items/ZItemInstance.h"
#include "Progression/ZProgressionComponent.h"
#include "Progression/ZTraitDefinition.h"
#include "SaveGame/ZSaveSettings.h"
#include "SaveGame/ZSaveSubsystem.h"
#include "Weapons/ZWeapon.h"
#include "Weapons/ZWeaponData.h"

#define LOCTEXT_NAMESPACE "ZSave"

namespace
{
	/** Stacks past this many in a save are left out, so a huge save can't flood the game (and the network) */
	constexpr int32 MaxSavedStacks = 500;

	/** As many recipes the game doesn't know are kept */
	constexpr int32 MaxUnknownRecipes = 500;

	/** A player asking to save again within this many seconds is ignored */
	constexpr double MinSecondsBetweenRequestedSaves = 2.0;

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<bool> CVarTestDontSendCharacter(TEXT("Z.Save.TestDontSendCharacter"), false,
		TEXT("For testing the host's wait: a joining player doesn't send their character, so the host gives up and they play a new one that isn't saved"));
#endif

	/** A data asset by its primary asset id (a renamed one through the Asset Manager's redirects), or null if the game doesn't have it */
	template <typename AssetType>
	AssetType* LoadPrimaryAsset(const FPrimaryAssetId& AssetId, const FPrimaryAssetType& ExpectedType)
	{
		if (!AssetId.IsValid() || AssetId.PrimaryAssetType != ExpectedType)
		{
			return nullptr;
		}

		// only assets the Asset Manager knows, so a save can't make the game load anything else
		const UAssetManager& AssetManager = UAssetManager::Get();
		FSoftObjectPath Path = AssetManager.GetPrimaryAssetPath(AssetId);
		if (Path.IsNull())
		{
			const FPrimaryAssetId Redirected = AssetManager.GetRedirectedPrimaryAssetId(AssetId);
			if (Redirected.IsValid() && Redirected.PrimaryAssetType == ExpectedType)
			{
				Path = AssetManager.GetPrimaryAssetPath(Redirected);
			}
		}

		return Path.IsNull() ? nullptr : Cast<AssetType>(Path.TryLoad());
	}
}

UZPlayerSaveComponent::UZPlayerSaveComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// for its RPCs (it has no properties to replicate)
	SetIsReplicatedByDefault(true);
}

void UZPlayerSaveComponent::BeginPlay()
{
	Super::BeginPlay();

	// the last save happens when the world starts closing: later (EndPlay), a level change has already destroyed the PlayerState
	const APlayerController* PlayerController = GetPlayerController();
	const bool bJoinedPlayer = PlayerController && PlayerController->IsLocalController() && !GetOwner()->HasAuthority();
	if (GetOwner()->HasAuthority() || bJoinedPlayer)
	{
		WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UZPlayerSaveComponent::HandleWorldTearDown);
	}

	// a joined player's machine also saves when the host ends the game, before the host's actors are removed here
	if (bJoinedPlayer && GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UZPlayerSaveComponent::HandleNetworkFailure);
	}

	// a joining player sends their character as soon as their controller is here (the host's own is taken by the game mode)
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(this);
	if (bJoinedPlayer && !bCharacterSent && Saves)
	{
		bCharacterSent = true;

#if !UE_BUILD_SHIPPING
		if (CVarTestDontSendCharacter.GetValueOnGameThread())
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: not sending the character (Z.Save.TestDontSendCharacter)"));
			return;
		}
#endif

		const FZCharacterSaveData& Character = Saves->GetActiveCharacter();
		SentCharacterId = Character.CharacterId;
		UE_LOG(LogZSurvivalGame, Log, TEXT("Save: sending the character '%s' to the host"), *Character.CharacterName);
		ServerReceiveCharacter(Character);
	}
}

void UZPlayerSaveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// changes the world teardown didn't catch (e.g. a player leaving), if the PlayerState is still there
	if (GetOwner()->HasAuthority() && bDirty)
	{
		SaveCharacter(true);
	}

	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}

	UnbindFromChanges();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SaveTimer);
		World->GetTimerManager().ClearTimer(WaitTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UZPlayerSaveComponent::LoadLocalCharacter()
{
	if (!GetOwner()->HasAuthority() || bCharacterReady)
	{
		return;
	}

	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(this);
	if (!Saves)
	{
		// can't happen in a game, but the player should still get a character
		CharacterId = FGuid::NewGuid();
		FinishLoading();
		return;
	}

	// on the server this runs right away
	bCharacterSent = true;
	ServerReceiveCharacter(Saves->GetActiveCharacter());
}

void UZPlayerSaveComponent::WaitForCharacter()
{
	if (bCharacterReady)
	{
		OnCharacterReady.Broadcast(this);
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(WaitTimer, this, &UZPlayerSaveComponent::HandleWaitTimedOut, UZSaveSettings::Get()->CharacterArrivalTimeout, false);
}

void UZPlayerSaveComponent::ServerReceiveCharacter_Implementation(const FZCharacterSaveData& Character)
{
	// only the first one counts: sending an older save again mustn't bring back what was given away since
	if (bCharacterReady)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: '%s' sent a character again; only the first one counts"), *GetNameSafe(GetOwner()));
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(WaitTimer);
	ApplyCharacter(Character);
	bSaveProgress = true;
	FinishLoading();
}

void UZPlayerSaveComponent::HandleWaitTimedOut()
{
	if (bCharacterReady)
	{
		return;
	}

	UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: '%s''s character didn't arrive within %.0f s: they play a new character, which isn't saved (their save stays as it was)"),
		*GetNameSafe(GetOwner()), UZSaveSettings::Get()->CharacterArrivalTimeout);

	CharacterId = FGuid::NewGuid();
	CharacterName = UZSaveSettings::Get()->DefaultCharacterName;
	bSaveProgress = false;
	FinishLoading();

	if (AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOwner()))
	{
		PlayerController->ClientShowMessage(LOCTEXT("CharacterLate", "Your character didn't reach the host in time, so you're playing a new one. It isn't saved; your saved character is unchanged."));
	}
}

void UZPlayerSaveComponent::HandleWorldTearDown(UWorld* World)
{
	if (World != GetWorld())
	{
		return;
	}

	// a joined player's own machine saves what it has (the host's last save may not reach it any more)
	if (!GetOwner()->HasAuthority())
	{
		SaveOnOwner();
		return;
	}

	// the host's own is always written (with the time played); a joined player's latest changes are sent if their connection can still take them
	const APlayerController* PlayerController = GetPlayerController();
	if (bDirty || (PlayerController && PlayerController->IsLocalController()))
	{
		SaveCharacter(true);
	}
}

void UZPlayerSaveComponent::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (World == GetWorld())
	{
		SaveOnOwner();
	}
}

void UZPlayerSaveComponent::SaveOnOwner()
{
	const APlayerController* PlayerController = GetPlayerController();
	const AZSurvivalGamePlayerState* PlayerState = GetZPlayerState();
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(this);
	if (bSavedOnLeaving || GetOwner()->HasAuthority() || !PlayerController || !PlayerController->IsLocalController() || !Saves
		|| !IsValid(PlayerState) || PlayerState->IsActorBeingDestroyed())
	{
		return;
	}

	// only the character this save sent and the host loaded (not a new one played because it arrived too late), once all of it is here
	const FZCharacterSaveData& Saved = Saves->GetActiveCharacter();
	if (!SentCharacterId.IsValid() || Saved.CharacterId != SentCharacterId || !PlayerState->GetProgression()->WasRestored() || !PlayerState->GetInventory()->HaveAllItemsArrived()
		|| !PlayerState->GetEquipment()->HaveAllItemsArrived())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Save: not saved on leaving: the host hasn't loaded this slot's character (yet)"));
		return;
	}

	bSavedOnLeaving = true;

	// the character as the host last sent it to this machine, and what only the save knows: which character it is, and things this game doesn't have
	FZCharacterSaveData Character = BuildCharacterData();
	Character.CharacterId = Saved.CharacterId;
	Character.CharacterName = Saved.CharacterName;

	for (const FZSavedItem& Item : Saved.Inventory)
	{
		if (!IsKnownAsset(Item.Item))
		{
			Character.Inventory.Add(Item);
		}
	}

	for (const FPrimaryAssetId& Recipe : Saved.DiscoveredRecipes)
	{
		if (!IsKnownAsset(Recipe))
		{
			Character.DiscoveredRecipes.AddUnique(Recipe);
		}
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Save: saving '%s' on leaving the host's game (%d stacks, %d xp)"), *Character.CharacterName, Character.Inventory.Num(), Character.Experience);
	Saves->WriteActiveSlot(Character, nullptr, true);
}

bool UZPlayerSaveComponent::IsKnownAsset(const FPrimaryAssetId& AssetId)
{
	const UAssetManager& AssetManager = UAssetManager::Get();
	return AssetManager.GetPrimaryAssetPath(AssetId).IsValid() || AssetManager.GetRedirectedPrimaryAssetId(AssetId).IsValid();
}

void UZPlayerSaveComponent::FinishLoading()
{
	bCharacterReady = true;
	BindToChanges();
	OnCharacterReady.Broadcast(this);
}

void UZPlayerSaveComponent::ApplyCharacter(const FZCharacterSaveData& Character)
{
	AZSurvivalGamePlayerState* PlayerState = GetZPlayerState();
	if (!PlayerState)
	{
		UE_LOG(LogZSurvivalGame, Error, TEXT("Save: '%s' has no AZSurvivalGamePlayerState, so their character can't be loaded. Check the game mode's Player State Class."), *GetNameSafe(GetOwner()));
		return;
	}

	// everything that doesn't fit the game's rules is changed to fit and listed for the log; what the game doesn't have is kept in the save, and listed too
	TArray<FString> Fixes;
	TArray<FString> Kept;

	CharacterId = Character.CharacterId.IsValid() ? Character.CharacterId : FGuid::NewGuid();
	CharacterName = Character.CharacterName.TrimStartAndEnd().Left(ZMaxCharacterNameLength);
	if (CharacterName.IsEmpty())
	{
		CharacterName = UZSaveSettings::Get()->DefaultCharacterName;
	}

	UnknownItems.Reset();
	UnknownRecipes.Reset();

	// --- INVENTORY: the saved stacks as they were, each item checked by its fragments ---

	// a new player has nothing yet; anything already there makes way
	UZInventoryComponent* Inventory = PlayerState->GetInventory();
	for (const FZInventoryItem& Stack : Inventory->GetItems())
	{
		Inventory->RemoveItemInstance(Stack.Instance, Stack.Count);
	}

	const UZCraftingSubsystem* CraftingSubsystem = UZCraftingSubsystem::Get(this);
	UZItemInstance* EquippedItem = nullptr;
	TArray<TPair<UZItemInstance*, FZEquipmentSlot>> SavedEquipment;
	int32 NumStacks = 0;

	for (const FZSavedItem& Saved : Character.Inventory)
	{
		UZItemDefinition* Item = LoadPrimaryAsset<UZItemDefinition>(Saved.Item, UZItemDefinition::GetItemAssetType());
		if (!Item)
		{
			// kept in the save, so it comes back if the item does (e.g. renamed without a redirect)
			if (UnknownItems.Num() < MaxSavedStacks)
			{
				UnknownItems.Add(Saved);
				Kept.Add(FString::Printf(TEXT("item '%s' x%d"), *Saved.Item.ToString(), Saved.Count));
			}
			else
			{
				Fixes.Add(FString::Printf(TEXT("the game has no item '%s', and the save has too many of those to keep"), *Saved.Item.ToString()));
			}

			continue;
		}

		if (NumStacks >= MaxSavedStacks)
		{
			Fixes.Add(FString::Printf(TEXT("more than %d stacks: '%s' x%d left out"), MaxSavedStacks, *GetNameSafe(Item), Saved.Count));
			continue;
		}

		const int32 Count = FMath::Clamp(Saved.Count, 1, Item->MaxStackSize);
		if (Count != Saved.Count)
		{
			Fixes.Add(FString::Printf(TEXT("'%s' x%d is outside 1..%d per stack"), *GetNameSafe(Item), Saved.Count, Item->MaxStackSize));
		}

		UZItemInstance* Instance = Inventory->AddStack(Item, Count);
		if (!Instance)
		{
			UnknownItems.Add(Saved);
			Kept.Add(FString::Printf(TEXT("'%s' x%d, which there's no room for"), *GetNameSafe(Item), Count));
			continue;
		}

		++NumStacks;

		TMap<FGameplayTag, int32> Stats;
		for (const FZSavedItemStat& Stat : Saved.Stats)
		{
			if (Stat.Tag.MatchesTag(ZGameplayTags::Item_Stat) && Stat.Value >= 0)
			{
				Stats.Add(Stat.Tag, Stat.Value);
			}
			else
			{
				Fixes.Add(FString::Printf(TEXT("'%s' stat '%s' %d left out"), *GetNameSafe(Item), *Stat.Tag.ToString(), Stat.Value));
			}
		}

		TArray<UZItemDefinition*> Mods;
		for (const FPrimaryAssetId& ModId : Saved.Mods)
		{
			if (UZItemDefinition* Mod = LoadPrimaryAsset<UZItemDefinition>(ModId, UZItemDefinition::GetItemAssetType()))
			{
				Mods.Add(Mod);
			}
			else
			{
				Fixes.Add(FString::Printf(TEXT("the game has no mod '%s' (taken off '%s')"), *ModId.ToString(), *GetNameSafe(Item)));
			}
		}

		// its fragments keep their own stats within limits: durability, magazine ammo, mods that fit
		Instance->RestoreState(Stats, Mods);

		// upgrades only as high as an upgrade recipe can take this item
		const UZItemFragment_Weapon* Weapon = Instance->FindFragment<UZItemFragment_Weapon>();
		const int32 WeaponMaxUpgrade = Weapon && Weapon->WeaponData ? Weapon->WeaponData->MaxUpgradeLevel : 0;
		const int32 MaxUpgradeLevel = FMath::Max(WeaponMaxUpgrade, CraftingSubsystem ? CraftingSubsystem->GetHighestUpgradeLevel(Instance, ZGameplayTags::Item_Stat_UpgradeLevel) : 0);
		if (Instance->GetUpgradeLevel() > MaxUpgradeLevel)
		{
			Fixes.Add(FString::Printf(TEXT("'%s' upgrade +%d is more than +%d"), *GetNameSafe(Item), Instance->GetUpgradeLevel(), MaxUpgradeLevel));
			Instance->SetStatTagStack(ZGameplayTags::Item_Stat_UpgradeLevel, MaxUpgradeLevel);
		}

		if (Saved.bEquipped && Weapon && !EquippedItem)
		{
			EquippedItem = Instance;
		}

		// the slot it was in (a save from before there was equipment only knows the weapon in the hands: it goes in its slot)
		if (Saved.EquipmentSlot.IsValid() || (Saved.bEquipped && Weapon))
		{
			SavedEquipment.Emplace(Instance, FZEquipmentSlot(Saved.EquipmentSlot, Saved.EquipmentIndex));
		}
	}

	// --- EQUIPMENT: each item back in its slot, if it still goes there ---

	PlayerState->GetEquipment()->RestoreEquipment(SavedEquipment, Fixes);

	// the weapon that was in the hands comes out when the character spawns
	PlayerState->PendingEquipItem = EquippedItem;

	// --- PROGRESSION ---

	TArray<TPair<UZTraitDefinition*, int32>> TraitRanks;
	for (const FZSavedTraitRank& Saved : Character.Traits)
	{
		if (UZTraitDefinition* Trait = LoadPrimaryAsset<UZTraitDefinition>(Saved.Trait, UZTraitDefinition::GetTraitAssetType()))
		{
			TraitRanks.Emplace(Trait, Saved.Rank);
		}
		else
		{
			Fixes.Add(FString::Printf(TEXT("the game has no trait '%s' (its points can be spent again)"), *Saved.Trait.ToString()));
		}
	}

	UZProgressionComponent* Progression = PlayerState->GetProgression();
	Progression->RestoreProgress(Character.Experience, Character.BonusTraitPoints, TraitRanks, Fixes);

	// --- CRAFTING ---

	TArray<TPair<FGameplayTag, int32>> SkillExperience;
	for (const FZSavedTradeskill& Saved : Character.Tradeskills)
	{
		SkillExperience.Emplace(Saved.Skill, Saved.Experience);
	}

	TArray<UZCraftingRecipe*> Recipes;
	for (const FPrimaryAssetId& RecipeId : Character.DiscoveredRecipes)
	{
		if (UZCraftingRecipe* Recipe = LoadPrimaryAsset<UZCraftingRecipe>(RecipeId, UZCraftingRecipe::GetRecipeAssetType()))
		{
			Recipes.Add(Recipe);
		}
		else if (UnknownRecipes.Num() < MaxUnknownRecipes)
		{
			UnknownRecipes.AddUnique(RecipeId);
			Kept.Add(FString::Printf(TEXT("recipe '%s'"), *RecipeId.ToString()));
		}
	}

	PlayerState->GetCrafting()->RestoreCrafting(SkillExperience, Recipes, Fixes);

	// a character that got its starting items once doesn't get them again
	PlayerState->bReceivedStartingItems = Character.bReceivedStartingItems;

	UE_LOG(LogZSurvivalGame, Log, TEXT("Save: '%s' plays '%s': level %d, %d stacks, %d traits, %d recipes found"), *GetNameSafe(GetOwner()), *CharacterName,
		Progression->GetLevel(), NumStacks, Progression->GetTraitRanks().Num(), Recipes.Num());

	// e.g. an item renamed without a redirect (see SAVING.md); it comes back when the game has it again
	UE_CLOG(Kept.Num() > 0, LogZSurvivalGame, Warning, TEXT("Save: '%s''s save has things this game doesn't, kept in the save as they are: %s"),
		*GetNameSafe(GetOwner()), *FString::Join(Kept, TEXT("; ")));

	if (Fixes.Num() > 0)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: '%s''s character didn't fit the game's rules and was changed: %s"), *GetNameSafe(GetOwner()), *FString::Join(Fixes, TEXT("; ")));

		if (AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOwner()))
		{
			PlayerController->ClientShowMessage(LOCTEXT("CharacterFixed", "Some of your character didn't fit the game's rules and was changed (the host's log lists what)."));
		}
	}
}

FZCharacterSaveData UZPlayerSaveComponent::BuildCharacterData() const
{
	FZCharacterSaveData Character;
	Character.CharacterId = CharacterId;
	Character.CharacterName = CharacterName;

	const AZSurvivalGamePlayerState* PlayerState = GetZPlayerState();
	if (!PlayerState)
	{
		return Character;
	}

	Character.bReceivedStartingItems = PlayerState->bReceivedStartingItems;

	// the weapon in the hands keeps its magazine in the weapon until it's put away (before the character spawns: the one it will take out)
	const AZSurvivalGameCharacter* Pawn = Cast<AZSurvivalGameCharacter>(PlayerState->GetPawn());
	const UZItemInstance* EquippedItem = Pawn ? Pawn->GetEquippedItem() : PlayerState->PendingEquipItem.Get();
	const AZWeapon* EquippedWeapon = Pawn ? Pawn->GetEquippedWeapon() : nullptr;
	const UZEquipmentComponent* Equipment = PlayerState->GetEquipment();

	for (const FZInventoryItem& Stack : PlayerState->GetInventory()->GetItems())
	{
		const UZItemDefinition* Item = Stack.Instance ? Stack.Instance->GetItemDefinition() : nullptr;
		if (!Item)
		{
			continue;
		}

		FZSavedItem& Saved = Character.Inventory.AddDefaulted_GetRef();
		Saved.Item = Item->GetPrimaryAssetId();
		Saved.Count = Stack.Count;
		Saved.bEquipped = Stack.Instance == EquippedItem;

		const FZEquipmentSlot EquipmentSlot = Equipment->GetSlotOfItem(Stack.Instance);
		Saved.EquipmentSlot = EquipmentSlot.Type;
		Saved.EquipmentIndex = EquipmentSlot.IsValid() ? EquipmentSlot.Index : 0;

		TMap<FGameplayTag, int32> Stats = Stack.Instance->GetStats();
		if (Saved.bEquipped && EquippedWeapon && EquippedWeapon->GetItemInstance() == Stack.Instance)
		{
			Stats.Add(ZGameplayTags::Item_Stat_MagazineAmmo, EquippedWeapon->GetAmmo());
		}

		for (const TPair<FGameplayTag, int32>& Stat : Stats)
		{
			if (Stat.Value > 0)
			{
				FZSavedItemStat& SavedStat = Saved.Stats.AddDefaulted_GetRef();
				SavedStat.Tag = Stat.Key;
				SavedStat.Value = Stat.Value;
			}
		}

		for (const UZItemDefinition* Mod : Stack.Instance->GetAttachedMods())
		{
			Saved.Mods.Add(Mod->GetPrimaryAssetId());
		}
	}

	Character.Inventory.Append(UnknownItems);

	const UZProgressionComponent* Progression = PlayerState->GetProgression();
	Character.Experience = Progression->GetExperience();
	Character.BonusTraitPoints = Progression->GetBonusTraitPoints();
	for (const TPair<UZTraitDefinition*, int32>& Rank : Progression->GetTraitRanks())
	{
		FZSavedTraitRank& SavedRank = Character.Traits.AddDefaulted_GetRef();
		SavedRank.Trait = Rank.Key->GetPrimaryAssetId();
		SavedRank.Rank = Rank.Value;
	}

	const UZCraftingComponent* Crafting = PlayerState->GetCrafting();
	for (const TPair<FGameplayTag, int32>& Skill : Crafting->GetAllSkillExperience())
	{
		FZSavedTradeskill& SavedSkill = Character.Tradeskills.AddDefaulted_GetRef();
		SavedSkill.Skill = Skill.Key;
		SavedSkill.Experience = Skill.Value;
	}

	for (const UZCraftingRecipe* Recipe : Crafting->GetDiscoveredRecipes())
	{
		Character.DiscoveredRecipes.Add(Recipe->GetPrimaryAssetId());
	}

	Character.DiscoveredRecipes.Append(UnknownRecipes);
	return Character;
}

void UZPlayerSaveComponent::RequestSave(bool bWaitUntilWritten)
{
	// the host saves the whole game; a joined player only has their own character to save
	if (GetOwner()->HasAuthority())
	{
		SaveAllPlayers(GetWorld(), bWaitUntilWritten);
	}
	else if (bWaitUntilWritten)
	{
		// e.g. switching slots: the host's answer would come too late
		SaveOnOwner();
	}
	else
	{
		ServerRequestSave();
	}
}

void UZPlayerSaveComponent::SaveAllPlayers(const UWorld* World, bool bWaitUntilWritten)
{
	if (!World)
	{
		return;
	}

	for (TActorIterator<APlayerController> It(World); It; ++It)
	{
		if (UZPlayerSaveComponent* PlayerSave = It->FindComponentByClass<UZPlayerSaveComponent>())
		{
			PlayerSave->SaveCharacter(bWaitUntilWritten);
		}
	}
}

void UZPlayerSaveComponent::ServerRequestSave_Implementation()
{
	const double Now = GetWorld()->GetRealTimeSeconds();
	if (Now - LastRequestedSaveTime >= MinSecondsBetweenRequestedSaves)
	{
		LastRequestedSaveTime = Now;
		SaveCharacter();
	}
}

void UZPlayerSaveComponent::SaveCharacter(bool bWaitUntilWritten)
{
	if (!GetOwner()->HasAuthority() || !bCharacterReady || !bSaveProgress)
	{
		return;
	}

	// never a character built without its PlayerState (e.g. destroyed by a level change): it would be empty, and replace the real one
	const AZSurvivalGamePlayerState* PlayerState = GetZPlayerState();
	if (!IsValid(PlayerState) || PlayerState->IsActorBeingDestroyed())
	{
		// a joined player who left saves on their own machine (SaveOnOwner); for the host's own player this would be a problem
		const APlayerController* PlayerController = GetPlayerController();
		if (PlayerController && PlayerController->IsLocalController())
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("Save: '%s' not saved: their PlayerState is gone"), *GetNameSafe(GetOwner()));
		}
		else
		{
			UE_LOG(LogZSurvivalGame, Log, TEXT("Save: '%s' left; their own machine saved their character"), *GetNameSafe(GetOwner()));
		}

		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(SaveTimer);
	bDirty = false;

	const FZCharacterSaveData Character = BuildCharacterData();
	const APlayerController* PlayerController = GetPlayerController();
	if (PlayerController && PlayerController->IsLocalController())
	{
		// the host's own save holds the world too
		FZWorldSaveData World;
		if (const AZSurvivalGameState* GameState = GetWorld()->GetGameState<AZSurvivalGameState>())
		{
			World.Progress = GameState->GetWorldProgress();
		}

		World.MapName = UWorld::RemovePIEPrefix(GetWorld()->GetPackage()->GetName());

		if (UZSaveSubsystem* Saves = UZSaveSubsystem::Get(this))
		{
			Saves->WriteActiveSlot(Character, &World, bWaitUntilWritten);
		}
	}
	else
	{
		// the player's own machine writes it into their slot
		ClientWriteCharacter(Character);
	}

	UE_LOG(LogZSurvivalGame, Verbose, TEXT("Save: saved '%s' ('%s', %d stacks, %d xp)"), *GetNameSafe(GetOwner()), *Character.CharacterName, Character.Inventory.Num(), Character.Experience);
}

void UZPlayerSaveComponent::ClientWriteCharacter_Implementation(const FZCharacterSaveData& Character)
{
	if (UZSaveSubsystem* Saves = UZSaveSubsystem::Get(this))
	{
		Saves->WriteActiveSlot(Character, nullptr);
	}
}

void UZPlayerSaveComponent::MarkDirty()
{
	if (!GetOwner()->HasAuthority() || !bCharacterReady || !bSaveProgress)
	{
		return;
	}

	bDirty = true;

	// the first change starts the wait; changes until then are saved with it
	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (!TimerManager.IsTimerActive(SaveTimer))
	{
		TimerManager.SetTimer(SaveTimer, FTimerDelegate::CreateUObject(this, &UZPlayerSaveComponent::SaveCharacter, false), UZSaveSettings::Get()->SaveDelay, false);
	}
}

void UZPlayerSaveComponent::HandleInventoryChanged(UZInventoryComponent* Inventory)
{
	MarkDirty();
}

void UZPlayerSaveComponent::HandleProgressionChanged(UZProgressionComponent* Progression)
{
	MarkDirty();
}

void UZPlayerSaveComponent::HandleTradeskillsChanged(UZCraftingComponent* Crafting)
{
	MarkDirty();
}

void UZPlayerSaveComponent::HandleRecipeDiscovered(UZCraftingComponent* Crafting, UZCraftingRecipe* Recipe)
{
	MarkDirty();
}

void UZPlayerSaveComponent::HandleEquipmentChanged(UZEquipmentComponent* Equipment)
{
	MarkDirty();
}

void UZPlayerSaveComponent::BindToChanges()
{
	AZSurvivalGamePlayerState* PlayerState = GetZPlayerState();
	if (!PlayerState || BoundPlayerState.Get() == PlayerState)
	{
		return;
	}

	UnbindFromChanges();
	BoundPlayerState = PlayerState;

	PlayerState->GetInventory()->OnInventoryChanged.AddUniqueDynamic(this, &UZPlayerSaveComponent::HandleInventoryChanged);
	PlayerState->GetProgression()->OnProgressionChanged.AddUniqueDynamic(this, &UZPlayerSaveComponent::HandleProgressionChanged);
	PlayerState->GetCrafting()->OnTradeskillsChanged.AddUniqueDynamic(this, &UZPlayerSaveComponent::HandleTradeskillsChanged);
	PlayerState->GetCrafting()->OnRecipeDiscovered.AddUniqueDynamic(this, &UZPlayerSaveComponent::HandleRecipeDiscovered);
	PlayerState->GetEquipment()->OnEquipmentChanged.AddUniqueDynamic(this, &UZPlayerSaveComponent::HandleEquipmentChanged);
}

void UZPlayerSaveComponent::UnbindFromChanges()
{
	if (AZSurvivalGamePlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->GetInventory()->OnInventoryChanged.RemoveDynamic(this, &UZPlayerSaveComponent::HandleInventoryChanged);
		PlayerState->GetProgression()->OnProgressionChanged.RemoveDynamic(this, &UZPlayerSaveComponent::HandleProgressionChanged);
		PlayerState->GetCrafting()->OnTradeskillsChanged.RemoveDynamic(this, &UZPlayerSaveComponent::HandleTradeskillsChanged);
		PlayerState->GetCrafting()->OnRecipeDiscovered.RemoveDynamic(this, &UZPlayerSaveComponent::HandleRecipeDiscovered);
		PlayerState->GetEquipment()->OnEquipmentChanged.RemoveDynamic(this, &UZPlayerSaveComponent::HandleEquipmentChanged);
	}

	BoundPlayerState.Reset();
}

APlayerController* UZPlayerSaveComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

AZSurvivalGamePlayerState* UZPlayerSaveComponent::GetZPlayerState() const
{
	const APlayerController* PlayerController = GetPlayerController();
	return PlayerController ? PlayerController->GetPlayerState<AZSurvivalGamePlayerState>() : nullptr;
}

#undef LOCTEXT_NAMESPACE
