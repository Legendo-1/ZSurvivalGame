// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ZSurvivalGameTypes.h"
#include "ZGameplayAbility.generated.h"

class AZSurvivalGameCharacter;
class UZCharacterMovementComponent;

/**
 *  Base class for the player's abilities
 *  Predicted: the owning client starts it right away, the server runs it too and confirms it (or cancels it)
 *
 *  Commitment (souls-like, see COMBAT.md): actions (attacks, shots, dodges) make the character busy (State.Busy) while they run.
 *  Abilities that wait while busy can't start then, unless the busy action's animation is in their Interrupt Window (e.g. the combo window of an attack):
 *  then they start and cut it short. A stagger blocks everything. On a player's machine, what has to wait goes into the input buffer (UZCombatComponent)
 */
UCLASS(Abstract)
class UZGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UZGameplayAbility();

	/** Which input activates this ability; None for abilities started by code or events */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EZAbilityInputID InputID = EZAbilityInputID::None;

	/** --- COMMITMENT --- */

	/** Can't start while the character is busy with another action (e.g. no dodging mid-swing): it waits in the input buffer instead */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Commitment")
	bool bWaitsWhileBusy = false;

	/** May start anyway while the busy action's animation is in this window (e.g. Window.Combo for the next attack, Window.Dodge for a dodge), cutting that action short */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Commitment", meta = (Categories = "Window"))
	FGameplayTag InterruptWindow;

	/** True if it would have to wait for the character's current action now: busy, and not in this ability's Interrupt Window (or staggered) */
	bool IsBlockedByCommitment(const FGameplayAbilityActorInfo* ActorInfo) const;

	/** Also checks the commitment (see bWaitsWhileBusy) */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:

	/**
	 *  Makes it an action: the character is busy while it runs (State.Busy), it waits for other actions, and started in another action's Window
	 *  (its own Interrupt Window, if any) it cuts that action short (they all carry Ability.Type.Action). Call it in the constructor, after setting the asset tags
	 */
	void SetupAsAction(const FGameplayTag& Window);

	/** Adds a tag to the ability's asset tags (what other abilities cancel or block it by) */
	void AddAssetTag(const FGameplayTag& Tag);

	/** The character using this ability, or null if it isn't ours */
	static AZSurvivalGameCharacter* GetZCharacter(const FGameplayAbilityActorInfo* ActorInfo);

	/** The character's movement component, which syncs sprint and aim with the server */
	static UZCharacterMovementComponent* GetZMovement(const FGameplayAbilityActorInfo* ActorInfo);
};
