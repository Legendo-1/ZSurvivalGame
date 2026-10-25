// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "ZSurvivalGameCharacter.h"

namespace
{
	// the free bits of a move's compressed flags that carry our states
	constexpr uint8 FlagSprint = FSavedMove_Character::FLAG_Custom_0;
	constexpr uint8 FlagAim = FSavedMove_Character::FLAG_Custom_1;

	// the speed limit uses the last two bits (FLAG_Custom_2 and FLAG_Custom_3)
	constexpr uint8 SpeedLimitShift = 6;
	constexpr uint8 SpeedLimitMask = 0x3;
}

/** A saved move that also remembers sprint, aim and the speed limit */
class FZSavedMove : public FSavedMove_Character
{
	typedef FSavedMove_Character Super;

public:

	virtual void Clear() override
	{
		Super::Clear();

		bSavedWantsToSprint = false;
		bSavedWantsToAim = false;
		SavedSpeedLimit = EZSpeedLimit::None;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Flags = Super::GetCompressedFlags();

		if (bSavedWantsToSprint)
		{
			Flags |= FlagSprint;
		}

		if (bSavedWantsToAim)
		{
			Flags |= FlagAim;
		}

		Flags |= static_cast<uint8>((static_cast<uint8>(SavedSpeedLimit) & SpeedLimitMask) << SpeedLimitShift);

		return Flags;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		// moves with different states have to reach the server separately
		const FZSavedMove* NewZMove = static_cast<const FZSavedMove*>(NewMove.Get());
		if (bSavedWantsToSprint != NewZMove->bSavedWantsToSprint
			|| bSavedWantsToAim != NewZMove->bSavedWantsToAim
			|| SavedSpeedLimit != NewZMove->SavedSpeedLimit)
		{
			return false;
		}

		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

		// remember the states this move was made with
		if (const UZCharacterMovementComponent* Movement = Cast<UZCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToSprint = Movement->WantsToSprint();
			bSavedWantsToAim = Movement->WantsToAim();
			SavedSpeedLimit = Movement->GetSpeedLimit();
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);

		// replaying this move after a correction: use the states it was made with
		if (UZCharacterMovementComponent* Movement = Cast<UZCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			Movement->SetWantsToSprint(bSavedWantsToSprint);
			Movement->SetWantsToAim(bSavedWantsToAim);
			Movement->SetSpeedLimit(SavedSpeedLimit);
		}
	}

private:

	bool bSavedWantsToSprint = false;
	bool bSavedWantsToAim = false;
	EZSpeedLimit SavedSpeedLimit = EZSpeedLimit::None;
};

/** Client prediction data that saves FZSavedMove moves */
class FZNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
public:

	explicit FZNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FZSavedMove());
	}
};

bool UZCharacterMovementComponent::IsSprinting() const
{
	const AZSurvivalGameCharacter* ZCharacter = Cast<AZSurvivalGameCharacter>(CharacterOwner);

	return bWantsToSprint && !bWantsToAim && SpeedLimit == EZSpeedLimit::None
		&& ZCharacter && ZCharacter->CanSprintInMoveDirection();
}

float UZCharacterMovementComponent::GetMaxSpeed() const
{
	// on the ground and in the air the speed comes from the state (jog, sprint, aim, speed limit)
	const AZSurvivalGameCharacter* ZCharacter = Cast<AZSurvivalGameCharacter>(CharacterOwner);
	const bool bUsesWalkSpeed = MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking || MovementMode == MOVE_Falling;
	if (!ZCharacter || !bUsesWalkSpeed || IsCrouching())
	{
		return Super::GetMaxSpeed();
	}

	return ZCharacter->GetMaxWalkSpeedFor(IsSprinting(), bWantsToAim, SpeedLimit);
}

void UZCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FlagSprint) != 0;
	bWantsToAim = (Flags & FlagAim) != 0;

	// ignore values that aren't a speed limit
	const uint8 SpeedLimitBits = static_cast<uint8>((Flags >> SpeedLimitShift) & SpeedLimitMask);
	SpeedLimit = SpeedLimitBits <= static_cast<uint8>(EZSpeedLimit::Walk) ? static_cast<EZSpeedLimit>(SpeedLimitBits) : EZSpeedLimit::None;
}

FNetworkPredictionData_Client* UZCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UZCharacterMovementComponent* MutableThis = const_cast<UZCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FZNetworkPredictionData_Client(*this);
	}

	return ClientPredictionData;
}

void UZCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Face the camera when always-face-camera is on or while aiming (TODO: also while attacking, once attacks exist), otherwise turn toward movement
	const AZSurvivalGameCharacter* ZCharacter = Cast<AZSurvivalGameCharacter>(CharacterOwner);
	const bool bFaceCamera = bWantsToAim || (ZCharacter && ZCharacter->ShouldAlwaysFaceCamera());

	bOrientRotationToMovement = !bFaceCamera;
	bUseControllerDesiredRotation = bFaceCamera;
}

bool UZCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealWantsToSprint = bWantsToSprint;
	const bool bRealWantsToAim = bWantsToAim;
	const EZSpeedLimit RealSpeedLimit = SpeedLimit;

	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();

	bWantsToSprint = bRealWantsToSprint;
	bWantsToAim = bRealWantsToAim;
	SpeedLimit = RealSpeedLimit;

	return bResult;
}
