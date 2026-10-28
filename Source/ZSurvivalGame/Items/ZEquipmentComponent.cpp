// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZEquipmentComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayEffect_Equipment.h"
#include "Items/ZEquipmentSettings.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemDefinition.h"
#include "Items/ZItemInstance.h"

#define LOCTEXT_NAMESPACE "ZItems"

FText FZEquipmentSlot::GetDisplayName() const
{
	const UZEquipmentSettings* Settings = UZEquipmentSettings::Get();
	const FText TypeName = Settings->GetSlotTypeName(Type);
	return Settings->GetSlotCount(Type) > 1 ? FText::Format(LOCTEXT("NumberedSlot", "{0} {1}"), TypeName, Index + 1) : TypeName;
}

FString FZEquipmentSlot::ToString() const
{
	return FString::Printf(TEXT("%s %d"), *Type.ToString(), Index);
}

UZEquipmentComponent::UZEquipmentComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

UZEquipmentComponent* UZEquipmentComponent::FindEquipmentComponent(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UZEquipmentComponent* OwnEquipment = Actor->FindComponentByClass<UZEquipmentComponent>())
	{
		return OwnEquipment;
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

	return PlayerState ? PlayerState->FindComponentByClass<UZEquipmentComponent>() : nullptr;
}

void UZEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only the owner has the items (the inventory replicates to them only); everyone sees the weapon in the hands through the character
	DOREPLIFETIME_CONDITION(UZEquipmentComponent, Equipped, COND_OwnerOnly);
}

void UZEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		if (UZInventoryComponent* Inventory = GetInventory())
		{
			Inventory->OnItemRemoving.AddUObject(this, &UZEquipmentComponent::HandleItemRemoving);
		}
	}
}

void UZEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UZInventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnItemRemoving.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

TArray<FZEquipmentSlot> UZEquipmentComponent::GetAllSlots()
{
	TArray<FZEquipmentSlot> Slots;
	for (const FZEquipmentSlotType& SlotType : UZEquipmentSettings::Get()->SlotTypes)
	{
		for (int32 Index = 0; SlotType.Type.IsValid() && Index < FMath::Max(SlotType.Count, 1); ++Index)
		{
			Slots.Emplace(SlotType.Type, Index);
		}
	}

	return Slots;
}

bool UZEquipmentComponent::IsValidSlot(const FZEquipmentSlot& Slot)
{
	return Slot.IsValid() && Slot.Index >= 0 && Slot.Index < UZEquipmentSettings::Get()->GetSlotCount(Slot.Type);
}

UZItemInstance* UZEquipmentComponent::GetItemInSlot(const FZEquipmentSlot& Slot) const
{
	const FZEquippedItem* Entry = Equipped.FindByPredicate([&Slot](const FZEquippedItem& Candidate) { return Candidate.Slot == Slot; });
	return Entry ? Entry->Item : nullptr;
}

FZEquipmentSlot UZEquipmentComponent::GetSlotOfItem(const UZItemInstance* Item) const
{
	const FZEquippedItem* Entry = Item ? Equipped.FindByPredicate([Item](const FZEquippedItem& Candidate) { return Candidate.Item == Item; }) : nullptr;
	return Entry ? Entry->Slot : FZEquipmentSlot();
}

bool UZEquipmentComponent::IsEquipped(const UZItemInstance* Item) const
{
	return GetSlotOfItem(Item).IsValid();
}

bool UZEquipmentComponent::CanEquip(const UZItemInstance* Item, const FZEquipmentSlot& Slot) const
{
	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	const UZInventoryComponent* Inventory = GetInventory();
	return Definition && Inventory && IsValidSlot(Slot) && Definition->GetEquipmentSlotType() == Slot.Type && Inventory->GetStackCount(Item) > 0;
}

TArray<UZItemInstance*> UZEquipmentComponent::GetItemsOfSlotType(const FGameplayTag& Type) const
{
	TArray<UZItemInstance*> Items;
	const int32 NumSlots = UZEquipmentSettings::Get()->GetSlotCount(Type);
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		if (UZItemInstance* Item = GetItemInSlot(FZEquipmentSlot(Type, Index)))
		{
			Items.Add(Item);
		}
	}

	return Items;
}

bool UZEquipmentComponent::HaveAllItemsArrived() const
{
	return !Equipped.ContainsByPredicate([](const FZEquippedItem& Entry) { return !Entry.Item || !Entry.Item->GetItemDefinition(); });
}

void UZEquipmentComponent::RequestEquip(UZItemInstance* Item, FZEquipmentSlot Slot)
{
	// on the server this runs right away
	ServerEquip(Item, Slot);
}

void UZEquipmentComponent::RequestUnequip(UZItemInstance* Item)
{
	ServerUnequip(Item);
}

void UZEquipmentComponent::ServerEquip_Implementation(UZItemInstance* Item, FZEquipmentSlot Slot)
{
	// CanEquip checks it's this player's item and that it goes in that slot
	EquipItemByPlayer(Item, Slot);
}

void UZEquipmentComponent::ServerUnequip_Implementation(UZItemInstance* Item)
{
	UnequipItem(Item);
}

bool UZEquipmentComponent::EquipItem(UZItemInstance* Item, FZEquipmentSlot Slot)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	if (!Slot.IsValid())
	{
		Slot = FindSlotFor(Item);
	}

	if (!CanEquip(Item, Slot))
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' can't equip '%s' in %s"), *GetNameSafe(GetOwner()), Item ? *Item->GetDebugString() : TEXT("nothing"), *Slot.ToString());
		return false;
	}

	const FZEquipmentSlot OldSlot = GetSlotOfItem(Item);
	if (OldSlot == Slot)
	{
		return true;
	}

	// what was in the slot goes back to the inventory, or to the item's old slot (both are the same kind)
	UZItemInstance* Displaced = GetItemInSlot(Slot);
	Equipped.RemoveAll([&Slot, Item](const FZEquippedItem& Entry) { return Entry.Slot == Slot || Entry.Item == Item; });

	FZEquippedItem& NewEntry = Equipped.AddDefaulted_GetRef();
	NewEntry.Slot = Slot;
	NewEntry.Item = Item;

	if (Displaced && OldSlot.IsValid())
	{
		FZEquippedItem& SwappedEntry = Equipped.AddDefaulted_GetRef();
		SwappedEntry.Slot = OldSlot;
		SwappedEntry.Item = Displaced;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' equipped '%s' in %s%s"), *GetNameSafe(GetOwner()), *GetNameSafe(Item->GetItemDefinition()), *Slot.ToString(),
		Displaced ? *FString::Printf(TEXT(" (instead of '%s')"), *GetNameSafe(Displaced->GetItemDefinition())) : TEXT(""));

	HandleEquipmentChanged();
	return true;
}

bool UZEquipmentComponent::EquipItemByPlayer(UZItemInstance* Item, FZEquipmentSlot Slot)
{
	const FZEquipmentSlot TargetSlot = Slot.IsValid() ? Slot : FindSlotFor(Item);
	const bool bAlreadyThere = TargetSlot.IsValid() && GetSlotOfItem(Item) == TargetSlot;
	if (!EquipItem(Item, TargetSlot))
	{
		return false;
	}

	const APlayerState* PlayerState = Cast<APlayerState>(GetOwner());
	APawn* User = PlayerState ? PlayerState->GetPawn() : Cast<APawn>(GetOwner());

	// e.g. the sound of putting on a jacket
	if (!bAlreadyThere && User)
	{
		for (const UZItemFragment* Fragment : Item->GetItemDefinition()->Fragments)
		{
			if (Fragment)
			{
				Fragment->OnEquippedByPlayer(Item, User);
			}
		}
	}

	// e.g. the character takes out a weapon that was just equipped
	OnItemEquippedByPlayer.Broadcast(this, Item);
	return true;
}

bool UZEquipmentComponent::EquipInFreeSlot(UZItemInstance* Item)
{
	if (IsEquipped(Item))
	{
		return true;
	}

	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	const FGameplayTag Type = Definition ? Definition->GetEquipmentSlotType() : FGameplayTag();
	const int32 NumSlots = UZEquipmentSettings::Get()->GetSlotCount(Type);
	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		const FZEquipmentSlot Slot(Type, Index);
		if (!GetItemInSlot(Slot))
		{
			return EquipItem(Item, Slot);
		}
	}

	return false;
}

bool UZEquipmentComponent::UnequipItem(UZItemInstance* Item)
{
	if (!GetOwner()->HasAuthority() || !Item)
	{
		return false;
	}

	const FZEquipmentSlot Slot = GetSlotOfItem(Item);
	if (Equipped.RemoveAll([Item](const FZEquippedItem& Entry) { return Entry.Item == Item; }) <= 0)
	{
		return false;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' took '%s' out of %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Item->GetItemDefinition()), *Slot.ToString());
	HandleEquipmentChanged();
	return true;
}

void UZEquipmentComponent::RestoreEquipment(const TArray<TPair<UZItemInstance*, FZEquipmentSlot>>& SavedEquipment, TArray<FString>& OutFixes)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	Equipped.Reset();

	const auto AddEntry = [this](UZItemInstance* Item, const FZEquipmentSlot& Slot)
	{
		FZEquippedItem& Entry = Equipped.AddDefaulted_GetRef();
		Entry.Slot = Slot;
		Entry.Item = Item;
	};

	// first every item that fits the slot it was saved in, then the others in a free slot of their kind
	TArray<UZItemInstance*> WithoutSlot;
	for (const TPair<UZItemInstance*, FZEquipmentSlot>& Saved : SavedEquipment)
	{
		UZItemInstance* Item = Saved.Key;
		const FZEquipmentSlot& Slot = Saved.Value;
		if (!Item || IsEquipped(Item))
		{
			continue;
		}

		if (!Slot.IsValid())
		{
			WithoutSlot.Add(Item);
		}
		else if (CanEquip(Item, Slot) && !GetItemInSlot(Slot))
		{
			AddEntry(Item, Slot);
		}
		else
		{
			OutFixes.Add(FString::Printf(TEXT("'%s' can't be in %s"), *GetNameSafe(Item->GetItemDefinition()), *Slot.ToString()));
			WithoutSlot.Add(Item);
		}
	}

	for (UZItemInstance* Item : WithoutSlot)
	{
		const FGameplayTag Type = Item->GetItemDefinition() ? Item->GetItemDefinition()->GetEquipmentSlotType() : FGameplayTag();
		const int32 NumSlots = UZEquipmentSettings::Get()->GetSlotCount(Type);
		bool bPlaced = false;
		for (int32 Index = 0; Index < NumSlots && !bPlaced; ++Index)
		{
			const FZEquipmentSlot Slot(Type, Index);
			if (CanEquip(Item, Slot) && !GetItemInSlot(Slot))
			{
				AddEntry(Item, Slot);
				bPlaced = true;
			}
		}

		// (an item that can't be equipped at all was already listed above)
		if (!bPlaced && NumSlots > 0)
		{
			OutFixes.Add(FString::Printf(TEXT("'%s' has no free slot, so it's only in the inventory"), *GetNameSafe(Item->GetItemDefinition())));
		}
	}

	HandleEquipmentChanged();
}

void UZEquipmentComponent::OnRep_Equipped()
{
	OnEquipmentChanged.Broadcast(this);
}

void UZEquipmentComponent::HandleItemRemoving(UZInventoryComponent* Inventory, UZItemInstance* Item)
{
	// dropped, used up in a craft, ...: it can't stay equipped
	if (IsEquipped(Item))
	{
		UnequipItem(Item);
	}
}

void UZEquipmentComponent::HandleEquipmentChanged()
{
	ApplyEquipmentEffects();
	OnEquipmentChanged.Broadcast(this);
}

void UZEquipmentComponent::ApplyEquipmentEffects()
{
	UAbilitySystemComponent* ASC = GetAbilitySystem();
	if (!ASC || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (EquipmentEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(EquipmentEffectHandle);
		EquipmentEffectHandle.Invalidate();
	}

	// every equipped item's bonus per attribute, added up
	TMap<FName, float> Bonuses;
	for (const FZEquippedItem& Entry : Equipped)
	{
		const UZItemDefinition* Definition = Entry.Item ? Entry.Item->GetItemDefinition() : nullptr;
		if (!Definition)
		{
			continue;
		}

		for (const FZStatModifier& Modifier : Definition->GetEquippedStatModifiers())
		{
			if (Modifier.Attribute.IsValid())
			{
				Bonuses.FindOrAdd(UZGameplayEffect_Equipment::GetMagnitudeName(Modifier.Attribute)) += Modifier.Value;
			}
		}
	}

	if (Bonuses.IsEmpty())
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZGameplayEffect_Equipment::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	// the effect has a modifier for every stat attribute, so every one gets a value (0 if nothing equipped changes it)
	for (const FGameplayAttribute& Attribute : UZAttributeSet::GetTraitAttributes())
	{
		const FName MagnitudeName = UZGameplayEffect_Equipment::GetMagnitudeName(Attribute);
		Spec.Data->SetSetByCallerMagnitude(MagnitudeName, Bonuses.FindRef(MagnitudeName));
	}

	EquipmentEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

FZEquipmentSlot UZEquipmentComponent::FindSlotFor(const UZItemInstance* Item) const
{
	const UZItemDefinition* Definition = Item ? Item->GetItemDefinition() : nullptr;
	const FGameplayTag Type = Definition ? Definition->GetEquipmentSlotType() : FGameplayTag();
	const int32 NumSlots = UZEquipmentSettings::Get()->GetSlotCount(Type);
	if (NumSlots <= 0)
	{
		return FZEquipmentSlot();
	}

	const FZEquipmentSlot CurrentSlot = GetSlotOfItem(Item);
	if (CurrentSlot.Type == Type)
	{
		return CurrentSlot;
	}

	for (int32 Index = 0; Index < NumSlots; ++Index)
	{
		const FZEquipmentSlot Slot(Type, Index);
		if (!GetItemInSlot(Slot))
		{
			return Slot;
		}
	}

	// all taken: in place of the first one
	return FZEquipmentSlot(Type, 0);
}

UZInventoryComponent* UZEquipmentComponent::GetInventory() const
{
	return UZInventoryComponent::FindInventoryComponent(GetOwner());
}

UAbilitySystemComponent* UZEquipmentComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

FString UZEquipmentComponent::GetDebugString() const
{
	TArray<FString> Entries;
	for (const FZEquipmentSlot& Slot : GetAllSlots())
	{
		if (const UZItemInstance* Item = GetItemInSlot(Slot))
		{
			Entries.Add(FString::Printf(TEXT("%s: %s"), *Slot.GetDisplayName().ToString(), *GetNameSafe(Item->GetItemDefinition())));
		}
	}

	return Entries.Num() > 0 ? FString::Join(Entries, TEXT(", ")) : TEXT("nothing equipped");
}

#undef LOCTEXT_NAMESPACE
