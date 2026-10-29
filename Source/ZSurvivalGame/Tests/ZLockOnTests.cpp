// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Combat/ZLockOnComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 *  How lock-on scores targets (UZLockOnComponent::ScoreTarget), without a world: run with
 *  UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests ZSurvivalGame.LockOn; Quit" -unattended -nullrhi
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZLockOnScoreTest, "ZSurvivalGame.LockOn.Scoring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZLockOnScoreTest::RunTest(const FString& Parameters)
{
	// a camera 250 cm behind and 50 cm right of the character, looking straight ahead (+X)
	FZLockOnView View;
	View.OwnerLocation = FVector::ZeroVector;
	View.ViewLocation = FVector(-250.f, 50.f, 15.f);
	View.ViewDirection = FVector::ForwardVector;

	constexpr float MaxDistance = 250.f;
	constexpr float MaxAngle = 20.f;
	constexpr float DistanceWeight = 2.f;
	float Score = 0.f;

	TestTrue(TEXT("An enemy straight ahead in range can be locked on to"), UZLockOnComponent::ScoreTarget(View, FVector(200.f, 50.f, 15.f), MaxDistance, MaxAngle, DistanceWeight, Score));
	TestFalse(TEXT("An enemy farther than the lock-on distance can't"), UZLockOnComponent::ScoreTarget(View, FVector(300.f, 50.f, 15.f), MaxDistance, MaxAngle, DistanceWeight, Score));
	TestFalse(TEXT("An enemy behind the character is too far from the middle of the screen"), UZLockOnComponent::ScoreTarget(View, FVector(-150.f, 0.f, 0.f), MaxDistance, MaxAngle, DistanceWeight, Score));
	TestFalse(TEXT("An enemy behind the camera can't, however far the lock-on reaches"), UZLockOnComponent::ScoreTarget(View, FVector(-400.f, 50.f, 15.f), 10000.f, MaxAngle, DistanceWeight, Score));
	TestFalse(TEXT("An enemy far to the side of the screen can't"), UZLockOnComponent::ScoreTarget(View, FVector(0.f, 220.f, 0.f), MaxDistance, MaxAngle, DistanceWeight, Score));
	TestFalse(TEXT("The camera's own location can't"), UZLockOnComponent::ScoreTarget(View, View.ViewLocation, 10000.f, MaxAngle, DistanceWeight, Score));

	// two enemies at the same distance: the one nearer the middle of the screen wins
	float CenterScore = 0.f;
	float SideScore = 0.f;
	UZLockOnComponent::ScoreTarget(View, FVector(200.f, 50.f, 15.f), MaxDistance, MaxAngle, DistanceWeight, CenterScore);
	UZLockOnComponent::ScoreTarget(View, FVector(195.f, 100.f, 15.f), MaxDistance, MaxAngle, DistanceWeight, SideScore);
	TestTrue(TEXT("Nearer the middle of the screen scores better"), CenterScore < SideScore);

	// two enemies in the same direction: the closer one wins
	float NearScore = 0.f;
	float FarScore = 0.f;
	UZLockOnComponent::ScoreTarget(View, FVector(100.f, 50.f, 15.f), MaxDistance, MaxAngle, DistanceWeight, NearScore);
	UZLockOnComponent::ScoreTarget(View, FVector(240.f, 50.f, 15.f), MaxDistance, MaxAngle, DistanceWeight, FarScore);
	TestTrue(TEXT("Closer scores better in the same direction"), NearScore < FarScore);

	// the score is the angle (0 here: straight ahead of the camera) plus the distance weight per meter from the character
	const FVector Ahead(200.f, 50.f, 15.f);
	UZLockOnComponent::ScoreTarget(View, Ahead, MaxDistance, MaxAngle, DistanceWeight, Score);
	TestEqual(TEXT("Straight ahead scores its distance in meters times the weight"), Score, static_cast<float>(DistanceWeight * Ahead.Size() / 100.0), 0.01f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
