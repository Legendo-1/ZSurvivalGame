# Multiplayer & abilities

How the player's actions, weapons, enemies, health and death work in multiplayer, and the plan for what isn't built yet. Melee combat, poise and staggers: COMBAT.md. Items, the inventory, pickups and the interact key have their own guide: ITEMS.md. Crafting and tradeskills: CRAFTING.md. Levels, trait points and traits: PROGRESSION.md. The HUD and the inventory screen: UI.md. Sounds, effects and animations: AUDIO.md. Saving: SAVING.md.

**Last updated:** 2026-10-29 (lock-on; hit-stop and the hit flash; spacing between players and enemies)

## The basic idea

- **One player hosts** (listen server). The host's game is the **server** and decides what really happens. The other players are **clients**.
- **Prediction:** when you press a key, your own game does the action right away so it feels instant. The server does the same action and confirms it, or corrects you if it disagrees.
- **Replication:** the server sends values (health, weapon type, ...) to the clients, so everyone sees the same thing.

## Who owns what

| What | Lives on | Why |
|---|---|---|
| Ability system, health, stamina, stats, inventory, equipment, crafting, tradeskills, level and traits | **PlayerState** (`AZSurvivalGamePlayerState`) | Every player has one on every machine, and it survives respawns |
| Body, camera, input, the weapon in the hands | **Character** (`AZSurvivalGameCharacter`, BP_ThirdPersonCharacter) | The part you control and see |
| Sprint, aim and speed limit states | **Movement component** (`UZCharacterMovementComponent`) | Sent to the server with every move, so the server moves you at the same speed |
| Weapon stats | **Weapon Data Assets**: guns `UZRangedWeaponData` (DA_Pistol, DA_Rifle), melee weapons `UZMeleeWeaponData` (DA_Machete) with a **moveset** of attacks (`UZMeleeMoveset`) | One asset per weapon, easy to tune |
| The input buffer and the combo | **Combat component** on the character (`UZCombatComponent`) | Presses wait there on your machine; the server uses it to check combos |
| What a melee swing hits | **Melee trace component** on the character (`UZMeleeTraceComponent`) | Sweeps the blade on the machine that finds the hits: your own (players) or the server (enemies) |
| Lock-on | **Lock-on component** on the character (`UZLockOnComponent`), **lock-on points** on enemies (`UZLockOnTargetComponent`) | Picks the target, turns the camera and the character on your own machine; tells the server the target |
| How close players and enemies get | **Spacing** component on every character (`UZCharacterSpacingComponent`) | An invisible, wider capsule only the other side bumps into; plain collision, so every machine agrees |
| A weapon in the hands | **Weapon actor** (`AZWeapon`) | Spawned by the server when a weapon item is equipped, attached to the hand on every machine; the server keeps its ammo |
| A crafting station | **Station actor** (`AZCraftingStation`, BP_Workbench) | Placed in a level; recipes that need it only work nearby (the server checks the distance) |
| Enemies' health and attacks | **The enemy itself** (`AZAICharacter`) | Enemies have no PlayerState, so their ability system is on the character |
| Enemy decisions | **AI controller** (`AZAIController`) | Only exists on the server |
| What's been done in the world (bosses, shortcuts) | **GameState** (`AZSurvivalGameState`) | Everyone sees it; it's saved with the host's game |
| Bringing your character in and saving it | **Player Save** component on the PlayerController (`UZPlayerSaveComponent`), and `UZSaveSubsystem` for the files on your PC | The PlayerController only exists on the server and your own machine, the two that need it |

## What happens when you press a key

Sprint as an example:

1. **Sprint key** → `DoSprintStart` → `PressAbilityInput(Sprint)`.
2. The ability system finds the ability whose **InputID** is Sprint and checks whether it may start: not aiming, not firing, no speed limit, more than 10 stamina.
3. **Your game runs it right away:** the Sprint ability tells the movement component "wants to sprint", so you speed up immediately.
4. **The server runs it too.** Every move you send also says "sprinting", so the server moves you at the same speed and doesn't pull you back.
5. **The server drains stamina** and sends the new value to you, so your HUD bar goes down.
6. **Releasing the key** ends the ability on both. Running out of stamina: the server ends it and tells your game.

```
Key → Character (DoSprintStart) → Ability system (on PlayerState) → Sprint ability → Movement component → speed
                                         ↑                                                   ↓
                                   server confirms                         flags sent to the server with every move
```

## Shooting

1. **Fire button** → `DoFireStart` → the Fire ability. Automatic weapons fire again every frame the button is held, as fast as the weapon's fire rate allows. An empty magazine reloads instead, if there's ammo in the inventory.
2. **Your game aims** from the camera (the crosshair), adds the weapon's spread (smaller while aiming) and traces each bullet.
3. **Your game shows the shot right away:** camera kick (recoil), and the shot and impact effects.
4. **The hits are sent to the server.** The server checks they're possible (in range), uses one round, and deals the damage.
5. **The server sends the effects to the other players**, and the new ammo count to you.

The host does all of this on its own machine.

Shots go through the capsule and hit the character's mesh, so a hit knows the bone. Hits on a bone with "head" in its name do **2× damage** (Headshot Multiplier in the weapon data).

## Melee attacks and locked-in actions

COMBAT.md explains the rules for players. In multiplayer:

1. **Left mouse** with a melee weapon → `DoFireStart` → the combat component. If you're busy (mid-swing, mid-dodge, a shot's recovery, staggered), the press waits in the input buffer; otherwise it starts the attack now.
2. **The attack's number in the combo** (first, second, ...) is picked by your machine (from the Combo Window it sees) and sent with the attack as a gameplay event (`Event.Attack.Light` / `Heavy`).
3. **Your game runs the attack right away** (predicted, like the dodge): the animation, its root motion, and the hitboxes. **The server runs it too**, a moment later. The server only accepts a first attack or the follow-up of the attack before (`Combo Timing Tolerance`), so nobody can skip to the strongest one.
4. **Your machine finds the hits** (the blade during the Weapon Hitbox notify) and sends them to the server, which checks them: close enough (the weapon's Max Reach plus **Melee Reach Tolerance**) and not hit more often than the attack has hitboxes. Then it deals the damage and poise damage.
5. **The server decides staggers** (poise is only on the server): it starts the stagger, cuts short whatever you were doing, and tells your machine.
6. **The server is lenient about timing:** it trusts your machine about when a window was open (your animation runs a moment ahead of the server's). It only refuses actions while you're staggered or dead.
7. **Hit-stop** (your swing almost stops for a moment when it hits) is a state of your moves, like sprinting: your machine slows the attack, and the server slows exactly the same moves when they arrive. So the attack's movement and its windows stay in step on both.
8. **The hit flash** comes with the target's Hit Cue, which the server sends to everyone with every hit: every machine flashes the character itself.

Enemies run everything on the server.

## Lock-on

COMBAT.md → Lock-on explains it for players. In multiplayer:

1. **Your machine does all of it:** it picks the target, moves the camera and turns your character toward the target by pointing the control rotation at it. The control rotation goes to the server with every move anyway (it's how the server knows where you look), so the server turns you the same way.
2. **The movement component sends one more state with every move:** "strafing" (facing the target instead of turning toward where you walk). The four compressed move flags were full (sprint, aim, speed limit), so this rides in the movement component's own move data, which has room for more states.
3. **The server is told the target** (one message when it changes). It checks it (alive, an enemy, roughly in range) and passes it on to the other players. Nothing decides gameplay with it: it's there for animations and later features.
4. **Finding a target is cheap:** every lock-on point registers itself in a list (`UZLockOnSubsystem`), so the search never looks through the whole level. Walls are checked with a line trace only for the best few, and while locked on, about ten times a second.

## The abilities

| Ability | What it does | Blocked by | Cost / notes |
|---|---|---|---|
| **Sprint** (`UZGameplayAbility_Sprint`) | Sprint speed while the key is held | Aiming, firing, speed limit, 10 stamina or less | Stamina per second, drained by the server |
| **Aim** (`UZGameplayAbility_Aim`) | Aim walk speed, faces the camera, camera zoom, less spread, while the right mouse button is held | | Cancels sprint |
| **Dodge** (`UZGameplayAbility_Dodge`) | Quick dash in the movement direction (forward when standing still); invulnerable for the first 0.2 s | Less stamina than the cost; busy (except in a Dodge Window) | 25 stamina; cancels a reload |
| **Fire** (`UZGameplayAbility_Fire`) | One shot of the gun in the hands (left mouse / right trigger); busy for the Shot Recovery Time after it | Reloading, dodging, empty magazine, fire rate, busy | 1 round; cancels sprint |
| **Reload** (`UZGameplayAbility_Reload`) | Loads the magazine from the inventory's ammo after the weapon's reload time (R / gamepad left face button) | Full magazine, no ammo in the inventory, busy | Jog speed at most meanwhile |
| **Light / Heavy attack** (`UZGameplayAbility_LightAttack`, `_HeavyAttack`) | One attack of the melee weapon's light or heavy combo: its animation, and damage where its hitboxes touch | No melee weapon, in the air, no stamina left, busy (except in a Combo Window) | The attack's stamina; no walking meanwhile; cancels sprint and aim |
| **Stagger** (`UZGameplayAbility_Stagger`) | Players and enemies: started by the server when poise breaks; the stagger animation, no actions meanwhile | | Cancels every other ability |
| **Melee attack** (`UZGameplayAbility_MeleeAttack`) | Enemies only: attack animation, then damage and poise damage to everyone hostile in front | Busy, staggered | Server only |

Every ability is blocked while dead or staggered.

**Tags** are labels the abilities use to find and block each other:
- `Ability.Sprint`, `Ability.Aim`, `Ability.Dodge`, `Ability.Fire`, `Ability.Reload`, `Ability.Attack.Light`, `Ability.Attack.Heavy`, `Ability.Stagger`, `Ability.MeleeAttack`: on each ability, e.g. Aim cancels anything tagged `Ability.Sprint`.
- `Ability.Type.Action`: on the actions (attacks, dodge, fire). An action started in another one's window cuts that one short.
- `State.Sprinting`, `State.Aiming`, `State.Dodging`, `State.Firing`, `State.Reloading`, `State.Attacking`: on the character while that ability runs, e.g. Fire is blocked by `State.Reloading`.
- `State.Busy`: while an action runs. Actions (and the reload) wait until it's gone, unless the animation is in their window (`Window.Combo`, `Window.Dodge`).
- `State.Staggered`: during a stagger. It blocks every ability. `State.Invulnerable`: the dodge's first moments; damage passes through.
- `State.Dead`: on a character when its health reaches 0. It blocks every ability.

## Weapons

- **Weapon Data Assets** are in `Content/Weapons`: **DA_Pistol** (semi-automatic, 12 rounds) and **DA_Rifle** (automatic, 30 rounds). Open one to change its damage, fire rate, spread, range, recoil, magazine size, reload time and **Ammo Item** (which inventory item reloading uses).
- **Weapons are items** (`ID_Pistol`, `ID_Rifle`): the player carries them in the inventory and equips one per weapon slot (short gun, long gun, melee; ITEMS.md → Equipment). The server spawns the weapon in the hands when one is taken out.
- **The weapon keys** (1 = short gun, 2 = long gun, 3 = melee) call `SelectWeaponSlot`: your machine switches the animation set at once, and the server takes the weapon in that slot out (or puts it away, if it's already in the hands). A key for an empty slot does nothing.
- **Set Weapon Type** (Blueprints and tests) still works: it takes out a weapon of that animation type, an equipped one first (an unequipped one is equipped first).
- **The weapons are invisible for now:** there are no weapon meshes in the project yet. Set a Skeletal Mesh or Static Mesh in the data asset to show one. **Attach Socket** says where it goes on the character (default `weapon_r`, or `hand_r` if the mesh doesn't have it), **Attach Offset** lines it up, and **Muzzle Socket** marks where the shots start.
- **Fire and reload animations** are optional: set **Fire Montage** / **Reload Montage** in the data asset.
- **Upgrades and mods** (CRAFTING.md) change a weapon item's stats. The weapon in the hands works its stats out from its data, its item (upgrade level and mods) and the holder's traits. It sends its item to its owner only (`COND_OwnerOnly`), so the owner's machine predicts spread, recoil, the magazine and reload time with the same numbers as the server.
- **When a magazine gets smaller** (e.g. an Extended Magazine comes off while the weapon is in the hands), the server moves the extra rounds back into the reserve ammo.

- **Melee weapons** (`ID_Machete`, DA_Machete) are weapons in the hands like the guns, without ammo. Their attacks are in their moveset (COMBAT.md).

**A new gun:** right-click in `Content/Weapons` → **Miscellaneous → Data Asset** → **ZRangedWeaponData** and fill it in. Then make an item with a **Weapon** fragment pointing to it (ITEMS.md → How to add things). Its **Weapon Type** decides which animation set it uses, its **Equipment Slot** which slot (and key) it goes in. **A new melee weapon:** COMBAT.md → Making a melee weapon.

## Enemies

- **`AZAICharacter`** is the base class for every AI character (zombies, other monsters, human NPCs). Each one has its own health, team, AI ranges and melee numbers (Class Defaults: **Survival|Stats**, **AI**, **AI|Melee**).
- **`AZZombieCharacter`** is the first enemy type: the Quinn mannequin with the combat template's animations and punch, until zombie models exist. 60 health, slower than a jogging player, 15 damage and 15 poise damage per hit, 30 poise (two machete hits stagger it).
- **Enemies stagger** when their poise breaks: their attack is cut short and the AI waits it out. **Max Poise** is under **Survival|Stats**, the **Stagger Montage** under **AI|Stagger**, the poise damage of its hits under **AI|Melee**.
- **The AI** (`AZAIController`, server only) goes for the closest living player within its aggro range, walks there and attacks when close. With a **Nav Mesh** in the level it walks around obstacles; without one it walks straight at you. It moves by accelerating, like a player's input (**Use Acceleration For Paths**, set in `AZAICharacter`), so the animation Blueprint plays its walk instead of sliding it along in the standing pose.
- **`AZEnemySpawner`**: place it in a level to keep spawning enemies around it (Max Alive, Spawn Interval, Spawn Radius, Total To Spawn, Enemy Class).

**A new enemy type:** right-click in the Content Browser → **Blueprint Class** → search **ZZombieCharacter** (or **ZAICharacter** for something completely different) → change its mesh, animation Blueprint, attack montage and numbers. A special attack (e.g. a spitter's acid) is a new ability added to **Default Abilities**.

**Teams** (`EZTeam`): Players, Monsters, Humans. Characters on the same team can't damage each other (no friendly fire). Friendly human NPCs use the Players team.

## Damage, health and death

- **All damage goes through `UZCombatLibrary::ApplyDamage`**, which Blueprints can use too (e.g. traps). It checks the teams, lowers the target's health through a GameplayEffect on **IncomingDamage**, and plays the hit effect.
- **Health and stamina** are attributes in `UZAttributeSet`: Health, MaxHealth, Stamina, MaxStamina. **Only the server changes them.** Health is sent to everyone, stamina only to its own player (for the HUD).
- **Armor** (an attribute too, mostly from worn armor) lowers all damage on the server: damage × 100 / (100 + Armor). Enemies have 0, so later armored bosses only need a value.
- **Poise** (Poise, MaxPoise) is worn down by hits that carry poise damage (`ApplyDamage`'s Poise Damage); at 0 the character staggers (COMBAT.md). A character in its dodge's invulnerable moment (`State.Invulnerable`) takes no damage and no poise damage.
- **A player at 0 health** falls as a ragdoll and puts the weapon away; the items stay in the inventory. After **5 seconds** they respawn at a Player Start with full health and stamina. The body disappears 10 seconds later.
- **An enemy at 0 health** falls as a ragdoll, stops its AI, drops its loot (its **Loot Table**) and disappears after 10 seconds. Every player gets its **Experience Reward** and **Trait Point Reward** (for bosses), see PROGRESSION.md. Its Blueprint can react with the **On Died** event, which also says who killed it (e.g. to count kills later).
- **Player Starts:** each level needs at least one; one per player is best. If they're all crowded, the character is moved to a free spot nearby instead of failing to spawn.

## Effects and sounds (GameplayCues)

The code sends an event (a GameplayCue tag) to every player for shots, impacts, hits, deaths, reloads, taking out weapons, level-ups, pickups, used items and crafting. Nothing is shown or heard until a **GameplayCue Notify** asset with the same tag exists in `Content/GameplayCues`. AUDIO.md lists every event and explains how to make the sounds, effects and animations.

Until then, the shooter sees **debug lines**: white = shot, yellow dot = hit, red dot = damage dealt (red only on the host, since only the server deals damage). Turn them off in the console with `Z.Weapon.DrawDebug 0`.

## Joining with your character (saving)

Remnant style: each player's character is saved on their own PC, and the world on the host's (SAVING.md has the details).
1. **Joining:** your machine sends the character from your save to the host. The host checks it against the game's rules, puts it on your PlayerState (inventory, level, traits, tradeskills, recipes), and only then spawns your character, with the weapon you had in your hands. The host's own character comes from the host's PC the same way, without the network.
2. **Playing:** a few seconds after something changes (10 by default), the host builds your character from the game and sends it back; your machine writes it into your save. The host writes its own save, which also holds the world.
3. **Leaving:** when you leave or the host ends the game, the host can't send anything any more, so your machine writes your character one last time from its copy of what the host sent (only if the host loaded your character from that save).
4. **The world** (what's been done in it, e.g. a boss beaten) is the host's. It's on the GameState, so everyone sees it, and only the host's save keeps it.

The host checks every character when it joins, so a save can't bring anything past the game's rules into a game. Each saved character has an ID, and a save only goes into the slot it came from.

## HUD and inventory screen

The HUD (`WBP_HUD`, the widget names it needs) and the inventory screen are explained in UI.md.

## What the other players see

| What | How it reaches them |
|---|---|
| Movement and rotation | The engine sends it |
| Looking up/down (aim offset) | The engine sends the aim pitch |
| Aiming (aim walk/idle) | `bIsAiming`, sent by the server |
| Weapon type and the weapon in the hands | `CurrentWeaponType` and `EquippedWeapon`, set by the server (weapon keys, equipping) |
| Armor and jewelry | Nothing yet: only the owner gets what's equipped (armor isn't visible on the character yet) |
| Shots, impacts, hits, deaths, reloads, taking out weapons, crafting, ... | GameplayCues, sent by the server (AUDIO.md) |
| Attack, fire, reload, dodge and stagger animations | The ability system plays them for everyone |
| Who you're locked on to | The lock-on component's target, sent by the server (your character's facing already shows it) |
| A character flashing when hit | It comes with the Hit Cue (sent by the server) |
| The animation for taking out a weapon | It comes with the equip GameplayCue |
| Footsteps | Nothing is sent: every machine plays them from the animations and movement it already has (AUDIO.md → Footsteps) |
| Death (ragdoll) | `bIsDead`, sent by the server |
| Enemies | They replicate like any character; only their decisions stay on the server |

## Where to change values

| Value | Where |
|---|---|
| Jog, sprint, aim walk and walk speeds, max sprint angle | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Movement** |
| Stamina drain and regen per second | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Stats** |
| Starting items (weapons, ammo, ...) | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Inventory** |
| Equipment slots (how many rings, ...) and how strong armor is | **Project Settings → Game → Equipment** |
| Which key takes out which weapon slot | BP_ThirdPersonCharacter → Class Defaults → **Input → Weapon Slot Inputs** (the keys themselves: `IMC_Default`) |
| What armor and jewelry give | The item's **Wearable** fragment (`Content/Items/Armor`, `Content/Items/Jewelry`) |
| What enemies drop | The loot table in `Content/Items/LootTables` (set on the enemy: **AI\|Loot**) |
| Experience and trait points for a kill | The enemy's Class Defaults → **AI\|Rewards** |
| Levels, trait points, tradeskill levels | **Project Settings → Game → Progression** |
| Traits | `Content/Progression/Traits` (PROGRESSION.md) |
| Weapon damage, fire rate, spread, ammo, reload time, recoil | The weapon's Data Asset in `Content/Weapons` |
| Mod slots, max upgrade level, damage per upgrade | The weapon's Data Asset → **Upgrades** |
| What a mod does | The mod item's **Weapon Mod** fragment (`Content/Items/Mods`) |
| Max health and max stamina of players (100) | `UZAttributeSet` constructor in C++ |
| Enemy health, team, speed, ranges, melee damage | `AZZombieCharacter` in C++, or a Blueprint child's Class Defaults |
| Respawn delay (5 s) and dead body time (10 s) | BP_ThirdPersonGameMode → Class Defaults → **Respawn** |
| Dodge cost, speed, duration and invulnerable time, minimum stamina to sprint | The ability's C++ constructor (or a Blueprint child of the ability) |
| Melee damage, poise damage, attack speed, reach | The melee weapon's Data Asset; each attack's multipliers and stamina in its moveset (COMBAT.md) |
| Input buffer time, poise recovery, stagger length, the host's slack for melee hits | **Project Settings → Game → Combat** |
| How long stamina waits before it comes back | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Stats → Stamina Regen Delay** |

## Testing

**2 players:** in the editor, click the three dots ⋮ next to Play, then:
- **Number of Players:** 2
- **Net Mode:** Play As Listen Server

The **Server** window is the host. The **Client** window is the second player.

**Console commands** (open the console with the key under Esc while playing; in multiplayer, type them in the Server window):

| Command | What it does |
|---|---|
| `ZSpawnZombies 3` | Spawns 3 zombies 8 m in front of the camera; `ZSpawnZombies 1 300` spawns one 3 m away |
| `ZKillEnemies` | Kills every enemy |
| `ZHeal` | Fills your health and stamina |
| `ZHurt 50` | Takes 50 health (`ZHurt 0` shows your health) |
| `ZSpawnDummy 120` | A zombie without AI in front of you, to practice melee on (`ZSpawnDummy 120 all`: one in front of every player) |
| `ZStagger` | Staggers you (`ZStagger players`, `ZStagger enemies`) |
| `ZCombat` | Your combat state: attack, input buffer, busy or staggered, open windows, lock-on, how far the nearest enemy stands, poise and stamina |
| `ZLockOn` | Locks on to the enemy nearest the middle of the screen, or lets go (like the lock-on key) |
| `ZTurn -20` | Turns your camera 20° to the left, like the mouse (to test switching and letting go) |
| `ZSpawnDummyAt 220 -55` | A dummy 2.2 m away, 55° to the left (`all` at the end: for every player) |
| `Z.Melee.DrawDebug 1` | Draws melee hitboxes and hits |
| `ZFloorSurface Metal` | The floor under you sounds like metal until Play stops; `ZFloorSurface` says what it is |
| `Z.Footsteps.Debug 1` / `0` | Shows / hides every footstep (Output Log and in the world) |
| `Z.Weapon.DrawDebug 0` / `1` | Hides / shows the shot debug lines |
| `log LogZSurvivalGame Verbose` | Writes every shot, reload, pickup and inventory change to the Output Log |

The item and equipment commands (`ZGiveItem`, `ZShowInventory`, `ZDropItem`, `ZUseItem`, `ZSpawnPickup`, `ZEquip`, `ZEquipment`) are in ITEMS.md, the crafting commands (`ZCombine`, `ZSkills`, ...) in CRAFTING.md, the progression commands (`ZTraits`, `ZRaiseTrait`, ...) in PROGRESSION.md, the save commands (`ZSave`, `ZSaveSlots`, `ZNewCharacter`, ...) in SAVING.md.

**Saves while testing:** each Play continues your saved character, and the Client window keeps its own (in `Saved/SaveGames/PIE1`). For a new character every Play: Project Settings → Game → Saving → untick **Use Saves In Editor**.

**Automated test runs:** `-ExecCmds` runs every command at once at startup, before a client has joined. `ZDelay 5 <command>` runs a command later, and `ZOnClients <command>` (host) runs it on every client, e.g. `ZDelay 12 ZOnClients ZSkills`.

## Plan: what isn't built yet

Not decided in detail. This is the direction, so new systems fit what's already there.

- **Weapon looks:** weapon meshes, and the sounds, effects and animations themselves. The events that play them are ready (AUDIO.md).
- **Hit reactions:** small flinches on every hit (staggers when poise breaks are built: COMBAT.md).
- **Downed and revive:** players go down first and teammates can revive them, before dying for real.
- **Smarter AI:** AI perception (sight and hearing) instead of distance, monsters and human NPCs fighting each other, patrols and ranged attacks, possibly with a StateTree.
- **Damage types** (acid, electric), e.g. from weapon mods: GameplayEffects with tags (Blueprint children of `UZGameplayEffect_Damage`). Effects everyone sees are GameplayCues.
- **Armor weight tiers:** e.g. slower dodges for heavy armor: a weight stat on the Wearable fragment, added up by the equipment component like the other stats.
- **Armor you can see:** a list of what's worn sent to everyone (item names only, not the items), and meshes on the character.
- **Harvesting and storage:** built on the item system (plans in ITEMS.md and CRAFTING.md).
- **Spawn protection:** a few seconds without damage after respawning, so enemies at a Player Start can't kill a player again right away.
- **Hosting and joining over the internet:** right now only in the editor. Real hosting needs an online service (Steam or Epic Online Services).
