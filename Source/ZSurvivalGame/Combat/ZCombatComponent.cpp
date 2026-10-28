// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/ZCombatComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameCharacter.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayAbility.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Combat/ZCombatSettings.h"
#include "Weapons/ZMeleeMoveset.h"
#include "Weapons/ZMeleeWeaponData.h"
#include "Weapons/ZWeapon.h"

namespace
{
	/** The ability input of an action that has one (dodge, fire, reload) */
	EZAbilityInputID GetActionInputID(EZCombatAction Action)
	{
		switch (Action)
		{
		case EZCombatAction::Dodge:		return EZAbilityInputID::Dodge;
		case EZCombatAction::Fire:		return EZAbilityInputID::Fire;
		case EZCombatAction::Reload:	return EZAbilityInputID::Reload;
		default:						return EZAbilityInputID::None;
		}
	}

	bool IsAttackAction(EZCombatAction Action)
	{
		return Action == EZCombatAction::LightAttack || Action == EZCombatAction::HeavyAttack;
	}

	EZMeleeAttackType GetAttackType(EZCombatAction Action)
	{
		return Action == EZCombatAction::HeavyAttack ? EZMeleeAttackType::Heavy : EZMeleeAttackType::Light;
	}
}

UZCombatComponent::UZCombatComponent()
{
	// only ticks while a press waits in the buffer
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

UAbilitySystemComponent* UZCombatComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

double UZCombatComponent::GetTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

const UZMeleeMoveset* UZCombatComponent::GetMoveset() const
{
	const IZCombatantInterface* Combatant = Cast<IZCombatantInterface>(GetOwner());
	const AZWeapon* Weapon = Combatant ? Combatant->GetHeldWeapon() : nullptr;
	const UZMeleeWeaponData* MeleeData = Weapon ? Weapon->GetMeleeData() : nullptr;
	return MeleeData ? MeleeData->Moveset.Get() : nullptr;
}

void UZCombatComponent::RequestAction(EZCombatAction Action, const FGameplayTag& WeaponSlot)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Action == EZCombatAction::None || !Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (IsActionBlocked(Action))
	{
		// the newest press waits for its turn (an older one is forgotten)
		BufferedAction = Action;
		BufferedWeaponSlot = WeaponSlot;
		BufferedTime = GetTime();
		SetComponentTickEnabled(true);
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' buffered %s"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Action));
		return;
	}

	// a press that can go now replaces one that was still waiting
	ClearBuffer();
	DoAction(Action, WeaponSlot);
}

void UZCombatComponent::ClearBuffer()
{
	BufferedAction = EZCombatAction::None;
	BufferedWeaponSlot = FGameplayTag();
	SetComponentTickEnabled(false);
}

void UZCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (BufferedAction == EZCombatAction::None)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// pressed too long ago: forgotten
	if (GetTime() - BufferedTime > UZCombatSettings::Get()->InputBufferTime)
	{
		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' buffered %s ran out of time"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(BufferedAction));
		ClearBuffer();
		return;
	}

	// its turn: a window opened, or the action before is over
	if (!IsActionBlocked(BufferedAction))
	{
		const EZCombatAction Action = BufferedAction;
		const FGameplayTag WeaponSlot = BufferedWeaponSlot;
		ClearBuffer();

		UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' does buffered %s"), *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(Action));
		DoAction(Action, WeaponSlot);
	}
}

bool UZCombatComponent::IsActionBlocked(EZCombatAction Action) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystem();
	if (!ASC)
	{
		return false;
	}

	// a weapon switch isn't an ability: it waits until the player is free
	if (Action == EZCombatAction::SwitchWeapon)
	{
		return ASC->HasMatchingGameplayTag(ZGameplayTags::State_Busy) || ASC->HasMatchingGameplayTag(ZGameplayTags::State_Staggered);
	}

	// the ability knows which windows let it cut in
	const FGameplayAbilitySpec* Spec = FindActionSpec(Action);
	const UGameplayAbility* AbilitySource = Spec ? (Spec->GetPrimaryInstance() ? Spec->GetPrimaryInstance() : Spec->Ability.Get()) : nullptr;
	const UZGameplayAbility* Ability = Cast<UZGameplayAbility>(AbilitySource);
	return Ability && Ability->bWaitsWhileBusy && Ability->IsBlockedByCommitment(ASC->AbilityActorInfo.Get());
}

void UZCombatComponent::DoAction(EZCombatAction Action, const FGameplayTag& WeaponSlot)
{
	UAbilitySystemComponent* ASC = GetAbilitySystem();
	if (!ASC)
	{
		return;
	}

	if (IsAttackAction(Action))
	{
		// the attack's number in the combo goes with the event (the server checks it may follow the attack before)
		const UZMeleeMoveset* Moveset = GetMoveset();
		if (!Moveset)
		{
			return;
		}

		const EZMeleeAttackType Type = GetAttackType(Action);
		FGameplayEventData Payload;
		Payload.EventTag = Type == EZMeleeAttackType::Light ? ZGameplayTags::Event_Attack_Light : ZGameplayTags::Event_Attack_Heavy;
		Payload.Instigator = GetOwner();
		Payload.Target = GetOwner();
		Payload.EventMagnitude = GetNextAttackStep(Moveset, Type);
		ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
		return;
	}

	if (Action == EZCombatAction::SwitchWeapon)
	{
		if (AZSurvivalGameCharacter* Character = Cast<AZSurvivalGameCharacter>(GetOwner()))
		{
			Character->SelectWeaponSlot(WeaponSlot);
		}

		return;
	}

	// dodge, fire and reload are pressed like their keys (fire stays pressed until the button is let go: the character releases it)
	const int32 InputID = static_cast<int32>(GetActionInputID(Action));
	ASC->AbilityLocalInputPressed(InputID);
	if (Action != EZCombatAction::Fire)
	{
		ASC->AbilityLocalInputReleased(InputID);
	}
}

const FGameplayAbilitySpec* UZCombatComponent::FindActionSpec(EZCombatAction Action) const
{
	UAbilitySystemComponent* ASC = GetAbilitySystem();
	if (!ASC)
	{
		return nullptr;
	}

	// attacks are started by events, so they're found by their tag
	if (IsAttackAction(Action))
	{
		const FGameplayTag AttackTag = Action == EZCombatAction::HeavyAttack ? ZGameplayTags::Ability_Attack_Heavy : ZGameplayTags::Ability_Attack_Light;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetAssetTags().HasTag(AttackTag))
			{
				return &Spec;
			}
		}

		return nullptr;
	}

	const EZAbilityInputID InputID = GetActionInputID(Action);
	return InputID != EZAbilityInputID::None ? ASC->FindAbilitySpecFromInputID(static_cast<int32>(InputID)) : nullptr;
}

int32 UZCombatComponent::GetNextAttackStep(const UZMeleeMoveset* Moveset, EZMeleeAttackType Type) const
{
	// pressed in the current attack's Combo Window: the combo goes on
	if (Moveset && CurrentAttack.IsValid() && LastAttackStep != INDEX_NONE && UZCombatLibrary::IsActionWindowOpen(GetAbilitySystem(), ZGameplayTags::Window_Combo))
	{
		const int32 FollowUp = Moveset->GetFollowUp(LastAttackType, LastAttackStep, Type);
		if (FollowUp != INDEX_NONE)
		{
			return FollowUp;
		}
	}

	return 0;
}

bool UZCombatComponent::IsValidFollowUp(const UZMeleeMoveset* Moveset, EZMeleeAttackType Type, int32 Step) const
{
	if (Step == 0)
	{
		return true;
	}

	// the attack before is still going here, or ended only just (the server's copy runs a little behind the player's machine)
	const bool bRecent = CurrentAttack.IsValid() || GetTime() - LastAttackEndTime <= UZCombatSettings::Get()->ComboTimingTolerance;
	return Moveset && LastAttackStep != INDEX_NONE && bRecent && Moveset->GetFollowUp(LastAttackType, LastAttackStep, Type) == Step;
}

void UZCombatComponent::NotifyAttackStarted(const UGameplayAbility* Attack, EZMeleeAttackType Type, int32 Step)
{
	CurrentAttack = Attack;
	LastAttackType = Type;
	LastAttackStep = Step;
}

void UZCombatComponent::NotifyAttackEnded(const UGameplayAbility* Attack)
{
	// a newer attack may already have taken over
	if (CurrentAttack.Get() == Attack)
	{
		CurrentAttack = nullptr;
		LastAttackEndTime = GetTime();
	}
}

FString UZCombatComponent::GetDebugString() const
{
	TArray<FString> Parts;

	const TCHAR* AttackTypeName = LastAttackType == EZMeleeAttackType::Light ? TEXT("light") : TEXT("heavy");
	if (CurrentAttack.IsValid())
	{
		Parts.Add(FString::Printf(TEXT("attacking (%s %d)"), AttackTypeName, LastAttackStep + 1));
	}
	else if (LastAttackStep != INDEX_NONE)
	{
		Parts.Add(FString::Printf(TEXT("last attack %s %d, %.1f s ago"), AttackTypeName, LastAttackStep + 1, GetTime() - LastAttackEndTime));
	}

	if (BufferedAction != EZCombatAction::None)
	{
		Parts.Add(FString::Printf(TEXT("buffered %s (%.2f s ago)"), *UEnum::GetDisplayValueAsText(BufferedAction).ToString(), GetTime() - BufferedTime));
	}

	if (const UAbilitySystemComponent* ASC = GetAbilitySystem())
	{
		const TPair<FGameplayTag, const TCHAR*> States[] = {
			{ ZGameplayTags::State_Busy, TEXT("busy") },
			{ ZGameplayTags::State_Staggered, TEXT("staggered") },
			{ ZGameplayTags::State_Invulnerable, TEXT("invulnerable") },
		};

		for (const TPair<FGameplayTag, const TCHAR*>& State : States)
		{
			if (ASC->HasMatchingGameplayTag(State.Key))
			{
				Parts.Add(State.Value);
			}
		}

		if (UZCombatLibrary::IsActionWindowOpen(ASC, ZGameplayTags::Window_Combo))
		{
			Parts.Add(TEXT("in combo window"));
		}

		if (UZCombatLibrary::IsActionWindowOpen(ASC, ZGameplayTags::Window_Dodge))
		{
			Parts.Add(TEXT("in dodge window"));
		}
	}

	return Parts.IsEmpty() ? TEXT("free") : FString::Join(Parts, TEXT(", "));
}
