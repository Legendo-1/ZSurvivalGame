// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/ZCombatLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayEffect_Damage.h"
#include "AbilitySystem/ZGameplayEffect_Heal.h"
#include "AbilitySystem/ZGameplayTags.h"

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

bool UZCombatLibrary::ApplyDamage(AActor* Source, AActor* Target, float Damage, const FHitResult& Hit)
{
	if (!Source || !Source->HasAuthority() || Damage <= 0.f || !CanDamage(Source, Target) || IsDead(Target))
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
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

	Spec.Data->SetSetByCallerMagnitude(ZGameplayTags::SetByCaller_Damage, Damage);
	EffectASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	// hit sounds and effects for everyone: the target's own hit cue (e.g. a zombie's differs from a player's)
	FGameplayCueParameters CueParams;
	CueParams.Location = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : Target->GetActorLocation();
	CueParams.Normal = Hit.ImpactNormal;
	CueParams.RawMagnitude = Damage;
	CueParams.Instigator = Source;
	CueParams.EffectCauser = Source;
	CueParams.PhysicalMaterial = Hit.PhysMaterial.Get();
	UZGameplayCueLibrary::ExecuteCue(Target, UZGameplayCueLibrary::GetCharacterCues(Target).HitCue, CueParams);

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
