// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "ZGameplayCueLibrary.generated.h"

/** The cues of one kind of character, so e.g. a zombie and a player sound different when they're hit or die */
USTRUCT(BlueprintType)
struct FZCharacterCues
{
	GENERATED_BODY()

	FZCharacterCues();

	/** Taking damage, e.g. blood and a grunt (Location = where it hit, Raw Magnitude = damage). Empty = none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Character.Hit"))
	FGameplayTag HitCue;

	/** Dying (Instigator = the killer). Empty = none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects", meta = (Categories = "GameplayCue.Character.Death"))
	FGameplayTag DeathCue;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UZCharacterCuesInterface : public UInterface
{
	GENERATED_BODY()
};

/** Characters (players, enemies) give their own cues to the code that hits or kills them */
class IZCharacterCuesInterface
{
	GENERATED_BODY()

public:

	virtual const FZCharacterCues& GetCharacterCues() const = 0;
};

/**
 *  Plays GameplayCues: the moments sounds, particles and camera shakes are hung on. C++ (or a Blueprint) only says when, with a tag;
 *  what plays is a GameplayCue Notify asset with that tag in Content/GameplayCues, made in the editor (see AUDIO.md)
 *  Cues go through the target's ability system, so every player near it sees and hears them
 */
UCLASS()
class UZGameplayCueLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 *  Plays a one-off cue on an actor (a shot, a death, a pickup) for every player
	 *  Call it on the server; or inside an ability on the owning player's machine, which plays it there at once (the server sends it to the others)
	 *  Does nothing and returns false if the tag is empty, the actor has no ability system, or it's called on a client outside an ability
	 */
	UFUNCTION(BlueprintCallable, Category = "Effects", meta = (DefaultToSelf = "Target", AutoCreateRefTerm = "Parameters"))
	static bool ExecuteCue(AActor* Target, FGameplayTag CueTag, const FGameplayCueParameters& Parameters);

	/** Server: starts a cue that lasts until RemoveCue, e.g. a looping sound (players who come near later get it too) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Effects", meta = (DefaultToSelf = "Target", AutoCreateRefTerm = "Parameters"))
	static bool AddCue(AActor* Target, FGameplayTag CueTag, const FGameplayCueParameters& Parameters);

	/** Server: stops a cue started with AddCue */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Effects", meta = (DefaultToSelf = "Target"))
	static void RemoveCue(AActor* Target, FGameplayTag CueTag);

	/** A character's own hit and death cues, or the general ones if it has none */
	UFUNCTION(BlueprintPure, Category = "Effects")
	static FZCharacterCues GetCharacterCues(const AActor* Character);
};
