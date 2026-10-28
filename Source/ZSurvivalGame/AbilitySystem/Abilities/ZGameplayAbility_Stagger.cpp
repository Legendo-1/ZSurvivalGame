// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/Abilities/ZGameplayAbility_Stagger.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameCharacter.h"
#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Combat/ZCombatSettings.h"

UZGameplayAbility_Stagger::UZGameplayAbility_Stagger()
{
	// the server decides it (poise lives there); a player's machine then runs it too, and only the server ends it early. A new stagger starts it over
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	bRetriggerInstancedAbility = true;

	SetAssetTags(FGameplayTagContainer(ZGameplayTags::Ability_Stagger.GetTag()));

	// staggered and busy: nothing else starts meanwhile (the player's presses wait in the input buffer)
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Staggered);
	ActivationOwnedTags.AddTag(ZGameplayTags::State_Busy);

	// a staggered character can be staggered again (it starts over)
	ActivationBlockedTags.RemoveTag(ZGameplayTags::State_Staggered);

	// it cuts short everything else: attacks, dodges, shots, reloads, sprinting, aiming
	CancelAbilitiesWithTag.AddTag(ZGameplayTags::Ability);

	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = ZGameplayTags::Event_Stagger;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UZGameplayAbility_Stagger::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (!Avatar || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// the character's stagger sound and effect, for everyone
	if (ActorInfo->IsNetAuthority())
	{
		FGameplayCueParameters StaggerCue;
		StaggerCue.Location = Avatar->GetActorLocation();
		StaggerCue.Instigator = TriggerEventData ? const_cast<AActor*>(TriggerEventData->Instigator.Get()) : nullptr;
		StaggerCue.EffectCauser = StaggerCue.Instigator;
		UZGameplayCueLibrary::ExecuteCue(Avatar, UZGameplayCueLibrary::GetCharacterCues(Avatar).StaggerCue, StaggerCue);
	}

	// no walking: a player through the speed limit (it reaches the server with their moves), AI stops where it is
	AZSurvivalGameCharacter* Player = GetZCharacter(ActorInfo);
	ACharacter* Character = Cast<ACharacter>(Avatar);
	if (Player)
	{
		Player->SetSpeedLimit(EZSpeedLimit::Still);
	}
	else if (Character && ActorInfo->IsNetAuthority())
	{
		if (AAIController* AIController = Cast<AAIController>(Character->GetController()))
		{
			AIController->StopMovement();
		}

		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	// the stagger animation; a player's must stay in place, since the server starts it before their machine knows
	const IZCombatantInterface* Combatant = Cast<IZCombatantInterface>(Avatar);
	UAnimMontage* StaggerMontage = Combatant ? Combatant->GetStaggerMontage() : nullptr;
	if (StaggerMontage && Player && StaggerMontage->HasRootMotion())
	{
		UE_LOG(LogZSurvivalGame, Warning, TEXT("'%s' doesn't play the stagger animation '%s': a player's stagger must stay in place (root motion off)"), *GetNameSafe(Avatar), *GetNameSafe(StaggerMontage));
		StaggerMontage = nullptr;
	}

	// over when the animation starts blending out; without one, a pause
	float Duration = 0.f;
	if (StaggerMontage)
	{
		const float MontageLength = ActorInfo->AbilitySystemComponent->PlayMontage(this, ActivationInfo, StaggerMontage, 1.f);
		if (MontageLength > 0.f)
		{
			Duration = FMath::Max(MontageLength - StaggerMontage->GetDefaultBlendOutTime(), 0.1f);
		}
	}

	if (Duration <= 0.f)
	{
		Duration = UZCombatSettings::Get()->DefaultStaggerDuration;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' staggered for %.2f s (by '%s')"), *GetNameSafe(Avatar), Duration, TriggerEventData ? *GetNameSafe(TriggerEventData->Instigator.Get()) : TEXT("?"));

	UAbilityTask_WaitDelay* WaitStagger = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	WaitStagger->OnFinish.AddDynamic(this, &UZGameplayAbility_Stagger::OnStaggerFinished);
	WaitStagger->ReadyForActivation();
}

void UZGameplayAbility_Stagger::OnStaggerFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UZGameplayAbility_Stagger::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (AZSurvivalGameCharacter* Player = GetZCharacter(ActorInfo))
		{
			Player->SetSpeedLimit(EZSpeedLimit::None);
		}

		// cut short (e.g. by dying): the animation stops too
		if (bWasCancelled)
		{
			MontageStop();
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
