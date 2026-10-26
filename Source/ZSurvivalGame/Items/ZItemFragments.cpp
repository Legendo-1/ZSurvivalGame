// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemFragments.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "ZSurvivalGameCharacter.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZItemInstance.h"
#include "Progression/ZProgressionComponent.h"
#include "Weapons/ZWeaponData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZItems"

void UZItemFragment_Weapon::OnInstanceCreated(UZItemInstance* Instance) const
{
	// a new weapon starts with a full magazine
	if (WeaponData)
	{
		Instance->SetStatTagStack(ZGameplayTags::Item_Stat_MagazineAmmo, WeaponData->MagazineSize);
	}
}

bool UZItemFragment_Weapon::OnUse(UZItemInstance* Instance, APawn* User) const
{
	AZSurvivalGameCharacter* Character = Cast<AZSurvivalGameCharacter>(User);
	if (!Character || !WeaponData || Character->GetEquippedItem() == Instance)
	{
		return false;
	}

	Character->EquipItem(Instance);
	return true;
}

FText UZItemFragment_Weapon::GetUseActionName() const
{
	return LOCTEXT("EquipAction", "Equip");
}

#if WITH_EDITOR
void UZItemFragment_Weapon::ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const
{
	if (!WeaponData)
	{
		Context.AddError(LOCTEXT("WeaponNoData", "The Weapon fragment has no Weapon Data."));
	}

	if (Item->IsStackable())
	{
		Context.AddError(LOCTEXT("WeaponStackable", "Weapons keep their own magazine, so they can't stack: set Max Stack Size to 1."));
	}
}
#endif

bool UZItemFragment_WeaponMod::FitsWeapon(const UZWeaponData* WeaponData) const
{
	return WeaponData && Slot.IsValid() && WeaponData->ModSlots.HasTagExact(Slot)
		&& (AllowedWeaponTypes.IsEmpty() || AllowedWeaponTypes.Contains(WeaponData->WeaponType));
}

FText UZItemFragment_WeaponMod::GetModifiersText() const
{
	TArray<FText> Lines;
	for (const FZWeaponStatModifier& Modifier : Modifiers)
	{
		Lines.Add(UZWeaponStatsLibrary::DescribeModifier(Modifier));
	}

	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#if WITH_EDITOR
void UZItemFragment_WeaponMod::ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const
{
	if (!Slot.IsValid())
	{
		Context.AddError(LOCTEXT("ModNoSlot", "The Weapon Mod fragment has no Slot, so it can't be installed."));
	}

	if (Modifiers.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("ModNoModifiers", "The Weapon Mod fragment has no Modifiers, so it changes nothing."));
	}

	if (!Item->ItemTags.HasTag(ZGameplayTags::Item_Type_WeaponMod))
	{
		Context.AddWarning(LOCTEXT("ModNoTypeTag", "Weapon mods need the Item.Type.WeaponMod tag, so recipes that install mods find them."));
	}
}
#endif

bool UZItemFragment_Consumable::OnUse(UZItemInstance* Instance, APawn* User) const
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(User);
	if (!ASC)
	{
		return false;
	}

	// healing at full health would waste the item
	const bool bCanHeal = HealAmount > 0.f && ASC->GetNumericAttribute(UZAttributeSet::GetHealthAttribute()) < ASC->GetNumericAttribute(UZAttributeSet::GetMaxHealthAttribute());
	if (!bCanHeal && !EffectToApply)
	{
		return false;
	}

	if (bCanHeal)
	{
		UZCombatLibrary::ApplyHealing(User, User, HealAmount);
	}

	if (EffectToApply)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(Instance);
		ASC->ApplyGameplayEffectToSelf(EffectToApply.GetDefaultObject(), 1.f, Context);
	}

	return true;
}

FText UZItemFragment_Consumable::GetUseActionName() const
{
	return LOCTEXT("UseAction", "Use");
}

void UZItemFragment_Durability::OnInstanceCreated(UZItemInstance* Instance) const
{
	Instance->SetStatTagStack(ZGameplayTags::Item_Stat_Durability, MaxDurability);
}

#if WITH_EDITOR
void UZItemFragment_Durability::ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const
{
	if (Item->IsStackable())
	{
		Context.AddError(LOCTEXT("DurabilityStackable", "Items with durability keep their own durability, so they can't stack: set Max Stack Size to 1."));
	}
}
#endif

bool UZItemFragment_Progression::OnUse(UZItemInstance* Instance, APawn* User) const
{
	UZProgressionComponent* Progression = UZProgressionComponent::FindProgressionComponent(User);
	if (!Progression || (TraitPoints <= 0 && Experience <= 0))
	{
		return false;
	}

	Progression->AddTraitPoints(TraitPoints);
	Progression->AddExperience(Experience);
	return true;
}

FText UZItemFragment_Progression::GetUseActionName() const
{
	return LOCTEXT("ReadAction", "Read");
}

#if WITH_EDITOR
void UZItemFragment_Tool::ValidateForItem(const UZItemDefinition* Item, FDataValidationContext& Context) const
{
	if (!Item->ItemTags.HasTag(ZGameplayTags::Item_Type_Tool))
	{
		Context.AddWarning(LOCTEXT("ToolNoTypeTag", "The item has a Tool fragment but no Item.Type.Tool tag."));
	}

	const FGameplayTag ToolParent = FGameplayTag::RequestGameplayTag(TEXT("Item.Tool"));
	if (!Item->ItemTags.HasTag(ToolParent))
	{
		Context.AddWarning(LOCTEXT("ToolNoKindTag", "The item has a Tool fragment but no Item.Tool.* tag (e.g. Item.Tool.Screwdriver), so recipes can only ask for this exact item."));
	}
}
#endif

#undef LOCTEXT_NAMESPACE
