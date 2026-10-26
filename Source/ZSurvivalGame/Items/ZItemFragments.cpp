// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/ZItemFragments.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Items/ZItemInstance.h"
#include "Weapons/ZWeaponData.h"

void UZItemFragment_Weapon::OnInstanceCreated(UZItemInstance* Instance) const
{
	// a new weapon starts with a full magazine
	if (WeaponData)
	{
		Instance->SetStatTagStack(ZGameplayTags::Item_Stat_MagazineAmmo, WeaponData->MagazineSize);
	}
}

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
