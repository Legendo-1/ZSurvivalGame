# AI (enemies)

How enemies think: how they notice you, how they pick whom to fight in co-op, their combat states, and how to set up a new kind of enemy. Their attacks, poise and staggers are in COMBAT.md; how all of it works in multiplayer is also in MULTIPLAYER.md.

**Last updated:** 2026-10-29 (the Stone Golem's four attacks with your numbers; Min Range left at 0)

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
| **Approaching** | Picks the attack it goes for next, and walks at full speed until you're in that attack's range (to where it last noticed you, if it can't see you). If you're up on a ledge, it looks for a way up (a ramp, stairs) | In range and ready: it attacks. A circling enemy that has to wait: Strafing. Target gone: Idle |
| **Strafing** | Circles its target at **Strafe Distance**, facing it, at **Strafe Speed**; turns around when something blocks it | Done waiting (**Strafe Time Min/Max** after its last attack) and ready: Approaching, to go in for the attack. The target gets away: Approaching |
| **Attacking** | Makes the attack; it's locked into it until its animation is over | Then: a circling enemy circles, a zombie goes for you again |
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

## Attacks

Each kind of enemy has a list of attacks: **Class Defaults → AI|Attacks → Attacks**. How it uses them:
1. **It plans one:** among its attacks that are ready (off their own **Cooldown**) and that it can reach from where it stands (you're not closer than the attack's **Min Range**), it picks one at random, more often the ones with a higher **Weight**.
2. **It walks in** until you're within that attack's **Max Range**.
3. **It attacks.** Then it plans the next one. Between two attacks there's always at least **Attack Cooldown** (under **AI**).

| Setting of an attack | What it does |
|---|---|
| **Montage** | Its animation |
| **Min Range / Max Range** | It starts this attack when you're this far away (middle to middle, along the ground). Leave Min Range at 0 unless an attack must never be used up close: a higher Min Range only makes the attack come up less often |
| **Damage**, **Poise Damage** | Of each hit |
| **Cooldown** | Seconds before it uses this attack again (its other attacks can come sooner) |
| **Weight** | How often it's picked: 2 = twice as often as an attack with 1; 0 = never |
| **Fallback Hit** (Hit Delay, Reach, Radius) | Only for a montage without Weapon Hitbox notifies: one hit that many seconds in, a ball pushed straight forward from its middle |

**Where an attack hits:** like a player's attack, through the **Weapon Hitbox** notifies in its montage (COMBAT.md → Hitboxes). While one is active, the enemy's weapon is swept along the swing, and whoever it touches is hit once per notify. The weapon is the first of its **Attached Meshes** with **Is Weapon** ticked: between the mesh's sockets `BladeStart` and `BladeEnd` if it has them, otherwise the whole mesh along its longest side, as thick as the mesh (**Blade Radius** changes that). A hit only counts where the weapon really is: step back, or dodge through it (the dodge's invulnerable moment works against enemies too). To see the hitboxes while playing: `Z.Melee.DrawDebug 1` (green trails, in the host's window).

An attack without Weapon Hitbox notifies (the zombie's punch) hits with its **Fallback Hit** instead.

## A new kind of enemy

1. Right-click in the Content Browser → **Blueprint Class** → search **ZZombieCharacter** (or **ZAIBaseCharacter** for something completely different, e.g. a knight).
2. Change its mesh, animation Blueprint and numbers (**Survival|Stats**, **AI**), and give it its attacks (**AI|Attacks**, see Attacks above).
   - **Things it carries** (a weapon, a shield): **Looks → Attached Meshes** → **+** → pick the **Mesh** and type the **Socket** of its body it hangs from, e.g. `weapon_rSocket` (the body mesh's skeleton lists its sockets). Tick **Is Weapon** on the one its attacks hit with. They show up when you play, not in the Blueprint's viewport. Bullets and swings pass through them.
   - **Its attack montages:** add a **Weapon Hitbox** notify to each, over the frames where the weapon swings through (COMBAT.md → Hitboxes), so its hits follow the weapon.
3. Set its **Combat Settings**. For a knight that circles you: tick **Can Strafe**, and maybe a smaller **Vision Half Angle** and **Proximity Radius** so you can sneak up on it.
4. **A big enemy:** move its **LockOnTarget** up (COMBAT.md → Lock-on) and give its **Spacing** a larger **Extra Radius** (COMBAT.md → Spacing).
5. **Where it can be hit:** bullets and melee swings only hit the body shapes in its mesh's **Physics Asset**, not the mesh you see. Where the mesh sticks out past the shapes (a big head, helmet, shoulder armor), shots go through. To check: while playing, console `show collision` draws the shapes (`show collision` again hides them), and `Z.Weapon.DrawDebug 1` draws each shot. To fix: open the Physics Asset (the mesh → **Physics Asset** field → double-click it) → click a shape → make it bigger with **Radius** and **Length** in **Details** (or press **R** and drag in the viewport), or right-click a bone → **Add Shape** for a part without one. Save it.

**Changing settings during play** (e.g. a boss that starts circling in its second phase): the Blueprint node **Set Combat Settings** on the enemy (host only). Its AI picks them up right away.

**To try it:** drag its Blueprint into the level, or type `ZSpawnEnemyAt 600 0 0 <its Blueprint's name>` while playing.

## The Stone Golem

The first enemy made this way, from the Stone Golem pack: `BP_Enemy_StoneGolem` in `Content/Enemies/StoneGolem`. A slow, tough knight of stone with a hammer and a shield, that circles you between its swings. To meet it: `ZSpawnEnemyAt 600 0 0 BP_Enemy_StoneGolem`.

| | Golem | Zombie |
|---|---|---|
| Health | 500 (about 28 rifle shots) | 60 |
| Poise | 100 (about 6 light machete hits, or 2 heavy ones, stagger it) | 30 |
| Attacks | five hammer attacks (below) | a punch, 15 damage |
| Size | 1.1 × the pack's golem, about 2.1 m tall | a mannequin |
| Speed | walks at 240, turns slowly (200° a second): you can get behind it | 280 |
| Circling | at 3.2 m, 1.5 to 3 s between attacks | never |
| Reward | 50 experience | 10 |

Its attacks (the hammer hits; at least 2 s from one attack to the next):

| Attack | Range | Damage / poise damage | Cooldown | Weight | Notes |
|---|---|---|---|---|---|
| 1, sideways swing | 0–2.0 m | 35 / 50 | – | 1 | Passes at your head |
| 2, double swing | 0–2.0 m | 30 / 40 | – | 1 | Hits the most often |
| 3, lunge with a spin | 0–2.5 m | 40 / 60 | 6 s | 1 | Moves it 1.2 m; the spin passes at head height, so it often misses |
| 4, charge | 0–3.0 m | 20 / 25 each | 10 s | 1 | Moves it 4.3 m with four swings |

All four attacks have Min Range 0, so it can use any of them up close, each about as often (weight 1). Every hit but the charge's staggers you (your poise can't take 35 at once).

The quick swing (`AM_StoneGolem_Attack3`: 0–1.5 m, 25 / 35, weight 1) was taken out of its list: it arcs up and behind the golem and rarely connected. Its montage is still in the folder; to bring it back, add an entry to **Attacks** with that montage.

**Why 1.1 times and not bigger:** the pack's swings are made for its own size. At 1.25 times most of them passed over your head (in a test 3 of 11 swings hit, against 6 of 12 at 1.1).

What's in its folder:
- **AM_StoneGolem_Attack1** to **Attack5**: its attacks as montages (Attack3, the quick swing, isn't used now), each with **Weapon Hitbox** notifies (on a track named Hitbox) over the moments the hammer swings through the front, measured from the animations. Attack4 and Attack5 move the golem (root motion).
- **AM_StoneGolem_Stagger**: its stagger.
- **PHYS_StoneGolem_Enemy**: its hit shapes, a copy of the pack's with every shape grown to cover the part of the mesh it belongs to (so shots don't pass through its armor). The Blueprint uses it through the mesh's **Physics Asset Override**; the pack's own physics asset is unchanged.

**Still to do (your part, EDITOR_TASKS task 11):** its animation Blueprint (the pack's demo one, `ABP_StoneGolem`) only walks forward, so the golem slides sideways when it circles. It has no loot yet (**AI|Loot → Loot Table**).

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

- `ZAIStates` (console, host): every enemy's state, how long it's been in it, its target, the distance, whether it sees it, how many players it knows about, how much higher its target stands, how close it wants to get for its next attack (attack reach), and whether it may attack.
- `log LogZSurvivalGame Verbose` also writes which attack it picks and from how far ("attacks: attack 2 ... from 180 cm"), and each hit with the bone it hit.
- `Z.AI.Debug 1`: draws the state and target over each enemy, and a line to its target (green: it sees it, orange: it remembers where it was). Type it with the **1** (`Z.AI.Debug 0` turns it off); `Z.AI.Debug` alone only writes whether it's on. Shows in the host's window only.
- **Gameplay debugger:** press the **'** key (apostrophe) while playing and look at an enemy. The text in the top left is the gameplay debugger's: its **AI** category is the engine's general AI info, not these combat states. Its **Perception** category (turn it on with the number shown next to it at the top of the screen) draws the enemy's field of view and what it sees.
- `log LogZSurvivalGame Verbose`: every state change and target change in the Output Log.

## Console commands for testing

| Command | What it does |
|---|---|
| `ZAIStates` | Writes every enemy's AI state to the Output Log (*host*) |
| `ZAIStrafe 1` | Makes every living enemy circle its target between attacks; `ZAIStrafe 0` turns it off (*host*) |
| `ZSpawnEnemyAt 900 180 1` | A zombie with its AI 9 m away, 180° to the right (behind you), looking away from you (`0` at the end: looking at you) (*host*) |
| `ZSpawnEnemyAt 600 0 0 BP_Enemy_StoneGolem` | The same with another kind of enemy: its Blueprint's name at the end (*host*) |
| `Z.Melee.DrawDebug 1` | Draws the hitboxes of every melee attack, players' and enemies' (green), and their hits (red) |
| `ZAIDodge` | Pretends every enemy dodged, to try DodgeRecovering (*host*) |
| `ZHitEnemies 40 1` | Hurts every enemy for 40 as if player 1 (the first client) hit it; `0` is the host (*host*) |
| `ZSpawnZombies 1 700` | A zombie 7 m in front of the camera (*host*) |
| `Z.AI.Debug 1` | Draws each enemy's state and target |

## Plan: what isn't built yet

- **Patrols** and **going back home** when it loses you (Idle stands still for now).
- **Enemies that dodge** (DodgeRecovering is ready) and block.
- **Taking turns in co-op:** only a few enemies attacking the same player at once.
- **Hearing:** gunshots drawing enemies from farther away.
- **Walking around each other** in crowds (avoidance).
