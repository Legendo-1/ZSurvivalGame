// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "AI/ZAITargetingComponent.h"
#include "AI/ZAITypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A zombie-like enemy 1 m from a target it sees, ready to attack */
	FZAICombatSnapshot MakeZombieInRange()
	{
		FZAICombatSnapshot Snapshot;
		Snapshot.bHasTarget = true;
		Snapshot.bTargetVisible = true;
		Snapshot.DistanceToTarget = 100.f;
		Snapshot.AttackRange = 150.f;
		Snapshot.bAttackReady = true;
		Snapshot.bCanStrafe = false;
		Snapshot.bStrafeTimeOver = true;
		return Snapshot;
	}

	/** A knight-like enemy that circles at 3 m */
	FZAICombatSnapshot MakeStrafer(float Distance, bool bStrafeTimeOver, bool bAttackReady)
	{
		FZAICombatSnapshot Snapshot = MakeZombieInRange();
		Snapshot.bCanStrafe = true;
		Snapshot.StrafeDistance = 300.f;
		Snapshot.DistanceToTarget = Distance;
		Snapshot.bStrafeTimeOver = bStrafeTimeOver;
		Snapshot.bAttackReady = bAttackReady;
		return Snapshot;
	}
}

/**
 *  The AI combat state machine's decisions (ZAICombatRules), without a world: run with
 *  UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests ZSurvivalGame.AI; Quit" -unattended -nullrhi
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZAICombatRulesTest, "ZSurvivalGame.AI.CombatRules", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZAICombatRulesTest::RunTest(const FString& Parameters)
{
	using namespace ZAICombatRules;

	// failures name the states
	const auto ExpectState = [this](const TCHAR* What, EZAICombatState Actual, EZAICombatState Expected)
	{
		TestEqual(What, FString(LexToString(Actual)), FString(LexToString(Expected)));
	};

	// --- approaching ---
	FZAICombatSnapshot Snapshot = MakeZombieInRange();
	ExpectState(TEXT("In range, seen and ready: attack"), WhileApproaching(Snapshot), EZAICombatState::Attacking);

	Snapshot.bAttackReady = false;
	ExpectState(TEXT("In range but on cooldown (no strafing): hold and keep approaching"), WhileApproaching(Snapshot), EZAICombatState::Approaching);

	Snapshot = MakeZombieInRange();
	Snapshot.bTargetVisible = false;
	ExpectState(TEXT("In range but not seen: no attack"), WhileApproaching(Snapshot), EZAICombatState::Approaching);

	Snapshot = MakeZombieInRange();
	Snapshot.DistanceToTarget = 600.f;
	ExpectState(TEXT("Out of range: keep approaching"), WhileApproaching(Snapshot), EZAICombatState::Approaching);

	Snapshot.bHasTarget = false;
	ExpectState(TEXT("No target: idle"), WhileApproaching(Snapshot), EZAICombatState::Idle);

	// a target up on a ledge right above it: no swinging at the air, it looks for a way up
	Snapshot = MakeZombieInRange();
	Snapshot.HeightToTarget = 150.f;
	ExpectState(TEXT("Target on a ledge above, close on the ground: no attack, keep approaching"), WhileApproaching(Snapshot), EZAICombatState::Approaching);
	TestFalse(TEXT("A target on a ledge above isn't in attack range"), IsInAttackRange(Snapshot));

	Snapshot.HeightToTarget = -150.f;
	ExpectState(TEXT("Target down below a ledge: no attack, keep approaching"), WhileApproaching(Snapshot), EZAICombatState::Approaching);

	Snapshot.HeightToTarget = 40.f;
	ExpectState(TEXT("Target a step higher: attack"), WhileApproaching(Snapshot), EZAICombatState::Attacking);

	FZAICombatSnapshot OnLedge = MakeStrafer(300.f, false, true);
	OnLedge.HeightToTarget = 200.f;
	ExpectState(TEXT("Strafer, target on a ledge: no circling below it, find the way up"), WhileApproaching(OnLedge), EZAICombatState::Approaching);
	ExpectState(TEXT("Circling, the target jumped onto a ledge: go after it"), WhileStrafing(OnLedge), EZAICombatState::Approaching);
	ExpectState(TEXT("Strafer after an attack, the target on a ledge: go after it"), AfterAction(OnLedge), EZAICombatState::Approaching);

	ExpectState(TEXT("Strafer close, still has to wait: circle"), WhileApproaching(MakeStrafer(350.f, false, true)), EZAICombatState::Strafing);
	ExpectState(TEXT("Strafer far, still has to wait: keep approaching"), WhileApproaching(MakeStrafer(900.f, false, true)), EZAICombatState::Approaching);
	ExpectState(TEXT("Strafer in range, done waiting: attack"), WhileApproaching(MakeStrafer(120.f, true, true)), EZAICombatState::Attacking);

	// --- strafing ---
	ExpectState(TEXT("Circling, not done: keep circling"), WhileStrafing(MakeStrafer(300.f, false, true)), EZAICombatState::Strafing);
	ExpectState(TEXT("Circling, done and ready: go in"), WhileStrafing(MakeStrafer(300.f, true, true)), EZAICombatState::Approaching);
	ExpectState(TEXT("Circling, done but on cooldown: keep circling"), WhileStrafing(MakeStrafer(300.f, true, false)), EZAICombatState::Strafing);
	ExpectState(TEXT("The target got away: chase it"), WhileStrafing(MakeStrafer(300.f + StrafeLeash + 50.f, false, true)), EZAICombatState::Approaching);

	FZAICombatSnapshot NoTarget = MakeStrafer(300.f, false, true);
	NoTarget.bHasTarget = false;
	ExpectState(TEXT("Circling, target gone: idle"), WhileStrafing(NoTarget), EZAICombatState::Idle);

	// --- after an attack, a stagger or a dodge ---
	ExpectState(TEXT("Zombie after an attack: approach again"), AfterAction(MakeZombieInRange()), EZAICombatState::Approaching);
	ExpectState(TEXT("Strafer after an attack: circle"), AfterAction(MakeStrafer(150.f, false, false)), EZAICombatState::Strafing);
	ExpectState(TEXT("Strafer after a stagger, done waiting: go in"), AfterAction(MakeStrafer(150.f, true, true)), EZAICombatState::Approaching);
	ExpectState(TEXT("After anything, target gone: idle"), AfterAction(NoTarget), EZAICombatState::Idle);

	return true;
}

/** How the AI weighs the enemies it knows about (UZAITargetingComponent::ScoreEnemy) */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZAITargetScoringTest, "ZSurvivalGame.AI.TargetScoring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZAITargetScoringTest::RunTest(const FString& Parameters)
{
	const FZAITargetScoring Scoring;

	// nobody hurt it: the closer one wins
	const float Near = UZAITargetingComponent::ScoreEnemy(200.f, 0.f, true, false, Scoring);
	const float Far = UZAITargetingComponent::ScoreEnemy(1000.f, 0.f, true, false, Scoring);
	TestTrue(TEXT("Closer wins when nobody hurt it"), Near > Far);

	// seen beats only remembered, at the same distance
	TestTrue(TEXT("Seen beats remembered"), UZAITargetingComponent::ScoreEnemy(500.f, 0.f, true, false, Scoring) > UZAITargetingComponent::ScoreEnemy(500.f, 0.f, false, false, Scoring));

	// whoever hurt it a lot draws it, even from farther away (two machete hits from 8 m against someone at 2 m)
	const float Attacker = UZAITargetingComponent::ScoreEnemy(800.f, 50.f, true, false, Scoring);
	const float Bystander = UZAITargetingComponent::ScoreEnemy(200.f, 0.f, true, false, Scoring);
	TestTrue(TEXT("Who hurt it most draws it"), Attacker > Bystander);

	// two players at about the same score: it keeps the one it fights (no flickering in co-op)
	const float CurrentTarget = UZAITargetingComponent::ScoreEnemy(400.f, 0.f, true, true, Scoring);
	const float SlightlyCloser = UZAITargetingComponent::ScoreEnemy(300.f, 0.f, true, false, Scoring);
	TestTrue(TEXT("Keeps its target over one slightly closer"), CurrentTarget > SlightlyCloser);

	// but not forever: someone much closer takes over
	const float MuchCloser = UZAITargetingComponent::ScoreEnemy(100.f, 0.f, true, false, Scoring);
	const float FarCurrent = UZAITargetingComponent::ScoreEnemy(900.f, 0.f, true, true, Scoring);
	TestTrue(TEXT("Switches to someone much closer"), MuchCloser > FarCurrent);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
