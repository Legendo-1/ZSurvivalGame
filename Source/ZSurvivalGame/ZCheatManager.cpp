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
#include "ZSurvivalGameCharacter.h"
#include "ZSurvivalGamePlayerController.h"
#include "ZSurvivalGameState.h"
#include "AI/ZAICharacter.h"
#include "AI/ZZombieCharacter.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Audio/ZFootstepComponent.h"
#include "Crafting/ZCraftingComponent.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Crafting/ZCraftingStation.h"
#include "Crafting/ZCraftingSubsystem.h"
#include "Items/ZEquipmentComponent.h"
#include "Items/ZEquipmentSettings.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"
#include "Items/ZPickup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Progression/ZProgressionComponent.h"
#include "Progression/ZProgressionSubsystem.h"
#include "Progression/ZTraitDefinition.h"
#include "SaveGame/ZSaveGame.h"
#include "SaveGame/ZSaveStorage.h"
#include "SaveGame/ZSaveSubsystem.h"
#include "UI/ZCharacterPanelWidget.h"
#include "UI/ZEquipmentPanelWidget.h"
#include "UI/ZInventoryScreenWidget.h"
#include "UI/ZTraitRowWidget.h"
#include "Weapons/ZWeapon.h"
#include "Weapons/ZWeaponData.h"

bool UZCheatManager::CheckIsHost(const TCHAR* CommandName) const
{
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Client)
	{
		return true;
	}

	UE_LOG(LogZSurvivalGame, Warning, TEXT("%s only works on the host: type it in the Server window"), CommandName);
	return false;
}

void UZCheatManager::ZSpawnZombies(int32 Count, float Distance)
{
	APlayerController* PlayerController = GetOuterAPlayerController();
	if (!PlayerController || !CheckIsHost(TEXT("ZSpawnZombies")))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	// in a row across the view, ahead of the camera
	const FRotator FlatRotation(0.f, ViewRotation.Yaw, 0.f);
	const FVector Forward = FlatRotation.Vector();
	const FVector Right = FRotationMatrix(FlatRotation).GetUnitAxis(EAxis::Y);
	const int32 NumZombies = FMath::Max(Count, 1);
	const float HalfHeight = GetDefault<AZZombieCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	for (int32 Index = 0; Index < NumZombies; ++Index)
	{
		const float SideOffset = (Index - (NumZombies - 1) * 0.5f) * 150.f;
		const FVector Spot = ViewLocation + Forward * FMath::Max(Distance, 100.f) + Right * SideOffset;

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

void UZCheatManager::ZGiveItem(const FString& ItemName, int32 Count, const FString& Who)
{
	UZItemDefinition* Item = FindItem(ItemName);
	if (!Item || !CheckIsHost(TEXT("ZGiveItem")))
	{
		return;
	}

	for (const APlayerState* Player : GetTargetPlayers(Who))
	{
		if (UZInventoryComponent* Inventory = UZInventoryComponent::FindInventoryComponent(Player))
		{
			const int32 NumAdded = Inventory->AddItem(Item, FMath::Max(Count, 1));
			UE_LOG(LogZSurvivalGame, Log, TEXT("ZGiveItem: added %s x%d for '%s'"), *ItemName, NumAdded, *GetNameSafe(Player));
		}
	}
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

void UZCheatManager::ZWeaponStats()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const AZSurvivalGameCharacter* Character = PlayerController ? Cast<AZSurvivalGameCharacter>(PlayerController->GetPawn()) : nullptr;
	const AZWeapon* Weapon = Character ? Character->GetEquippedWeapon() : nullptr;
	if (!Weapon || !Weapon->GetWeaponData())
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZWeaponStats: no weapon in your hands"));
		return;
	}

	// on a client the item comes with the weapon, to its owner only
	const UZItemInstance* Item = Weapon->GetItemInstance();
	TArray<FString> Mods;
	if (Item)
	{
		for (const UZItemDefinition* Mod : Item->GetAttachedMods())
		{
			Mods.Add(Mod->GetName());
		}
	}

	const FVector2D Recoil = Weapon->GetRecoil();
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZWeaponStats: %s +%d [mods: %s]: damage %.2f, fire rate %.2f/s, spread %.2f (aiming %.2f), recoil %.3f up %.3f sideways, range %.0f, magazine %d/%d, reload %.2f s"),
		*GetNameSafe(Weapon->GetWeaponData()), Item ? Item->GetUpgradeLevel() : 0, Mods.Num() > 0 ? *FString::Join(Mods, TEXT(", ")) : (Item ? TEXT("none") : TEXT("no item here")),
		Weapon->GetDamage(), Weapon->GetFireRate(), Weapon->GetSpread(false), Weapon->GetSpread(true), Recoil.X, Recoil.Y, Weapon->GetRange(), Weapon->GetAmmo(), Weapon->GetMagazineSize(), Weapon->GetReloadTime());
}

void UZCheatManager::ZEquip(const FString& ItemName, int32 SlotNumber)
{
	// a request to the server, so this works in every window
	UZEquipmentComponent* Equipment = UZEquipmentComponent::FindEquipmentComponent(GetOuterAPlayerController());
	UZInventoryComponent* Inventory = GetMyInventory();
	const UZItemDefinition* Item = FindItem(ItemName);
	UZItemInstance* Instance = Inventory && Item ? Inventory->FindFirstInstance(Item) : nullptr;
	if (!Equipment || !Instance)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZEquip: you have no %s"), *ItemName);
		return;
	}

	if (!Item->IsEquippable())
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZEquip: %s can't be equipped (no Weapon or Wearable fragment)"), *ItemName);
		return;
	}

	// slot numbers count from 1, like the window shows them (Ring 3); none: a free slot of its kind
	const FZEquipmentSlot Slot = SlotNumber > 0 ? FZEquipmentSlot(Item->GetEquipmentSlotType(), SlotNumber - 1) : FZEquipmentSlot();
	Equipment->RequestEquip(Instance, Slot);
}

void UZCheatManager::ZUnequip(const FString& ItemName)
{
	UZEquipmentComponent* Equipment = UZEquipmentComponent::FindEquipmentComponent(GetOuterAPlayerController());
	UZInventoryComponent* Inventory = GetMyInventory();
	const UZItemDefinition* Item = FindItem(ItemName);

	// the equipped one, if there are several
	UZItemInstance* Instance = Equipment && Inventory && Item ? Inventory->FindFirstInstanceMatching([Item, Equipment](const UZItemInstance* Candidate)
	{
		return Candidate->GetItemDefinition() == Item && Equipment->IsEquipped(Candidate);
	}) : nullptr;

	if (!Instance)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZUnequip: you have no %s equipped"), *ItemName);
		return;
	}

	Equipment->RequestUnequip(Instance);
}

void UZCheatManager::ZEquipment()
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const UZEquipmentComponent* Equipment = UZEquipmentComponent::FindEquipmentComponent(PlayerController);
	if (!Equipment)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZEquipment: no equipment yet"));
		return;
	}

	const AZSurvivalGameCharacter* Character = PlayerController ? Cast<AZSurvivalGameCharacter>(PlayerController->GetPawn()) : nullptr;
	const UZItemInstance* InHands = Character ? Character->GetEquippedItem() : nullptr;
	const float Armor = UZAttributeSet::GetAttributeValue(PlayerController ? PlayerController->PlayerState : nullptr, UZAttributeSet::GetArmorAttribute(), 0.f);

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZEquipment: %s; in hands: %s (%s); armor %.0f (takes %.0f%% damage)"), *Equipment->GetDebugString(),
		InHands ? *GetNameSafe(InHands->GetItemDefinition()) : TEXT("nothing"), Character ? *UEnum::GetValueAsString(Character->GetWeaponType()) : TEXT("no character"),
		Armor, UZAttributeSet::GetDamageAfterArmor(100.f, Armor));
}

void UZCheatManager::ZInventoryUI(const FString& Command)
{
	AZSurvivalGamePlayerController* PlayerController = Cast<AZSurvivalGamePlayerController>(GetOuterAPlayerController());
	if (!PlayerController)
	{
		return;
	}

	TArray<FString> Words;
	Command.ParseIntoArrayWS(Words);
	const FString Action = Words.Num() > 0 ? Words[0].ToLower() : FString(TEXT("toggle"));

	if (Action == TEXT("open") || Action == TEXT("close") || Action == TEXT("toggle"))
	{
		if (Action == TEXT("open"))
		{
			PlayerController->OpenInventory();
		}
		else if (Action == TEXT("close"))
		{
			PlayerController->CloseInventory();
		}
		else
		{
			PlayerController->ToggleInventory();
		}

		UE_LOG(LogZSurvivalGame, Log, TEXT("ZInventoryUI: the inventory screen is %s"), PlayerController->IsInventoryOpen() ? TEXT("open") : TEXT("closed"));
		return;
	}

	UZInventoryScreenWidget* Screen = PlayerController->GetInventoryScreen();
	if (!Screen || !PlayerController->IsInventoryOpen())
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZInventoryUI: open the inventory screen first (ZInventoryUI open)"));
		return;
	}

	// items by asset name, the first of that kind in this player's inventory
	const UZInventoryComponent* Inventory = Screen->GetInventory();
	const auto FindInstance = [this, &Words, Inventory](int32 WordIndex) -> UZItemInstance*
	{
		const UZItemDefinition* Item = Words.IsValidIndex(WordIndex) ? FindItem(Words[WordIndex]) : nullptr;
		return Item && Inventory ? Inventory->FindFirstInstance(Item) : nullptr;
	};

	UZCharacterPanelWidget* CharacterPanel = Screen->GetCharacterPanel();

	if (Action == TEXT("select"))
	{
		Screen->SelectItem(FindInstance(1));
	}
	else if (Action == TEXT("use"))
	{
		Screen->UseItem(FindInstance(1));
	}
	else if (Action == TEXT("drop"))
	{
		Screen->DropItem(FindInstance(1), 0);
	}
	else if (Action == TEXT("add"))
	{
		Screen->AddToCombine(FindInstance(1));
	}
	else if (Action == TEXT("remove"))
	{
		Screen->RemoveFromCombine(FindInstance(1));
	}
	else if (Action == TEXT("clear"))
	{
		Screen->ClearCombine();
	}
	else if (Action == TEXT("count") && Words.IsValidIndex(1))
	{
		Screen->SetCraftCount(FCString::Atoi(*Words[1]));
	}
	else if (Action == TEXT("combine"))
	{
		Screen->CombineChosenItems();
	}
	else if (Action == TEXT("drag"))
	{
		Screen->CombineItems(FindInstance(1), FindInstance(2));
	}
	else if (Action == TEXT("raise") && CharacterPanel && Words.IsValidIndex(1))
	{
		// what the trait's + button does
		const UZProgressionSubsystem* Subsystem = UZProgressionSubsystem::Get(GetWorld());
		const UZTraitDefinition* Trait = Subsystem ? Subsystem->FindTraitByName(*Words[1]) : nullptr;
		Trait = Trait ? Trait : (Subsystem ? Subsystem->FindTraitByName(*(TEXT("TR_") + Words[1])) : nullptr);
		if (UZTraitRowWidget* Row = CharacterPanel->FindTraitRow(Trait))
		{
			Row->RaiseTrait();
		}
		else
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("ZInventoryUI: no trait row for '%s'"), *Words[1]);
		}
	}
	else if (Action == TEXT("reset") && CharacterPanel)
	{
		CharacterPanel->ResetTraits();
	}
	else if (Action == TEXT("equip"))
	{
		// what dropping an item on an equipment slot does: "equip ID_Rifle LongGun 1" (the slot's kind under Equipment.Slot, and its number from 1)
		FZEquipmentSlot Slot;
		if (Words.IsValidIndex(2))
		{
			Slot.Type = FGameplayTag::RequestGameplayTag(*(TEXT("Equipment.Slot.") + Words[2]), false);
			Slot.Index = Words.IsValidIndex(3) ? FCString::Atoi(*Words[3]) - 1 : 0;
		}

		UZItemInstance* Item = FindInstance(1);
		if (Slot.IsValid() && !Screen->CanEquip(Item, Slot))
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("ZInventoryUI: %s doesn't go in %s, so the drop does nothing"), Words.IsValidIndex(1) ? *Words[1] : TEXT("-"), *Slot.ToString());
		}
		else
		{
			Screen->EquipItem(Item, Slot);
		}
	}
	else if (Action == TEXT("unequip"))
	{
		// what right-clicking it in the equipment window does
		Screen->UnequipItem(FindInstance(1));
	}
	else if (Action != TEXT("show"))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZInventoryUI: don't know '%s' (open, close, show, select, use, drop, add, remove, clear, count, combine, drag, raise, reset, equip, unequip)"), *Command);
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZInventoryUI %s: %s"), *Command, *Screen->GetDebugString());
	if (CharacterPanel)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("ZInventoryUI character panel: %s"), *CharacterPanel->GetDebugString());
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

void UZCheatManager::ZGiveXP(int32 Amount, const FString& Who)
{
	if (!CheckIsHost(TEXT("ZGiveXP")))
	{
		return;
	}

	for (const APlayerState* Player : GetTargetPlayers(Who))
	{
		if (UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(Player))
		{
			Progression->AddExperience(Amount);
			UE_LOG(LogZSurvivalGame, Log, TEXT("ZGiveXP: '%s' is %s"), *GetNameSafe(Player), *Progression->GetDebugString());
		}
	}
}

void UZCheatManager::ZGiveTraitPoints(int32 Points, const FString& Who)
{
	if (!CheckIsHost(TEXT("ZGiveTraitPoints")))
	{
		return;
	}

	for (const APlayerState* Player : GetTargetPlayers(Who))
	{
		if (UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(Player))
		{
			Progression->AddTraitPoints(Points);
			UE_LOG(LogZSurvivalGame, Log, TEXT("ZGiveTraitPoints: '%s' is %s"), *GetNameSafe(Player), *Progression->GetDebugString());
		}
	}
}

void UZCheatManager::ZTraits()
{
	const UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(GetOuterAPlayerController());
	if (!Progression)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZTraits: no progression yet"));
		return;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Progression: %s"), *Progression->GetDebugString());
	for (const UZTraitDefinition* Trait : Progression->GetAllTraits())
	{
		const int32 Rank = Progression->GetTraitRank(Trait);
		UE_LOG(LogZSurvivalGame, Log, TEXT("Trait %s (%s): rank %d/%d, %d point(s) per rank%s %s"), *Trait->GetTraitName().ToString(), *Trait->GetName(), Rank, Trait->MaxRank,
			Trait->PointsPerRank, Progression->CanRaiseTrait(Trait) ? TEXT(", can raise") : TEXT(""), *Trait->GetEffectText(FMath::Max(Rank, 1)).ToString().Replace(TEXT("\n"), TEXT("; ")));
	}

	// the stats the traits change, as this machine sees them
	const APawn* Pawn = GetOuterAPlayerController() ? GetOuterAPlayerController()->GetPawn() : nullptr;
	TArray<FString> Stats;
	for (const FGameplayAttribute& Attribute : UZAttributeSet::GetTraitAttributes())
	{
		Stats.Add(FString::Printf(TEXT("%s %.2f"), *Attribute.GetName(), UZAttributeSet::GetAttributeValue(Pawn, Attribute, 0.f)));
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("Stats: Health %.1f, Stamina %.1f, %s"), UZAttributeSet::GetAttributeValue(Pawn, UZAttributeSet::GetHealthAttribute(), 0.f),
		UZAttributeSet::GetAttributeValue(Pawn, UZAttributeSet::GetStaminaAttribute(), 0.f), *FString::Join(Stats, TEXT(", ")));
}

void UZCheatManager::ZRaiseTrait(const FString& TraitName)
{
	// a request to the server, so this works in every window
	UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(GetOuterAPlayerController());
	const UZProgressionSubsystem* Subsystem = UZProgressionSubsystem::Get(GetWorld());
	if (!Progression || !Subsystem)
	{
		return;
	}

	UZTraitDefinition* Trait = Subsystem->FindTraitByName(*TraitName);
	if (!Trait)
	{
		Trait = Subsystem->FindTraitByName(*(TEXT("TR_") + TraitName));
	}

	if (!Trait)
	{
		TArray<FString> TraitNames;
		for (const UZTraitDefinition* Candidate : Subsystem->GetAllTraits())
		{
			TraitNames.Add(Candidate->GetName());
		}

		UE_LOG(LogZSurvivalGame, Warning, TEXT("No trait named '%s'. Traits: %s"), *TraitName, *FString::Join(TraitNames, TEXT(", ")));
		return;
	}

	if (!Progression->CanRaiseTrait(Trait))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZRaiseTrait: can't raise %s (rank %d/%d, %d point(s) unspent)"), *Trait->GetName(), Progression->GetTraitRank(Trait), Trait->MaxRank, Progression->GetUnspentTraitPoints());
		return;
	}

	Progression->RaiseTrait(Trait);
}

void UZCheatManager::ZResetTraits()
{
	if (UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(GetOuterAPlayerController()))
	{
		Progression->ResetTraits();
	}
}

void UZCheatManager::ZSave()
{
	if (UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld()))
	{
		Saves->SaveNow();
	}
}

void UZCheatManager::ZSaveSlots()
{
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld());
	if (!Saves)
	{
		return;
	}

	const TArray<FZSaveSlotInfo> Slots = Saves->GetSaveSlots();
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSaveSlots: %d slot(s) in %s"), Slots.Num(), *Saves->GetStorage()->Describe());
	for (const FZSaveSlotInfo& Slot : Slots)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("  %s: '%s', level %d, played %dh %02dm, saved %s%s%s"), *Slot.SlotName, *Slot.CharacterName, Slot.Level,
			static_cast<int32>(Slot.PlayTime.GetTotalHours()), Slot.PlayTime.GetMinutes(), *Slot.LastSaved.ToString(TEXT("%Y-%m-%d %H:%M:%S")),
			Slot.bIsActive ? TEXT(" (in use)") : TEXT(""), Slot.bCanLoad ? TEXT("") : TEXT(" (can't be loaded)"));
	}
}

void UZCheatManager::ZSaveInfo()
{
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld());
	if (!Saves)
	{
		return;
	}

	const FZSaveSlotInfo Slot = Saves->GetActiveSlotInfo();
	const FZCharacterSaveData& Character = Saves->GetActiveCharacter();
	const FZWorldSaveData& World = Saves->GetActiveWorld();

	TArray<FString> Items;
	for (const FZSavedItem& Item : Character.Inventory)
	{
		TArray<FString> Details;
		for (const FZSavedItemStat& Stat : Item.Stats)
		{
			Details.Add(FString::Printf(TEXT("%s %d"), *Stat.Tag.ToString(), Stat.Value));
		}

		for (const FPrimaryAssetId& Mod : Item.Mods)
		{
			Details.Add(FString::Printf(TEXT("mod %s"), *Mod.PrimaryAssetName.ToString()));
		}

		if (Item.EquipmentSlot.IsValid())
		{
			Details.Add(FString::Printf(TEXT("equipped: %s"), *FZEquipmentSlot(Item.EquipmentSlot, Item.EquipmentIndex).ToString()));
		}

		Items.Add(FString::Printf(TEXT("%s x%d%s%s"), *Item.Item.PrimaryAssetName.ToString(), Item.Count,
			Details.Num() > 0 ? *FString::Printf(TEXT(" (%s)"), *FString::Join(Details, TEXT(", "))) : TEXT(""), Item.bEquipped ? TEXT(" [in hands]") : TEXT("")));
	}

	TArray<FString> Traits;
	for (const FZSavedTraitRank& Trait : Character.Traits)
	{
		Traits.Add(FString::Printf(TEXT("%s %d"), *Trait.Trait.PrimaryAssetName.ToString(), Trait.Rank));
	}

	TArray<FString> Skills;
	for (const FZSavedTradeskill& Skill : Character.Tradeskills)
	{
		Skills.Add(FString::Printf(TEXT("%s %d xp"), *Skill.Skill.ToString(), Skill.Experience));
	}

	TArray<FString> Recipes;
	for (const FPrimaryAssetId& Recipe : Character.DiscoveredRecipes)
	{
		Recipes.Add(Recipe.PrimaryAssetName.ToString());
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSaveInfo: slot '%s', character '%s' (%s): level %d, %d xp, %d bonus trait points, starting items %s"),
		*Slot.SlotName, *Character.CharacterName, *Character.CharacterId.ToString(), Slot.Level, Character.Experience, Character.BonusTraitPoints,
		Character.bReceivedStartingItems ? TEXT("received") : TEXT("not yet"));
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSaveInfo: items: %s"), Items.Num() > 0 ? *FString::Join(Items, TEXT(", ")) : TEXT("none"));
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSaveInfo: traits: %s; tradeskills: %s; recipes: %s"), Traits.Num() > 0 ? *FString::Join(Traits, TEXT(", ")) : TEXT("none"),
		Skills.Num() > 0 ? *FString::Join(Skills, TEXT(", ")) : TEXT("none"), Recipes.Num() > 0 ? *FString::Join(Recipes, TEXT(", ")) : TEXT("none"));
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZSaveInfo: world: %s in %s"), World.Progress.IsEmpty() ? TEXT("no progress") : *World.Progress.ToStringSimple(),
		World.MapName.IsEmpty() ? TEXT("(the default level)") : *World.MapName);
}

void UZCheatManager::ZNewCharacter(const FString& CharacterName)
{
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld());
	const FString SlotName = Saves ? Saves->CreateSlot(CharacterName) : FString();
	if (!SlotName.IsEmpty())
	{
		Saves->LoadSlot(SlotName);
	}
}

void UZCheatManager::ZLoadSlot(const FString& SlotName)
{
	if (UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld()))
	{
		Saves->LoadSlot(SlotName);
	}
}

void UZCheatManager::ZDeleteSlot(const FString& SlotName)
{
	if (UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld()))
	{
		Saves->DeleteSlot(SlotName);
	}
}

void UZCheatManager::ZWorldProgress(const FString& Tag)
{
	AZSurvivalGameState* GameState = GetWorld()->GetGameState<AZSurvivalGameState>();
	if (!GameState)
	{
		return;
	}

	if (Tag.IsEmpty())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("ZWorldProgress: %s"), GameState->GetWorldProgress().IsEmpty() ? TEXT("nothing done yet") : *GameState->GetWorldProgress().ToStringSimple());
		return;
	}

	const FGameplayTag Progress = FGameplayTag::RequestGameplayTag(FName(*Tag), false);
	if (!Progress.MatchesTag(ZGameplayTags::WorldProgress))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZWorldProgress: '%s' isn't a WorldProgress tag (add it in Project Settings > Gameplay Tags, under WorldProgress)"), *Tag);
		return;
	}

	if (CheckIsHost(TEXT("ZWorldProgress")))
	{
		GameState->AddWorldProgress(Progress);
	}
}

void UZCheatManager::ZBreakSave()
{
#if !UE_BUILD_SHIPPING
	UZSaveSubsystem* Saves = UZSaveSubsystem::Get(GetWorld());
	UZSaveGame* Save = Saves && Saves->IsSavingEnabled() ? Saves->GetActiveSaveForTesting() : nullptr;
	if (!Save)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZBreakSave: saving is off"));
		return;
	}

	FZCharacterSaveData& Character = Save->Character;
	Character.Experience = 2000000000;
	Character.BonusTraitPoints = -5;

	// a trait far past its highest rank, listed twice
	FZSavedTraitRank Trait;
	Trait.Trait = FPrimaryAssetId(TEXT("Trait:TR_Vigor"));
	Trait.Rank = 99;
	Character.Traits.Add(Trait);
	Character.Traits.Add(Trait);

	// a tradeskill that isn't one, and one past the highest level
	FZSavedTradeskill& NotASkill = Character.Tradeskills.AddDefaulted_GetRef();
	NotASkill.Skill = ZGameplayTags::Item_Stat_Durability;
	NotASkill.Experience = 5;
	FZSavedTradeskill& TooMuch = Character.Tradeskills.AddDefaulted_GetRef();
	TooMuch.Skill = ZGameplayTags::Tradeskill_Electronics;
	TooMuch.Experience = 2000000000;

	// an item that doesn't exist, a stack bigger than stacks get, and a rifle with an impossible magazine, upgrade and mod
	FZSavedItem& Missing = Character.Inventory.AddDefaulted_GetRef();
	Missing.Item = FPrimaryAssetId(TEXT("Item:ID_DoesNotExist"));
	FZSavedItem& Ammo = Character.Inventory.AddDefaulted_GetRef();
	Ammo.Item = FPrimaryAssetId(TEXT("Item:ID_Ammo_Rifle"));
	Ammo.Count = 1000000;
	FZSavedItem& Rifle = Character.Inventory.AddDefaulted_GetRef();
	Rifle.Item = FPrimaryAssetId(TEXT("Item:ID_Rifle"));
	Rifle.Count = 5;
	Rifle.Mods.Add(FPrimaryAssetId(TEXT("Item:ID_DoesNotExist")));
	FZSavedItemStat& Magazine = Rifle.Stats.AddDefaulted_GetRef();
	Magazine.Tag = ZGameplayTags::Item_Stat_MagazineAmmo;
	Magazine.Value = 9999;
	FZSavedItemStat& Upgrade = Rifle.Stats.AddDefaulted_GetRef();
	Upgrade.Tag = ZGameplayTags::Item_Stat_UpgradeLevel;
	Upgrade.Value = 99;
	FZSavedItemStat& NotAStat = Rifle.Stats.AddDefaulted_GetRef();
	NotAStat.Tag = ZGameplayTags::Ability_Sprint;
	NotAStat.Value = 1;

	Character.DiscoveredRecipes.Add(FPrimaryAssetId(TEXT("Recipe:RC_DoesNotExist")));

	// a rifle in a ring slot players don't have, and ammo "worn" on the head (the rifle first: adding to the list can move it)
	Rifle.EquipmentSlot = ZGameplayTags::Equipment_Slot_Ring;
	Rifle.EquipmentIndex = 99;
	FZSavedItem& AmmoOnHead = Character.Inventory.AddDefaulted_GetRef();
	AmmoOnHead.Item = FPrimaryAssetId(TEXT("Item:ID_Ammo_Pistol"));
	AmmoOnHead.Count = 5;
	AmmoOnHead.EquipmentSlot = ZGameplayTags::Equipment_Slot_Head;

	if (Saves->WriteActiveSaveForTesting())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("ZBreakSave: the slot in use now breaks the game's rules; the next game with it shows what the host fixed"));
	}
#endif
}

void UZCheatManager::ZFloorSurface(const FString& SurfaceName)
{
	const APlayerController* PlayerController = GetOuterAPlayerController();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}

	// the floor under the player, the way footsteps find it
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ZFloorSurface), true, Pawn);
	Params.bReturnPhysicalMaterial = true;
	FHitResult Hit;
	const FVector Start = Pawn->GetActorLocation();
	UPrimitiveComponent* Floor = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 500.f), ECC_Visibility, Params) ? Hit.GetComponent() : nullptr;
	if (!Floor)
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZFloorSurface: there's no floor under you"));
		return;
	}

	if (SurfaceName.IsEmpty())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("ZFloorSurface: '%s' is %s (%s)"), *GetNameSafe(Floor->GetOwner()),
			*UZFootstepComponent::GetSurfaceName(UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get())), *GetNameSafe(Hit.PhysMaterial.Get()));
		return;
	}

	EPhysicalSurface Surface = SurfaceType_Default;
	if (!UZFootstepComponent::FindSurfaceByName(SurfaceName, Surface))
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("ZFloorSurface: there's no surface '%s' (Project Settings > Physics > Physical Surface)"), *SurfaceName);
		return;
	}

	// a physical material made just for this game session, like the Phys Material Override in a mesh's Details
	UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(Floor);
	Material->SurfaceType = Surface;
	Floor->SetPhysMaterialOverride(Material);
	UE_LOG(LogZSurvivalGame, Log, TEXT("ZFloorSurface: '%s' is now %s until the game ends"), *GetNameSafe(Floor->GetOwner()), *UZFootstepComponent::GetSurfaceName(Surface));
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
