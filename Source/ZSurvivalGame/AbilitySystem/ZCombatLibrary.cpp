// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AISense_Damage.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayEffect_Damage.h"
#include "AbilitySystem/ZGameplayEffect_Heal.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Combat/ZAnimNotifyState_ActionWindow.h"

FGenericTeamId UZCombatLibrary::GetTeam(const AActor* Actor)
{
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Actor))
	{
		return TeamAgent->GetGenericTeamId();
	}

	// pawns without a team of their own use their controller's
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (const IGenericTeamAgentInterface* ControllerTeamAgent = Cast<IGenericTeamAgentInterface>(Pawn->GetController()))
		{
			return ControllerTeamAgent->GetGenericTeamId();
		}
	}

	return FGenericTeamId::NoTeam;
}

bool UZCombatLibrary::CanDamage(const AActor* Source, const AActor* Target)
{
	if (!Source || !Target || Source == Target)
	{
		return false;
	}

	// anything without a team can be damaged by anyone; otherwise only other teams
	const FGenericTeamId SourceTeam = GetTeam(Source);
	const FGenericTeamId TargetTeam = GetTeam(Target);
	return SourceTeam == FGenericTeamId::NoTeam || TargetTeam == FGenericTeamId::NoTeam || SourceTeam != TargetTeam;
}

bool UZCombatLibrary::IsDead(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return ASC && ASC->HasMatchingGameplayTag(ZGameplayTags::State_Dead);
}

bool UZCombatLibrary::ApplyDamage(AActor* Source, AActor* Target, float Damage, const FHitResult& Hit, float PoiseDamage)
{
	if (!Source || !Source->HasAuthority() || (Damage <= 0.f && PoiseDamage <= 0.f) || !CanDamage(Source, Target) || IsDead(Target))
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);

	// e.g. the first part of a dodge: the hit passes through
	if (TargetASC && TargetASC->HasMatchingGameplayTag(ZGameplayTags::State_Invulnerable))
	{
		return false;
	}

	if (!TargetASC)
	{
		// no ability system: regular engine damage, for Blueprints that react to it (it has no health to lose here)
		const APawn* SourcePawn = Cast<APawn>(Source);
		AController* InstigatorController = SourcePawn ? SourcePawn->GetController() : nullptr;
		const FVector HitDirection = (Hit.TraceEnd - Hit.TraceStart).GetSafeNormal();
		UGameplayStatics::ApplyPointDamage(Target, Damage, HitDirection, Hit, InstigatorController, Source, UDamageType::StaticClass());
		return false;
	}

	// the effect comes from the source's ability system when it has one, so it knows who did it
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
	UAbilitySystemComponent* EffectASC = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = EffectASC->MakeEffectContext();
	Context.AddInstigator(Source, Source);
	Context.AddHitResult(Hit);

	const FGameplayEffectSpecHandle Spec = EffectASC->MakeOutgoingSpec(UZGameplayEffect_Damage::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(ZGameplayTags::SetByCaller_Damage, FMath::Max(Damage, 0.f));
	Spec.Data->SetSetByCallerMagnitude(ZGameplayTags::SetByCaller_PoiseDamage, FMath::Max(PoiseDamage, 0.f));
	EffectASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	// hit sounds and effects for everyone: the target's own hit cue (e.g. a zombie's differs from a player's)
	FGameplayCueParameters CueParams;
	CueParams.Location = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : Target->GetActorLocation();
	CueParams.Normal = Hit.ImpactNormal;
	CueParams.RawMagnitude = UZAttributeSet::GetDamageAfterArmor(Damage, TargetASC->GetNumericAttribute(UZAttributeSet::GetArmorAttribute()));
	CueParams.Instigator = Source;
	CueParams.EffectCauser = Source;
	CueParams.PhysicalMaterial = Hit.PhysMaterial.Get();
	UZGameplayCueLibrary::ExecuteCue(Target, UZGameplayCueLibrary::GetCharacterCues(Target).HitCue, CueParams);

	// an AI that is hit knows who did it and from where, even if it didn't see them (its damage sense; nothing happens for players)
	UAISense_Damage::ReportDamageEvent(Target, Target, Source, CueParams.RawMagnitude, Source->GetActorLocation(), CueParams.Location);

	return true;
}

bool UZCombatLibrary::ApplyHealing(AActor* Source, AActor* Target, float Amount)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Source || !Source->HasAuthority() || !TargetASC || Amount <= 0.f || IsDead(Target))
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Source);
	UAbilitySystemComponent* EffectASC = SourceASC ? SourceASC : TargetASC;

	FGameplayEffectContextHandle Context = EffectASC->MakeEffectContext();
	Context.AddInstigator(Source, Source);

	const FGameplayEffectSpecHandle Spec = EffectASC->MakeOutgoingSpec(UZGameplayEffect_Heal::StaticClass(), 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(ZGameplayTags::SetByCaller_Healing, Amount);
	EffectASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

bool UZCombatLibrary::IsBusy(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return ASC && ASC->HasMatchingGameplayTag(ZGameplayTags::State_Busy);
}

bool UZCombatLibrary::IsStaggered(const AActor* Actor)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	return ASC && ASC->HasMatchingGameplayTag(ZGameplayTags::State_Staggered);
}

bool UZCombatLibrary::Stagger(AActor* Target, AActor* Instigator)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || IsDead(Target))
	{
		return false;
	}

	// the stagger ability listens for this event (a character without one can't be staggered)
	FGameplayEventData Payload;
	Payload.EventTag = ZGameplayTags::Event_Stagger;
	Payload.Instigator = Instigator;
	Payload.Target = Target;
	return ASC->HandleGameplayEvent(Payload.EventTag, &Payload) > 0;
}

bool UZCombatLibrary::IsActionWindowOpen(const UAbilitySystemComponent* AbilitySystem, const FGameplayTag& Window)
{
	if (!AbilitySystem || !Window.IsValid())
	{
		return false;
	}

	// the action's montage (e.g. the attack) as this machine plays it
	const UAnimMontage* Montage = AbilitySystem->GetCurrentMontage();
	const UAnimInstance* AnimInstance = AbilitySystem->AbilityActorInfo.IsValid() ? AbilitySystem->AbilityActorInfo->GetAnimInstance() : nullptr;
	if (!Montage || !AnimInstance)
	{
		return false;
	}

	return UZAnimNotifyState_ActionWindow::IsWindowOpen(Montage, AnimInstance->Montage_GetPosition(Montage), Window);
}
