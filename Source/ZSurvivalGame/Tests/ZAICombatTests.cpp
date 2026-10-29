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

/** How an enemy picks among its attacks and how close it goes for them (ZAICombatRules::ChooseAttack, GetApproachRange) */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZAIAttackChoiceTest, "ZSurvivalGame.AI.AttackChoice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FZAIAttackChoiceTest::RunTest(const FString& Parameters)
{
	using namespace ZAICombatRules;

	const auto MakeOption = [](float MinRange, float MaxRange, float Weight, bool bReady)
	{
		FZAttackOption Option;
		Option.MinRange = MinRange;
		Option.MaxRange = MaxRange;
		Option.Weight = Weight;
		Option.bReady = bReady;
		return Option;
	};

	// a golem-like set: two close swings (one picked 3 times as often), a lunge from 1.5 to 3.3 m, a charge from 3 to 6.5 m
	TArray<FZAttackOption> Options = {
		MakeOption(0.f, 220.f, 3.f, true),
		MakeOption(0.f, 220.f, 1.f, true),
		MakeOption(150.f, 330.f, 2.f, true),
		MakeOption(300.f, 650.f, 1.f, true)
	};

	// close: only the swings fit; the roll goes through them by weight (3 + 1 = 4: the first 3/4 is the first swing)
	TestEqual(TEXT("Close, low roll: the first swing"), ChooseAttack(Options, 100.f, 0.1f), 0);
	TestEqual(TEXT("Close, roll just under 3/4: still the first swing"), ChooseAttack(Options, 100.f, 0.74f), 0);
	TestEqual(TEXT("Close, high roll: the second swing"), ChooseAttack(Options, 100.f, 0.9f), 1);
	TestEqual(TEXT("Close, a roll of 1: the last that fits"), ChooseAttack(Options, 100.f, 1.f), 1);

	// in between: the swings and the lunge overlap from 1.5 to 2.2 m (weights 3 + 1 + 2 = 6)
	TestEqual(TEXT("1.8 m, high roll: the lunge"), ChooseAttack(Options, 180.f, 0.95f), 2);
	TestEqual(TEXT("2.5 m: only the lunge"), ChooseAttack(Options, 250.f, 0.1f), 2);
	TestEqual(TEXT("5 m: only the charge"), ChooseAttack(Options, 500.f, 0.5f), 3);
	TestEqual(TEXT("8 m: nothing reaches"), ChooseAttack(Options, 800.f, 0.5f), static_cast<int32>(INDEX_NONE));

	// planning the next attack: from far away any ready attack can be planned (it walks in for it), close in only those the target isn't too close for
	// (all four by weight at 10 m: 3 + 1 + 2 + 1 = 7; a roll of 0.6 is 4.2 of them: past the first swing and the second, into the lunge)
	TestEqual(TEXT("Plan from 10 m, low roll: the first swing (walk in for it)"), PlanAttack(Options, 1000.f, 0.1f), 0);
	TestEqual(TEXT("Plan from 10 m, middle roll: the lunge"), PlanAttack(Options, 1000.f, 0.6f), 2);
	TestEqual(TEXT("Plan from 1 m, high roll: no lunge or charge that close"), PlanAttack(Options, 100.f, 0.99f), 1);
	TestEqual(TEXT("Approach for a swing from 10 m: its 2.2 m"), GetApproachRangeFor(Options[0], 1000.f), 220.f);
	TestEqual(TEXT("Approach for a swing that fits: stay"), GetApproachRangeFor(Options[0], 150.f), 150.f);

	// how close it wants to be without a plan: where an attack fits, it stays; otherwise it walks into the reach of the farthest one it can
	TestEqual(TEXT("1 m: a swing fits, stay"), GetApproachRange(Options, 100.f), 100.f);
	TestEqual(TEXT("5 m: the charge fits, stay"), GetApproachRange(Options, 500.f), 500.f);
	TestEqual(TEXT("10 m: walk into the charge's reach"), GetApproachRange(Options, 1000.f), 650.f);

	// a lunge that only reaches 2.6 m leaves a gap before the charge (from 3 m): from the gap it walks into the lunge's reach
	TArray<FZAttackOption> Gap = Options;
	Gap[2].MaxRange = 260.f;
	TestEqual(TEXT("2.9 m in the gap between lunge and charge: walk into the lunge's reach"), GetApproachRange(Gap, 290.f), 260.f);

	// attacks on their own cooldown don't count
	Options[3].bReady = false;
	TestEqual(TEXT("5 m with the charge on cooldown: nothing"), ChooseAttack(Options, 500.f, 0.5f), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("5 m with the charge on cooldown: walk into the lunge's reach"), GetApproachRange(Options, 500.f), 330.f);

	// only the charge ready and the target right in front: too close for it, it waits where it is
	TArray<FZAttackOption> OnlyCharge = Options;
	for (FZAttackOption& Option : OnlyCharge)
	{
		Option.bReady = false;
	}
	OnlyCharge[3].bReady = true;
	TestEqual(TEXT("Only the charge ready, target at 1 m: stay"), GetApproachRange(OnlyCharge, 100.f), 100.f);

	// a weight of 0: never picked
	Options[0].Weight = 0.f;
	TestEqual(TEXT("Close with the first swing at weight 0: the second"), ChooseAttack(Options, 100.f, 0.1f), 1);

	// nothing ready: it waits within the reach of all of them
	for (FZAttackOption& Option : Options)
	{
		Option.bReady = false;
	}
	TestEqual(TEXT("Nothing ready, 10 m: it still comes within reach to wait"), GetApproachRange(Options, 1000.f), 650.f);
	TestEqual(TEXT("Nothing ready: no attack"), ChooseAttack(Options, 100.f, 0.5f), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("No attacks at all: it stays"), GetApproachRange(TArray<FZAttackOption>(), 400.f), 400.f);

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
