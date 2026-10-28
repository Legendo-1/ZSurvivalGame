// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/ZRecipeOutcomes.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Crafting/ZCraftingComponent.h"
#include "Crafting/ZCraftingRecipe.h"
#include "Items/ZInventoryComponent.h"
#include "Items/ZItemFragments.h"
#include "Items/ZItemInstance.h"
#include "Weapons/ZWeaponDataBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ZCrafting"

namespace
{
	/** Gives an item back to the inventory, or puts it on the ground if there's no room */
	void GiveBack(UZInventoryComponent* Inventory, UZItemDefinition* Item)
	{
		if (Inventory && Item && Inventory->AddItem(Item, 1) == 0)
		{
			Inventory->DropOnGround(Item, 1);
		}
	}
}

FString UZRecipeOutcome::GetDebugString() const
{
	// the class name without its prefix
	FString Name = GetClass()->GetName();
	Name.RemoveFromStart(TEXT("ZRecipeOutcome_"));
	return FName::NameToDisplayString(Name, false);
}

FText UZRecipeOutcome::GetTargetName(const FZRecipeOutcomeContext& Context)
{
	const UZItemDefinition* Item = Context.Target ? Context.Target->GetItemDefinition() : nullptr;
	return Item ? Item->GetItemName() : FText::GetEmpty();
}

UZRecipeOutcome_UpgradeTarget::UZRecipeOutcome_UpgradeTarget()
{
	Stat = ZGameplayTags::Item_Stat_UpgradeLevel;
	Skill = ZGameplayTags::Tradeskill_Weaponsmithing;
}

int32 UZRecipeOutcome_UpgradeTarget::GetMaxLevel(const UZItemInstance* Target) const
{
	if (MaxLevel > 0)
	{
		return MaxLevel;
	}

	const UZItemFragment_Weapon* Weapon = Target ? Target->FindFragment<UZItemFragment_Weapon>() : nullptr;
	return Weapon && Weapon->WeaponData ? Weapon->WeaponData->MaxUpgradeLevel : 0;
}

bool UZRecipeOutcome_UpgradeTarget::CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const
{
	if (!Context.Target || !Stat.IsValid())
	{
		OutReason = LOCTEXT("UpgradeNothing", "There's nothing to upgrade.");
		return false;
	}

	const int32 Level = Context.Target->GetStatTagStackCount(Stat);
	if (Level >= GetMaxLevel(Context.Target))
	{
		OutReason = FText::Format(LOCTEXT("UpgradeMaxed", "Your {0} can't be upgraded any further."), GetTargetName(Context));
		return false;
	}

	// a hard limit, like recipes: each level needs more skill
	const int32 NeededSkill = Level * SkillPerLevel;
	const int32 SkillLevel = Context.Crafting && Skill.IsValid() ? Context.Crafting->GetSkillLevel(Skill) : 0;
	if (Skill.IsValid() && SkillLevel < NeededSkill)
	{
		OutReason = FText::Format(LOCTEXT("UpgradeSkill", "Upgrading your {0} to +{1} needs {2} {3}. You have {4}."),
			GetTargetName(Context), Level + 1, UZCraftingComponent::GetSkillName(Skill), NeededSkill, SkillLevel);
		return false;
	}

	return true;
}

void UZRecipeOutcome_UpgradeTarget::Apply(const FZRecipeOutcomeContext& Context) const
{
	if (!Context.Target)
	{
		return;
	}

	const int32 NewLevel = FMath::Min(Context.Target->GetStatTagStackCount(Stat) + Amount, GetMaxLevel(Context.Target));
	Context.Target->SetStatTagStack(Stat, NewLevel);

	if (Context.Inventory)
	{
		Context.Inventory->BroadcastChanged();
	}

	if (Context.Notes)
	{
		Context.Notes->Add(FText::Format(LOCTEXT("Upgraded", "Your {0} is now +{1}."), GetTargetName(Context), NewLevel));
	}
}

FString UZRecipeOutcome_UpgradeTarget::GetDebugString() const
{
	return FString::Printf(TEXT("%s +%d%s"), *Super::GetDebugString(), Amount,
		Skill.IsValid() ? *FString::Printf(TEXT(" (%s %d per level)"), *Skill.ToString(), SkillPerLevel) : TEXT(""));
}

#if WITH_EDITOR
void UZRecipeOutcome_UpgradeTarget::ValidateForRecipe(const UZCraftingRecipe* Recipe, FDataValidationContext& Context) const
{
	if (!Stat.IsValid())
	{
		Context.AddError(LOCTEXT("UpgradeNoStat", "Upgrade Target has no Stat to raise."));
	}
}
#endif

UZItemDefinition* UZRecipeOutcome_InstallMod::FindMod(const FZRecipeOutcomeContext& Context)
{
	for (UZItemDefinition* Item : Context.ConsumedItems)
	{
		if (Item && Item->FindFragment<UZItemFragment_WeaponMod>())
		{
			return Item;
		}
	}

	return nullptr;
}

bool UZRecipeOutcome_InstallMod::CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const
{
	const UZItemDefinition* Mod = FindMod(Context);
	const UZItemFragment_Weapon* Weapon = Context.Target ? Context.Target->FindFragment<UZItemFragment_Weapon>() : nullptr;
	if (!Mod || !Weapon || !Weapon->WeaponData)
	{
		OutReason = LOCTEXT("InstallNothing", "There's nothing to install that on.");
		return false;
	}

	if (!Mod->FindFragment<UZItemFragment_WeaponMod>()->FitsWeapon(Weapon->WeaponData))
	{
		OutReason = FText::Format(LOCTEXT("ModDoesntFit", "The {0} doesn't fit your {1}."), Mod->GetItemName(), GetTargetName(Context));
		return false;
	}

	return true;
}

void UZRecipeOutcome_InstallMod::Apply(const FZRecipeOutcomeContext& Context) const
{
	UZItemDefinition* Mod = FindMod(Context);
	if (!Context.Target || !Mod)
	{
		return;
	}

	// the mod is used up as an ingredient; the weapon keeps a reference to what's installed
	UZItemDefinition* OldMod = Context.Target->AttachMod(Mod);
	GiveBack(Context.Inventory, OldMod);

	if (Context.Inventory)
	{
		Context.Inventory->BroadcastChanged();
	}

	if (Context.Notes)
	{
		Context.Notes->Add(FText::Format(LOCTEXT("ModInstalled", "Installed the {0} on your {1}."), Mod->GetItemName(), GetTargetName(Context)));
		if (OldMod)
		{
			Context.Notes->Add(FText::Format(LOCTEXT("OldModBack", "The {0} is back in your inventory."), OldMod->GetItemName()));
		}
	}
}

#if WITH_EDITOR
void UZRecipeOutcome_InstallMod::ValidateForRecipe(const UZCraftingRecipe* Recipe, FDataValidationContext& Context) const
{
	// the mod comes from a used-up input
	const bool bTakesMod = Recipe && Recipe->Inputs.ContainsByPredicate([](const FZRecipeInput& Input)
	{
		return Input.IsConsumed() && (Input.Item ? Input.Item->FindFragment<UZItemFragment_WeaponMod>() != nullptr : Input.ItemTags.HasTag(ZGameplayTags::Item_Type_WeaponMod));
	});

	if (!bTakesMod)
	{
		Context.AddError(LOCTEXT("InstallNoMod", "Install Mod needs a used-up input that's a weapon mod: an item with a Weapon Mod fragment, or any item tagged Item.Type.WeaponMod."));
	}
}
#endif

bool UZRecipeOutcome_RemoveMods::CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const
{
	if (!Context.Target || Context.Target->GetAttachedMods().IsEmpty())
	{
		OutReason = FText::Format(LOCTEXT("NoModsToRemove", "There's nothing to take off your {0}."), GetTargetName(Context));
		return false;
	}

	return true;
}

void UZRecipeOutcome_RemoveMods::Apply(const FZRecipeOutcomeContext& Context) const
{
	if (!Context.Target)
	{
		return;
	}

	TArray<FText> ModNames;
	for (UZItemDefinition* Mod : Context.Target->DetachAllMods())
	{
		GiveBack(Context.Inventory, Mod);
		ModNames.Add(Mod->GetItemName());
	}

	if (Context.Inventory)
	{
		Context.Inventory->BroadcastChanged();
	}

	if (Context.Notes)
	{
		Context.Notes->Add(FText::Format(LOCTEXT("ModsRemoved", "Took {0} off your {1}."), FText::Join(LOCTEXT("ListSeparator", ", "), ModNames), GetTargetName(Context)));
	}
}

bool UZRecipeOutcome_RepairTarget::CanApply(const FZRecipeOutcomeContext& Context, FText& OutReason) const
{
	if (!Context.Target || !Context.Target->HasDurability() || Context.Target->GetDurability() >= Context.Target->GetMaxDurability())
	{
		OutReason = FText::Format(LOCTEXT("NoRepairNeeded", "Your {0} doesn't need repairing."), GetTargetName(Context));
		return false;
	}

	return true;
}

void UZRecipeOutcome_RepairTarget::Apply(const FZRecipeOutcomeContext& Context) const
{
	if (!Context.Target)
	{
		return;
	}

	const int32 MaxDurability = Context.Target->GetMaxDurability();
	const int32 NewDurability = Amount > 0 ? FMath::Min(Context.Target->GetDurability() + Amount, MaxDurability) : MaxDurability;
	Context.Target->SetStatTagStack(ZGameplayTags::Item_Stat_Durability, NewDurability);

	if (Context.Inventory)
	{
		Context.Inventory->BroadcastChanged();
	}

	if (Context.Notes)
	{
		Context.Notes->Add(FText::Format(LOCTEXT("Repaired", "Your {0} is repaired ({1}/{2})."), GetTargetName(Context), NewDurability, MaxDurability));
	}
}

#undef LOCTEXT_NAMESPACE
