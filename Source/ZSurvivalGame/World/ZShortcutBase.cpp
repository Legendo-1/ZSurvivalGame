// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/ZShortcutBase.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "ZSurvivalGame.h"
#include "ZSurvivalGameState.h"
#include "ZSurvivalGameTypes.h"
#include "AbilitySystem/ZGameplayTags.h"

#define LOCTEXT_NAMESPACE "ZShortcut"

AZShortcutBase::AZShortcutBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// replicated so the interaction can name it; whether it's open comes from the world progress on the game state
	bReplicates = true;
	SetReplicateMovement(false);
	NetDormancy = DORM_Initial;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(Root);
	InteractionBox->InitBoxExtent(FVector(120.f, 150.f, 150.f));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ZTraceChannel_Interaction, ECR_Block);
	InteractionBox->SetCanEverAffectNavigation(false);

#if WITH_EDITORONLY_DATA
	OpenSideArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("OpenSideArrow"));
	if (OpenSideArrow)
	{
		OpenSideArrow->SetupAttachment(Root);
		OpenSideArrow->ArrowColor = FColor::Green;
		OpenSideArrow->ArrowSize = 2.f;
		OpenSideArrow->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	}
#endif

	OpenText = LOCTEXT("Open", "Open");
	LockedText = LOCTEXT("Locked", "Won't open from this side");
}

void AZShortcutBase::BeginPlay()
{
	Super::BeginPlay();

	UE_CLOG(!ShortcutTag.MatchesTag(ZGameplayTags::WorldProgress), LogZSurvivalGame, Warning,
		TEXT("Shortcut '%s' has no WorldProgress.Shortcut.* Shortcut Tag: it can't be opened"), *GetName());

	ApplyOpenAlpha(0.f);
	BindToGameState();
}

void AZShortcutBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AZSurvivalGameState* GameState = BoundGameState.Get())
	{
		GameState->OnWorldProgressChanged.RemoveDynamic(this, &AZShortcutBase::HandleWorldProgressChanged);
	}

	GetWorldTimerManager().ClearTimer(BindRetryTimer);

	Super::EndPlay(EndPlayReason);
}

void AZShortcutBase::BindToGameState()
{
	AZSurvivalGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AZSurvivalGameState>() : nullptr;
	if (!GameState)
	{
		// try for 10 s, then the level probably doesn't use the game's game mode
		if (++BindAttempts <= 40)
		{
			GetWorldTimerManager().SetTimer(BindRetryTimer, this, &AZShortcutBase::BindToGameState, 0.25f, false);
		}
		else
		{
			UE_LOG(LogZSurvivalGame, Warning, TEXT("Shortcut '%s': no ZSurvivalGameState in this level, it stays closed"), *GetName());
		}
		return;
	}

	BoundGameState = GameState;
	GameState->OnWorldProgressChanged.AddUniqueDynamic(this, &AZShortcutBase::HandleWorldProgressChanged);

	// already opened in this world: open without moving
	RefreshOpenState(false);
}

void AZShortcutBase::HandleWorldProgressChanged(AZSurvivalGameState* GameState)
{
	// the save's progress arriving right after the level loads shouldn't play the opening
	RefreshOpenState(GetGameTimeSinceCreation() > 1.f);
}

void AZShortcutBase::RefreshOpenState(bool bAnimate)
{
	const AZSurvivalGameState* GameState = BoundGameState.Get();
	const bool bShouldBeOpen = GameState && ShortcutTag.IsValid() && GameState->HasWorldProgress(ShortcutTag);
	if (bShouldBeOpen == bOpen)
	{
		return;
	}

	bOpen = bShouldBeOpen;

	if (bAnimate && OpenDuration > 0.f)
	{
		SetActorTickEnabled(true);
	}
	else
	{
		OpenAlpha = bOpen ? 1.f : 0.f;
		ApplyOpenAlpha(OpenAlpha);
	}

	if (bOpen)
	{
		BP_OnOpened(bAnimate);
	}
}

void AZShortcutBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float TargetAlpha = bOpen ? 1.f : 0.f;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, TargetAlpha, DeltaSeconds, 1.f / FMath::Max(OpenDuration, KINDA_SMALL_NUMBER));
	ApplyOpenAlpha(FMath::SmoothStep(0.f, 1.f, OpenAlpha));

	if (FMath::IsNearlyEqual(OpenAlpha, TargetAlpha))
	{
		SetActorTickEnabled(false);
	}
}

bool AZShortcutBase::IsOnOpenSide(const APawn* Pawn) const
{
	return Pawn && FVector::DotProduct(Pawn->GetActorLocation() - GetActorLocation(), GetActorForwardVector()) >= 0.f;
}

bool AZShortcutBase::CanInteract_Implementation(APawn* Interactor) const
{
	if (!Interactor)
	{
		return false;
	}

	// closed: the prompt shows on both sides ("won't open from this side" on the wrong one)
	return bOpen ? CanInteractWhileOpen(Interactor) : ShortcutTag.IsValid();
}

FText AZShortcutBase::GetInteractionText_Implementation(APawn* Interactor) const
{
	if (bOpen)
	{
		return GetOpenInteractionText(Interactor);
	}

	return IsOnOpenSide(Interactor) ? OpenText : LockedText;
}

void AZShortcutBase::Interact_Implementation(APawn* Interactor)
{
	if (!HasAuthority() || !Interactor)
	{
		return;
	}

	if (bOpen)
	{
		InteractWhileOpen(Interactor);
		return;
	}

	if (!IsOnOpenSide(Interactor))
	{
		return;
	}

	if (AZSurvivalGameState* GameState = BoundGameState.Get())
	{
		UE_LOG(LogZSurvivalGame, Log, TEXT("Shortcut '%s' opened by '%s'"), *GetName(), *GetNameSafe(Interactor));

		// opens it on every machine through OnWorldProgressChanged, and it's saved with the host's game
		GameState->AddWorldProgress(ShortcutTag);
	}
}

#undef LOCTEXT_NAMESPACE
