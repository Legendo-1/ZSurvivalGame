# AI (enemies)

How enemies think: how they notice you, how they pick whom to fight in co-op, their combat states, and how to set up a new kind of enemy. Their attacks, poise and staggers are in COMBAT.md; how all of it works in multiplayer is also in MULTIPLAYER.md.

**Last updated:** 2026-10-29 (enemies don't swing at you from below a ledge: Max Attack Height)

## The parts

| Part | Class | What it does |
|---|---|---|
| The enemy | `AZAIBaseCharacter` (zombies: `AZZombieCharacter`) | Its health, team, attacks and poise, and its **Combat Settings** (senses, circling). It shows its combat state and target to every player |
| Its AI | `AZAIBaseController` | Runs the enemy, only on the host |
| Senses | **AI Perception** (on the AI) | Sight, and being hit |
| Targeting | `UZAITargetingComponent` (on the AI) | Which players it knows about, and whom it fights |
| Combat brain | `UZAICombatBrainComponent` (on the AI) | The combat states below |

## How an enemy notices you

- **Sight:** it sees you within **Sight Radius** (zombies: 30 m) if you're in its field of view (**Vision Half Angle**: 80° to each side) and no wall is in the way. Once it sees you, it keeps seeing you up to **Lose Sight Radius** (35 m), so it doesn't lose you right at the edge.
- **Up close:** it notices you within **Proximity Radius** (zombies: 5 m) even from behind (it hears or smells you).
- **Being hit:** it knows who hit it and from where, even from far away or behind.
- **Memory:** when it can't see you, it goes to where it last noticed you. After **Forget Time** (8 s) without seeing, sensing or being hit by you, it gives up and stands still (Idle).

## Whom it fights (co-op)

It weighs every player it knows about and fights the one with the best score:
- **Closer** is better (5 points less per meter).
- **Seen right now** is better than only remembered (+20).
- **Damage hurts its pride:** each point of damage a player dealt to it adds a point of threat, which fades by 5 per second. Whoever hurts it most draws it.
- **It keeps its current target** (+15), so it doesn't switch back and forth between two players at about the same score.

Example: the host stands 2 m from a zombie, the client shoots it from 8 m for 40 damage: the zombie turns on the client. If the host then hits it for 15, it stays on the client.

To change the weights: make a Blueprint of `AZAIBaseController` (see "An enemy that behaves differently" below) → **Targeting** → **Scoring**.

## The combat states

| State | What it does | When it leaves |
|---|---|---|
| **Idle** | Stands still, no target | It notices someone: Approaching |
| **Approaching** | Goes for its target at full speed (to where it last noticed it, if it can't see it). If you're up on a ledge, it looks for a way up (a ramp, stairs) | In attack range and ready: it attacks. A circling enemy that has to wait: Strafing. Target gone: Idle |
| **Strafing** | Circles its target at **Strafe Distance**, facing it, at **Strafe Speed**; turns around when something blocks it | Done waiting (**Strafe Time Min/Max** after its last attack) and ready: Approaching, to go in for the attack. The target gets away: Approaching |
| **Attacking** | Stands and swings; it's locked into the swing until it's over | Then: a circling enemy circles, a zombie goes for you again |
| **DodgeRecovering** | After a dodge, waits **Dodge Recovery Time** before doing anything | Then like after an attack |
| **Staggered** | Its poise broke: it doesn't move or turn until the stagger is over | Then like after an attack |

- **Zombies never circle:** they walk straight in, attack whenever their cooldown allows, and stand their ground in between.
- **Being staggered or dodging** interrupts every other state at once.
- **Enemies don't dodge yet:** DodgeRecovering is ready for when an enemy gets a dodge ability.

## Combat Settings for each kind of enemy

In the enemy's Blueprint (or C++ class): **Class Defaults → AI|Combat → Combat Settings**.

| Setting | What it does | Zombies |
|---|---|---|
| **Sight Radius** | How far it sees | 3000 cm |
| **Lose Sight Radius** | How far it keeps seeing someone it already sees | 3500 cm |
| **Vision Half Angle** | Half its field of view (90 = everything in front, 180 = all around) | 80° |
| **Proximity Radius** | Notices players this close without seeing them (0 = never) | 500 cm |
| **Forget Time** | Gives up on a target it hasn't sensed for this long | 8 s |
| **Max Attack Height** | It only attacks (and circles) you if your feet are at most this much higher or lower than its own. You're up on a ledge: it looks for a way up instead of swinging at the air below you | 100 cm |
| **Can Strafe** | Circles its target between attacks, like a souls knight | off |
| **Strafe Distance** | How far from its target it circles | 300 cm |
| **Strafe Time Min / Max** | How long it circles after an attack before going in again (random between the two) | 1.5 / 3.5 s |
| **Strafe Speed** | How fast it walks while circling (it chases at the movement's **Max Walk Speed**) | 150 cm/s |
| **Dodge Recovery Time** | How long after a dodge before it acts again | 0.4 s |

The attack itself (range, cooldown, damage, reach) is under **AI** and **AI|Melee** as before.

## A new kind of enemy

1. Right-click in the Content Browser → **Blueprint Class** → search **ZZombieCharacter** (or **ZAIBaseCharacter** for something completely different, e.g. a knight).
2. Change its mesh, animation Blueprint, attack montage and numbers (**Survival|Stats**, **AI**, **AI|Melee**).
3. Set its **Combat Settings**. For a knight that circles you: tick **Can Strafe**, and maybe a smaller **Vision Half Angle** and **Proximity Radius** so you can sneak up on it.
4. **A big enemy:** move its **LockOnTarget** up (COMBAT.md → Lock-on) and give its **Spacing** a larger **Extra Radius** (COMBAT.md → Spacing).
5. **Where it can be hit:** bullets and melee swings only hit the body shapes in its mesh's **Physics Asset**, not the mesh you see. Where the mesh sticks out past the shapes (a big head, helmet, shoulder armor), shots go through. To check: while playing, console `show collision` draws the shapes (`show collision` again hides them), and `Z.Weapon.DrawDebug 1` draws each shot. To fix: open the Physics Asset (the mesh → **Physics Asset** field → double-click it) → click a shape → make it bigger with **Radius** and **Length** in **Details** (or press **R** and drag in the viewport), or right-click a bone → **Add Shape** for a part without one. Save it.

**Changing settings during play** (e.g. a boss that starts circling in its second phase): the Blueprint node **Set Combat Settings** on the enemy (host only). Its AI picks them up right away.

## An enemy that behaves differently (advanced)

Each state is its own small piece, so one can be swapped for one kind of enemy without touching the others:
1. A C++ programmer makes a new state class from one of the existing ones (`UZAICombatState_Strafing` and so on, in `AI/ZAICombatStates.h`), e.g. a strafe that keeps more distance.
2. Right-click in the Content Browser → **Blueprint Class** → search **ZAIBaseController** → name it e.g. `BP_KnightAI`. Open it → select **Combat Brain** → **AI|States** → pick the new class for that state.
3. In the enemy's Blueprint: **Class Defaults → Pawn → AI Controller Class** → `BP_KnightAI`.

## Animations

The enemy's animation Blueprint can read two things (they reach every player's machine):
- **Get Combat State**: Idle, Approaching, Strafing, Attacking, DodgeRecovering or Staggered. E.g. a strafing blend space (walking sideways while facing you) while it's **Strafing**, a combat idle while it has a target.
- **Get Combat Target**: whom it fights, e.g. for its head to look at you.

The event **On Combat State Changed** in the enemy's Blueprint fires on every machine when the state changes, e.g. for a roar when it starts approaching.

## Moving around: the navmesh

Enemies find their way around walls, and up ramps and stairs to a ledge you're standing on, on the **navmesh**. The level `Lvl_ThirdPerson` has none yet, so they walk straight at you and get stuck: behind a wall, behind another player, or at the foot of a ledge, even with a ramp right next to it. To add one: **Place Actors** panel → **Volumes** → drag a **Nav Mesh Bounds Volume** into the level and scale it over the whole play area (press **P** in the viewport to see the walkable area in green; the ramps should be green too).

## Multiplayer and performance

- **All of it runs on the host.** The other players' machines get the enemy's movement, its combat state and target, and its attack and stagger animations from the host.
- **It's cheap:** an enemy thinks 5 times a second (**Think Interval** 0.2 s on the Combat Brain), and enemies are spread out so they don't all think in the same frame. Sight is handled by the engine's perception system, which spreads its checks over frames. Picking a target only goes through the few players it knows about.

## Seeing what an enemy thinks

- `ZAIStates` (console, host): every enemy's state, how long it's been in it, its target, the distance, whether it sees it, and how many players it knows about.
- `Z.AI.Debug 1`: draws the state and target over each enemy, and a line to its target (green: it sees it, orange: it remembers where it was). Type it with the **1** (`Z.AI.Debug 0` turns it off); `Z.AI.Debug` alone only writes whether it's on. Shows in the host's window only.
- **Gameplay debugger:** press the **'** key (apostrophe) while playing and look at an enemy. The text in the top left is the gameplay debugger's: its **AI** category is the engine's general AI info, not these combat states. Its **Perception** category (turn it on with the number shown next to it at the top of the screen) draws the enemy's field of view and what it sees.
- `log LogZSurvivalGame Verbose`: every state change and target change in the Output Log.

## Console commands for testing

| Command | What it does |
|---|---|
| `ZAIStates` | Writes every enemy's AI state to the Output Log (*host*) |
| `ZAIStrafe 1` | Makes every living enemy circle its target between attacks; `ZAIStrafe 0` turns it off (*host*) |
| `ZSpawnEnemyAt 900 180 1` | A zombie with its AI 9 m away, 180° to the right (behind you), looking away from you (`0` at the end: looking at you) (*host*) |
| `ZAIDodge` | Pretends every enemy dodged, to try DodgeRecovering (*host*) |
| `ZHitEnemies 40 1` | Hurts every enemy for 40 as if player 1 (the first client) hit it; `0` is the host (*host*) |
| `ZSpawnZombies 1 700` | A zombie 7 m in front of the camera (*host*) |
| `Z.AI.Debug 1` | Draws each enemy's state and target |

## Plan: what isn't built yet

- **Patrols** and **going back home** when it loses you (Idle stands still for now).
- **Choosing between several attacks** by range and cooldown, and attacks that use the hitboxes like the players' (COMBAT.md).
- **Enemies that dodge** (DodgeRecovering is ready) and block.
- **Taking turns in co-op:** only a few enemies attacking the same player at once.
- **Hearing:** gunshots drawing enemies from farther away.
- **Walking around each other** in crowds (avoidance).
