// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZCheatManager.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGamePlayerController.h"
#include "AI/ZAICharacter.h"
#include "AI/ZZombieCharacter.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Crafting/ZCraftingComponent.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZCraftingStation.h"
#include "Crafting/ZCraftingSubsystem.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"
#include "Items/ZPickup.h"

bool UZCheatManager::CheckIsHost(const TCHAR* CommandName) const
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Client)
	{
		return true;
	}

	UE_LOG(LogZSurvivalGame, Warning, TEXT("%s only works on the host: type it in the Server window"), CommandName);
	return false;
}

void UZCheatManager::ZSpawnZombies(int32 Count)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	if (!PlayerController || !CheckIsHost(TEXT("ZSpawnZombies")))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// in a row across the view, 8 m ahead of the camera
	const FRotator FlatRotation(0.f, ViewRotation.Yaw, 0.f);
	const FVector Forward = FlatRotation.Vector();
	const FVector Right = FRotationMatrix(FlatRotation).GetUnitAxis(EAxis::Y);
	const int32 NumZombies = FMath::Max(Count, 1);
	const float HalfHeight = GetDefault<AZZombieCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	for (int32 Index = 0; Index < NumZombies; ++Index)
	{
		const float SideOffset = (Index - (NumZombies - 1) * 0.5f) * 150.f;
		const FVector Spot = ViewLocation + Forward * 800.f + Right * SideOffset;

		// drop it onto the ground
		FHitResult Ground;
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZCheatSpawn), false, PlayerController->GetPawn());
		const bool bFoundGround = GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0.f, 0.f, 500.f), Spot - FVector(0.f, 0.f, 2000.f), ECC_Visibility, QueryParams);
		const FVector SpawnLocation = bFoundGround ? FVector(Ground.ImpactPoint) + FVector(0.f, 0.f, HalfHeight + 2.f) : Spot;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		GetWorld()->SpawnActor<AZZombieCharacter>(AZZombieCharacter::StaticClass(), SpawnLocation, (-Forward).Rotation(), SpawnParams);
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSpawnZombies: spawned %d"), NumZombies);
}

void UZCheatManager::ZKillEnemies()
{
	if (!CheckIsHost(TEXT("ZKillEnemies")))
	{
		return;
	}

	int32 NumKilled = 0;
	for (TActorIterator<AZAICharacter> It(GetWorld()); It; ++It)
	{
		UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
		if (ASC && !It->IsDead())
		{
			ASC->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), 0.f);
			++NumKilled;
		}
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZKillEnemies: killed %d"), NumKilled);
}

void UZCheatManager::ZHeal()
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	UAbilitySystemComponent* ASC = PlayerController ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->GetPawn()) : nullptr;
	if (!ASC || !CheckIsHost(TEXT("ZHeal")))
	{
		return;
	}

	ASC->SetNumericAttributeBase(UZAttributeSet::GetHealthAttribute(), ASC->GetNumericAttribute(UZAttributeSet::GetMaxHealthAttribute()));
	ASC->SetNumericAttributeBase(UZAttributeSet::GetStaminaAttribute(), ASC->GetNumericAttribute(UZAttributeSet::GetMaxStaminaAttribute()));
}

void UZCheatManager::ZHurt(float Amount)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	UAbilitySystemComponent* ASC = PlayerController ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->GetPawn()) : nullptr;
	if (!ASC || !CheckIsHost(TEXT("ZHurt")))
	{
		return;
	}

	const FGameplayAttribute HealthAttribute = UZAttributeSet::GetHealthAttribute();
	if (Amount > 0.f)
	{
		ASC->SetNumericAttributeBase(HealthAttribute, ASC->GetNumericAttributeBase(HealthAttribute) - Amount);
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZHurt: health is %.0f"), ASC->GetNumericAttribute(HealthAttribute));
}

UZItemDefinition* UZCheatManager::FindItem(const FString& ItemName) const
{
	UAssetManager& AssetManager = UAssetManager::Get();
	const FSoftObjectPath ItemPath = AssetManager.GetPrimaryAssetPath(FPrimaryAssetId(UZItemDefinition::GetItemAssetType(), FName(*ItemName)));
	if (UZItemDefinition* Item = Cast<UZItemDefinition>(ItemPath.TryLoad()))
	{
		return Item;
	}

	// not found: list the ones there are
	TArray<FPrimaryAssetId> ItemIds;
	AssetManager.GetPrimaryAssetIdList(UZItemDefinition::GetItemAssetType(), ItemIds);

	TArray<FString> ItemNames;
	for (const FPrimaryAssetId& ItemId : ItemIds)
	{
		ItemNames.Add(ItemId.PrimaryAssetName.ToString());
	}

	ItemNames.Sort();
	UE_LOG(LogZSurvivalGame, Warning, TEXT("No item named '%s'. Items: %s"), *ItemName, *FString::Join(ItemNames, TEXT(", ")));
	return nullptr;
}

UZInventoryComponent* UZCheatManager::GetMyInventory() const
{
	return UZInventoryComponent::FindInventoryComponent(GetOuterAPlayerController());
}

TArray<APlayerState*> UZCheatManager::GetTargetPlayers(const FString& Who) const
{
	TArray<APlayerState*> Players;
	const APlayerController* PlayerController = GetOuterAPlayerController();
	if (Who.Equals(TEXT("all"), ESearchCase::IgnoreCase))
	{
		if (const AGameStateBase* GameState = GetWorld()->GetGameState())
		{
			for (APlayerState* Player : GameState->PlayerArray)
			{
				Players.Add(Player);
			}
		}
	}
	else if (PlayerController && PlayerController->PlayerState)
	{
		Players.Add(PlayerController->PlayerState);
	}

	return Players;
}

void UZCheatManager::ZGiveItem(const FString& ItemName, int32 Count)
{
	UZInventoryComponent* Inventory = GetMyInventory();
	UZItemDefinition* Item = FindItem(ItemName);
	if (!Inventory || !Item || !CheckIsHost(TEXT("ZGiveItem")))
	{
		return;
	}

	const int32 NumAdded = Inventory->AddItem(Item, FMath::Max(Count, 1));
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZGiveItem: added %s x%d"), *ItemName, NumAdded);
}

void UZCheatManager::ZShowInventory()
{
	const UZInventoryComponent* Inventory = GetMyInventory();
	UE_LOG(LogZSurvivalGame, Log, TEXT("Inventory: %s"), Inventory ? *Inventory->GetDebugString() : TEXT("(none yet)"));
}

void UZCheatManager::ZDropItem(const FString& ItemName, int32 Count)
{
	// dropping is a request to the server, so this works in every window
	UZInventoryComponent* Inventory = GetMyInventory();
	const UZItemDefinition* Item = FindItem(ItemName);
	UZItemInstance* Instance = Inventory && Item ? Inventory->FindFirstInstance(Item) : nullptr;
	if (!Instance)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZDropItem: you have no %s"), *ItemName);
		return;
	}

	Inventory->DropItem(Instance, FMath::Max(Count, 1));
}

void UZCheatManager::ZUseItem(const FString& ItemName)
{
	// using is a request to the server, so this works in every window
	UZInventoryComponent* Inventory = GetMyInventory();
	const UZItemDefinition* Item = FindItem(ItemName);
	UZItemInstance* Instance = Inventory && Item ? Inventory->FindFirstInstance(Item) : nullptr;
	if (!Instance)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZUseItem: you have no %s"), *ItemName);
		return;
	}

	Inventory->UseItem(Instance);
}

void UZCheatManager::ZSpawnPickup(const FString& ItemName, int32 Count, float Distance)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	UZItemDefinition* Item = FindItem(ItemName);
	if (!Pawn || !Item || !CheckIsHost(TEXT("ZSpawnPickup")))
	{
		return;
	}

	const FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * FMath::Max(Distance, 0.f);
	const bool bSpawned = AZPickup::SpawnPickup(GetWorld(), nullptr, FZItemStack(Item, FMath::Max(Count, 1)), Location) != nullptr;
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSpawnPickup: %s"), bSpawned ? TEXT("spawned") : TEXT("failed"));
}

void UZCheatManager::ZCombine(const FString& ItemNames)
{
	// combining is a request to the server, so this works in every window
	UZCraftingComponent* Crafting = UZCraftingComponent::FindCraftingComponent(GetOuterAPlayerController());
	const UZInventoryComponent* Inventory = GetMyInventory();
	if (!Crafting || !Inventory)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZCombine: no inventory yet"));
		return;
	}

	TArray<FString> Names;
	ItemNames.ParseIntoArrayWS(Names);

	int32 Count = 1;
	if (Names.Num() > 1 && Names.Last().IsNumeric())
	{
		Count = FCString::Atoi(*Names.Last());
		Names.Pop();
	}

	TArray<UZItemInstance*> Items;
	for (const FString& Name : Names)
	{
		const UZItemDefinition* Item = FindItem(Name);
		if (!Item)
		{
			return;
		}

		// the same item named twice: another stack of it
		UZItemInstance* Instance = Inventory->FindFirstInstanceMatching([Item, &Items](const UZItemInstance* Candidate)
		{
			return Candidate->GetItemDefinition() == Item && !Items.Contains(Candidate);
		});

		if (!Instance)
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("ZCombine: you have no (more) %s"), *Name);
			return;
		}

		Items.Add(Instance);
	}

	Crafting->CombineItems(Items, Count);
}

void UZCheatManager::ZSkills()
{
	const UZCraftingComponent* Crafting = UZCraftingComponent::FindCraftingComponent(GetOuterAPlayerController());
	UE_LOG(LogZSurvivalGame, Log, TEXT("Crafting: %s"), Crafting ? *Crafting->GetDebugString() : TEXT("(none yet)"));
}

void UZCheatManager::ZSetSkill(const FString& SkillName, int32 Level)
{
	UZCraftingComponent* Crafting = UZCraftingComponent::FindCraftingComponent(GetOuterAPlayerController());
	if (!Crafting || !CheckIsHost(TEXT("ZSetSkill")))
	{
		return;
	}

	const FGameplayTag Skill = FGameplayTag::RequestGameplayTag(*(TEXT("Tradeskill.") + SkillName), false);
	if (!Skill.IsValid())
	{
		TArray<FString> SkillNames;
		for (const FGameplayTag& Tradeskill : UZCraftingComponent::GetAllTradeskills())
		{
			SkillNames.Add(UZCraftingComponent::GetSkillName(Tradeskill).ToString());
		}

		UE_LOG(LogZSurvivalGame, Warning, TEXT("No tradeskill named '%s'. Tradeskills: %s"), *SkillName, *FString::Join(SkillNames, TEXT(", ")));
		return;
	}

	Crafting->SetSkillLevel(Skill, Level);
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSetSkill: %s is level %d"), *Skill.ToString(), Crafting->GetSkillLevel(Skill));
}

void UZCheatManager::ZListRecipes()
{
	const UZCraftingSubsystem* Subsystem = UZCraftingSubsystem::Get(GetWorld());
	if (!Subsystem)
	{
		return;
	}

	for (const UZCraftingRecipe* Recipe : Subsystem->GetRecipes())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Recipe %s"), *Recipe->GetDebugString());
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZListRecipes: %d recipes"), Subsystem->GetRecipes().Num());
}

void UZCheatManager::ZForgetDiscoveries()
{
	UZCraftingComponent* Crafting = UZCraftingComponent::FindCraftingComponent(GetOuterAPlayerController());
	if (Crafting && CheckIsHost(TEXT("ZForgetDiscoveries")))
	{
		Crafting->ForgetDiscoveries();
		UE_LOG(LogZSurvivalGame, Log, TEXT("ZForgetDiscoveries: done"));
	}
}

void UZCheatManager::ZGiveCraftingKit(const FString& Who)
{
	if (!CheckIsHost(TEXT("ZGiveCraftingKit")))
	{
		return;
	}

	// enough to find a few recipes; the screwdriver is the only tool
	const TPair<const TCHAR*, int32> Kit[] = {
		{ TEXT("ID_Cloth"), 9 }, { TEXT("ID_Scrap"), 12 }, { TEXT("ID_BoneFragment"), 6 }, { TEXT("ID_RottenMeat"), 3 },
		{ TEXT("ID_DuctTape"), 4 }, { TEXT("ID_BrokenRadio"), 2 }, { TEXT("ID_ElectronicParts"), 3 }, { TEXT("ID_Gunpowder"), 2 }, { TEXT("ID_Screwdriver"), 1 }
	};

	for (APlayerState* Player : GetTargetPlayers(Who))
	{
		UZInventoryComponent* Inventory = UZInventoryComponent::FindInventoryComponent(Player);
		if (!Inventory)
		{
			continue;
		}

		for (const TPair<const TCHAR*, int32>& Entry : Kit)
		{
			if (UZItemDefinition* Item = FindItem(Entry.Key))
			{
				Inventory->AddItem(Item, Entry.Value);
			}
		}

		UE_LOG(LogZSurvivalGame, Log, TEXT("ZGiveCraftingKit: '%s' has %s"), *GetNameSafe(Player), *Inventory->GetDebugString());
	}
}

void UZCheatManager::ZSpawnWorkbench(const FString& Who)
{
	if (!CheckIsHost(TEXT("ZSpawnWorkbench")))
	{
		return;
	}

	UClass* WorkbenchClass = LoadClass<AZCraftingStation>(nullptr, TEXT("/Game/Crafting/Stations/BP_Workbench.BP_Workbench_C"));
	if (!WorkbenchClass)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZSpawnWorkbench: /Game/Crafting/Stations/BP_Workbench not found"));
		return;
	}

	for (const APlayerState* Player : GetTargetPlayers(Who))
	{
		const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		// on the ground 2 m in front
		const FVector Spot = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 200.f;
		FHitResult Ground;
		const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ZCheatSpawn), false, Pawn);
		const bool bFoundGround = GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0.f, 0.f, 200.f), Spot - FVector(0.f, 0.f, 1000.f), ECC_Visibility, QueryParams);
		const FVector SpawnLocation = bFoundGround ? FVector(Ground.ImpactPoint) + FVector(0.f, 0.f, 46.f) : Spot;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const AActor* Workbench = GetWorld()->SpawnActor<AActor>(WorkbenchClass, SpawnLocation, FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), SpawnParams);
		UE_LOG(LogZSurvivalGame, Log, TEXT("ZSpawnWorkbench: %s for '%s'"), Workbench ? TEXT("spawned") : TEXT("failed"), *GetNameSafe(Player));
	}
}

void UZCheatManager::ZSetDurability(const FString& ItemName, int32 Durability)
{
	UZInventoryComponent* Inventory = GetMyInventory();
	const UZItemDefinition* Item = FindItem(ItemName);
	UZItemInstance* Instance = Inventory && Item ? Inventory->FindFirstInstance(Item) : nullptr;
	if (!Instance || !CheckIsHost(TEXT("ZSetDurability")))
	{
		return;
	}

	if (!Instance->HasDurability())
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZSetDurability: %s has no durability"), *ItemName);
		return;
	}

	Instance->SetStatTagStack(ZGameplayTags::Item_Stat_Durability, FMath::Clamp(Durability, 0, Instance->GetMaxDurability()));
	Inventory->BroadcastChanged();
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSetDurability: %s"), *Instance->GetDebugString());
}

void UZCheatManager::ZDelay(float Seconds, const FString& Command)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	if (!PlayerController || Command.IsEmpty())
	{
		return;
	}

	FTimerHandle DelayTimer;
	GetWorld()->GetTimerManager().SetTimer(DelayTimer, FTimerDelegate::CreateWeakLambda(PlayerController, [PlayerController, Command]()
	{
		PlayerController->ConsoleCommand(Command);
	}), FMath::Max(Seconds, 0.01f), false);
}

void UZCheatManager::ZOnClients(const FString& Command)
{
	if (Command.IsEmpty() || !CheckIsHost(TEXT("ZOnClients")))
	{
		return;
	}

	int32 NumClients = 0;
	for (TActorIterator<AZSurvivalGamePlayerController> It(GetWorld()); It; ++It)
	{
		if (!It->IsLocalController())
		{
			It->ClientRunConsoleCommand(Command);
			++NumClients;
		}
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZOnClients: sent '%s' to %d client(s)"), *Command, NumClients);
}
