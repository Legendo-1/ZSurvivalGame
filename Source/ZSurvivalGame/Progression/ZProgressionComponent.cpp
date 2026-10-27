// Copyright Epic Games, Inc. All Rights Reserved.

#include "Progression/ZProgressionComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "ZSurvivalGame.h"
#include "AbilitySystem/ZAttributeSet.h"
#include "AbilitySystem/ZGameplayCueLibrary.h"
#include "AbilitySystem/ZGameplayEffect_Traits.h"
#include "AbilitySystem/ZGameplayTags.h"
#include "Progression/ZProgressionSettings.h"
#include "Progression/ZProgressionSubsystem.h"
#include "Progression/ZTraitDefinition.h"

void FZTraitRankList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
	if (OwnerComponent)
	{
		OwnerComponent->HandleTraitsReplicated();
	}
}

UZProgressionComponent::UZProgressionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TraitRanks(this)
{
	SetIsReplicatedByDefault(true);
}

UZProgressionComponent* UZProgressionComponent::FindProgressionComponent(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UZProgressionComponent* OwnProgression = Actor->FindComponentByClass<UZProgressionComponent>())
	{
		return OwnProgression;
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

	return PlayerState ? PlayerState->FindComponentByClass<UZProgressionComponent>() : nullptr;
}

void UZProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// only the player themselves needs their experience, points and traits (the bonuses replicate as attributes)
	DOREPLIFETIME_CONDITION(UZProgressionComponent, Experience, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UZProgressionComponent, BonusTraitPoints, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UZProgressionComponent, TraitRanks, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UZProgressionComponent, RestoreCount, COND_OwnerOnly);
}

void UZProgressionComponent::BeginPlay()
{
	Super::BeginPlay();

	SeenLevel = GetLevel();
	SeenBonusTraitPoints = BonusTraitPoints;
}

void UZProgressionComponent::PostRepNotifies()
{
	Super::PostRepNotifies();

	// after the OnReps, which checked it: the numbers that came with a restore count as seen now
	SeenRestoreCount = RestoreCount;
}

int32 UZProgressionComponent::GetLevel() const
{
	return UZProgressionSettings::Get()->GetCharacterLevelForExperience(Experience);
}

float UZProgressionComponent::GetLevelProgress() const
{
	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 Level = GetLevel();
	if (Level >= Settings->MaxCharacterLevel)
	{
		return 1.f;
	}

	const int32 LevelStart = Settings->GetCharacterExperienceForLevel(Level);
	const int32 LevelEnd = Settings->GetCharacterExperienceForLevel(Level + 1);
	return LevelEnd > LevelStart ? FMath::Clamp(static_cast<float>(Experience - LevelStart) / (LevelEnd - LevelStart), 0.f, 1.f) : 1.f;
}

void UZProgressionComponent::AddExperience(int32 Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0)
	{
		return;
	}

	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 OldLevel = GetLevel();
	const int64 MaxExperience = Settings->GetCharacterExperienceForLevel(Settings->MaxCharacterLevel);
	Experience = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Experience) + Amount, MaxExperience));

	const int32 NewLevel = GetLevel();
	UE_LOG(LogZSurvivalGame, Verbose, TEXT("'%s' got %d experience (%d total, level %d)"), *GetNameSafe(GetOwner()), Amount, Experience, NewLevel);

	// the owner's machine finds out in OnRep_Experience
	if (NewLevel > OldLevel)
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' reached level %d"), *GetNameSafe(GetOwner()), NewLevel);
		OnLevelUp.Broadcast(this, NewLevel);

		// a level-up sound and effect on the player's character, for everyone
		FGameplayCueParameters LevelUpCue;
		LevelUpCue.RawMagnitude = NewLevel;
		UZGameplayCueLibrary::ExecuteCue(GetOwner(), ZGameplayTags::GameplayCue_Character_LevelUp, LevelUpCue);
	}

	OnProgressionChanged.Broadcast(this);
}

int32 UZProgressionComponent::GetEarnedTraitPoints() const
{
	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 Earned = (GetLevel() - 1) * Settings->TraitPointsPerLevel + BonusTraitPoints;
	return Settings->MaxTraitPoints > 0 ? FMath::Min(Earned, Settings->MaxTraitPoints) : Earned;
}

int32 UZProgressionComponent::GetSpentTraitPoints() const
{
	int32 Spent = 0;
	for (const FZTraitRank& Entry : TraitRanks.Entries)
	{
		if (Entry.Trait)
		{
			Spent += Entry.Rank * Entry.Trait->PointsPerRank;
		}
	}

	return Spent;
}

int32 UZProgressionComponent::GetUnspentTraitPoints() const
{
	return FMath::Max(GetEarnedTraitPoints() - GetSpentTraitPoints(), 0);
}

void UZProgressionComponent::AddTraitPoints(int32 Points)
{
	if (!GetOwner()->HasAuthority() || Points <= 0)
	{
		return;
	}

	// over the limit (if there is one), the extra points are lost
	const int32 EarnedBefore = GetEarnedTraitPoints();
	BonusTraitPoints += Points;
	const int32 Gained = GetEarnedTraitPoints() - EarnedBefore;
	if (Gained <= 0)
	{
		BonusTraitPoints -= Points;
		return;
	}

	UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' got %d trait point(s), %d unspent"), *GetNameSafe(GetOwner()), Gained, GetUnspentTraitPoints());
	OnBonusTraitPoints.Broadcast(this, Gained);
	OnProgressionChanged.Broadcast(this);
}

int32 UZProgressionComponent::GetTraitRank(const UZTraitDefinition* Trait) const
{
	const FZTraitRank* Entry = TraitRanks.Entries.FindByPredicate([Trait](const FZTraitRank& Candidate) { return Candidate.Trait == Trait; });
	return Entry ? Entry->Rank : 0;
}

bool UZProgressionComponent::CanRaiseTrait(const UZTraitDefinition* Trait) const
{
	return Trait && GetTraitRank(Trait) < Trait->MaxRank && GetUnspentTraitPoints() >= Trait->PointsPerRank;
}

void UZProgressionComponent::RaiseTrait(UZTraitDefinition* Trait)
{
	// on the server this runs right away
	ServerRaiseTrait(Trait);
}

bool UZProgressionComponent::CanResetTraits() const
{
	return UZProgressionSettings::Get()->bAllowTraitReset && GetSpentTraitPoints() > 0;
}

void UZProgressionComponent::ResetTraits()
{
	ServerResetTraits();
}

TArray<UZTraitDefinition*> UZProgressionComponent::GetAllTraits() const
{
	const UZProgressionSubsystem* Subsystem = UZProgressionSubsystem::Get(this);
	return Subsystem ? Subsystem->GetAllTraits() : TArray<UZTraitDefinition*>();
}

TArray<TPair<UZTraitDefinition*, int32>> UZProgressionComponent::GetTraitRanks() const
{
	TArray<TPair<UZTraitDefinition*, int32>> Ranks;
	for (const FZTraitRank& Entry : TraitRanks.Entries)
	{
		if (Entry.Trait && Entry.Rank > 0)
		{
			Ranks.Emplace(Entry.Trait, Entry.Rank);
		}
	}

	return Ranks;
}

void UZProgressionComponent::RestoreProgress(int32 InExperience, int32 InBonusTraitPoints, const TArray<TPair<UZTraitDefinition*, int32>>& InTraitRanks, TArray<FString>& OutFixes)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	const UZProgressionSettings* Settings = UZProgressionSettings::Get();
	const int32 MaxExperience = Settings->GetCharacterExperienceForLevel(Settings->MaxCharacterLevel);
	Experience = FMath::Clamp(InExperience, 0, MaxExperience);
	if (Experience != InExperience)
	{
		OutFixes.Add(FString::Printf(TEXT("experience %d is outside 0..%d (the highest level)"), InExperience, MaxExperience));
	}

	// with a limit on trait points, bonus points over it are lost, as when they're earned (and a sane top either way, so sums can't overflow)
	const int32 PointsFromLevels = (GetLevel() - 1) * Settings->TraitPointsPerLevel;
	const int32 MaxBonusPoints = Settings->MaxTraitPoints > 0 ? FMath::Max(Settings->MaxTraitPoints - PointsFromLevels, 0) : 100000;
	BonusTraitPoints = FMath::Clamp(InBonusTraitPoints, 0, MaxBonusPoints);
	if (BonusTraitPoints != InBonusTraitPoints)
	{
		OutFixes.Add(FString::Printf(TEXT("bonus trait points %d are outside 0..%d"), InBonusTraitPoints, MaxBonusPoints));
	}

	TraitRanks.Entries.Reset();
	for (const TPair<UZTraitDefinition*, int32>& Saved : InTraitRanks)
	{
		UZTraitDefinition* Trait = Saved.Key;
		if (!Trait)
		{
			continue;
		}

		const int32 Rank = FMath::Clamp(Saved.Value, 0, Trait->MaxRank);
		if (Rank != Saved.Value)
		{
			OutFixes.Add(FString::Printf(TEXT("'%s' rank %d is outside 0..%d"), *GetNameSafe(Trait), Saved.Value, Trait->MaxRank));
		}

		if (Rank <= 0)
		{
			continue;
		}

		if (GetTraitRank(Trait) > 0)
		{
			OutFixes.Add(FString::Printf(TEXT("'%s' is in the save twice (the first one counts)"), *GetNameSafe(Trait)));
			continue;
		}

		FZTraitRank& Entry = TraitRanks.Entries.AddDefaulted_GetRef();
		Entry.Trait = Trait;
		Entry.Rank = Rank;
	}

	// more spent than earned: the player chooses again
	if (GetSpentTraitPoints() > GetEarnedTraitPoints())
	{
		OutFixes.Add(FString::Printf(TEXT("the traits cost %d points but the character has %d, so they were reset (the points can be spent again)"), GetSpentTraitPoints(), GetEarnedTraitPoints()));
		TraitRanks.Entries.Reset();
	}

	TraitRanks.MarkArrayDirty();
	++RestoreCount;

	ApplyTraitEffects();
	OnProgressionChanged.Broadcast(this);
}

void UZProgressionComponent::ServerRaiseTrait_Implementation(UZTraitDefinition* Trait)
{
	if (!CanRaiseTrait(Trait))
	{
		return;
	}

	FZTraitRank* Entry = TraitRanks.Entries.FindByPredicate([Trait](const FZTraitRank& Candidate) { return Candidate.Trait == Trait; });
	if (!Entry)
	{
		Entry = &TraitRanks.Entries.AddDefaulted_GetRef();
		Entry->Trait = Trait;
	}

	++Entry->Rank;
	TraitRanks.MarkItemDirty(*Entry);

	UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' raised '%s' to rank %d"), *GetNameSafe(GetOwner()), *GetNameSafe(Trait), Entry->Rank);
	ApplyTraitEffects();
	OnProgressionChanged.Broadcast(this);
}

void UZProgressionComponent::ServerResetTraits_Implementation()
{
	if (!CanResetTraits())
	{
		return;
	}

	TraitRanks.Entries.Reset();
	TraitRanks.MarkArrayDirty();

	UE_LOG(LogZSurvivalGame, Log, TEXT("'%s' reset their traits (%d points to spend)"), *GetNameSafe(GetOwner()), GetUnspentTraitPoints());
	ApplyTraitEffects();
	OnProgressionChanged.Broadcast(this);
}

void UZProgressionComponent::OnRep_Experience()
{
	// a saved character's experience arriving isn't a level-up
	const int32 Level = GetLevel();
	if (SeenLevel != INDEX_NONE && Level > SeenLevel && RestoreCount == SeenRestoreCount)
	{
		OnLevelUp.Broadcast(this, Level);
	}

	SeenLevel = Level;
	OnProgressionChanged.Broadcast(this);
}

void UZProgressionComponent::OnRep_BonusTraitPoints()
{
	if (SeenBonusTraitPoints != INDEX_NONE && BonusTraitPoints > SeenBonusTraitPoints && RestoreCount == SeenRestoreCount)
	{
		OnBonusTraitPoints.Broadcast(this, BonusTraitPoints - SeenBonusTraitPoints);
	}

	SeenBonusTraitPoints = BonusTraitPoints;
	OnProgressionChanged.Broadcast(this);
}

void UZProgressionComponent::ApplyTraitEffects()
{
	UAbilitySystemComponent* ASC = GetAbilitySystem();
	if (!ASC || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (TraitEffectHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(TraitEffectHandle);
		TraitEffectHandle.Invalidate();
	}

	// every trait's bonus per attribute, added up
	TMap<FName, float> Bonuses;
	for (const FZTraitRank& Entry : TraitRanks.Entries)
	{
		if (!Entry.Trait || Entry.Rank <= 0)
		{
			continue;
		}

		for (const FZTraitModifier& Modifier : Entry.Trait->Modifiers)
		{
			if (Modifier.Attribute.IsValid())
			{
				Bonuses.FindOrAdd(UZGameplayEffect_Traits::GetMagnitudeName(Modifier.Attribute)) += Modifier.ValuePerRank * Entry.Rank;
			}
		}
	}

	if (Bonuses.IsEmpty())
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UZGameplayEffect_Traits::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}

	// the effect has a modifier for every attribute traits can change, so every one gets a value (0 if no trait changes it)
	for (const FGameplayAttribute& Attribute : UZAttributeSet::GetTraitAttributes())
	{
		const FName MagnitudeName = UZGameplayEffect_Traits::GetMagnitudeName(Attribute);
		Spec.Data->SetSetByCallerMagnitude(MagnitudeName, Bonuses.FindRef(MagnitudeName));
	}

	TraitEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UZProgressionComponent::HandleTraitsReplicated()
{
	OnProgressionChanged.Broadcast(this);
}

UAbilitySystemComponent* UZProgressionComponent::GetAbilitySystem() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

FString UZProgressionComponent::GetDebugString() const
{
	TArray<FString> Ranks;
	for (const FZTraitRank& Entry : TraitRanks.Entries)
	{
		if (Entry.Trait && Entry.Rank > 0)
		{
			Ranks.Add(FString::Printf(TEXT("%s %d/%d"), *Entry.Trait->GetTraitName().ToString(), Entry.Rank, Entry.Trait->MaxRank));
		}
	}

	return FString::Printf(TEXT("level %d (%d xp, %.0f%% to next), trait points: %d earned, %d unspent; traits: %s"), GetLevel(), Experience, GetLevelProgress() * 100.f,
		GetEarnedTraitPoints(), GetUnspentTraitPoints(), Ranks.Num() > 0 ? *FString::Join(Ranks, TEXT(", ")) : TEXT("none"));
}
