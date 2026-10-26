# Multiplayer & abilities

How the player's actions, weapons, enemies, health and death work in multiplayer, and the plan for what isn't built yet. Items, the inventory, pickups and the interact key have their own guide: ITEMS.md.

**Last updated:** 2026-10-26 (weapons now come from the inventory, reloading uses reserve ammo, crowded spawns fixed)

## The basic idea

- **One player hosts** (listen server). The host's game is the **server** and decides what really happens. The other players are **clients**.
- **Prediction:** when you press a key, your own game does the action right away so it feels instant. The server does the same action and confirms it, or corrects you if it disagrees.
- **Replication:** the server sends values (health, weapon type, ...) to the clients, so everyone sees the same thing.

## Who owns what

| What | Lives on | Why |
|---|---|---|
| Ability system, health, stamina, inventory | **PlayerState** (`AZSurvivalGamePlayerState`) | Every player has one on every machine, and it survives respawns |
| Body, camera, input, the weapon in the hands | **Character** (`AZSurvivalGameCharacter`, BP_ThirdPersonCharacter) | The part you control and see |
| Sprint, aim and speed limit states | **Movement component** (`UZCharacterMovementComponent`) | Sent to the server with every move, so the server moves you at the same speed |
| Weapon stats | **Weapon Data Assets** (`UZWeaponData`: DA_Pistol, DA_Rifle) | One asset per weapon, easy to tune |
| A weapon in the hands | **Weapon actor** (`AZWeapon`) | Spawned by the server when a weapon item is equipped, attached to the hand on every machine; the server keeps its ammo |
| Enemies' health and attacks | **The enemy itself** (`AZAICharacter`) | Enemies have no PlayerState, so their ability system is on the character |
| Enemy decisions | **AI controller** (`AZAIController`) | Only exists on the server |

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

## The abilities

| Ability | What it does | Blocked by | Cost / notes |
|---|---|---|---|
| **Sprint** (`UZGameplayAbility_Sprint`) | Sprint speed while the key is held | Aiming, firing, speed limit, 10 stamina or less | Stamina per second, drained by the server |
| **Aim** (`UZGameplayAbility_Aim`) | Aim walk speed, faces the camera, camera zoom, less spread, while the right mouse button is held | | Cancels sprint |
| **Dodge** (`UZGameplayAbility_Dodge`) | Quick dash in the movement direction (forward when standing still) | Less stamina than the cost | 25 stamina; cancels a reload |
| **Fire** (`UZGameplayAbility_Fire`) | One shot of the weapon in the hands (left mouse / right trigger) | Reloading, dodging, empty magazine, fire rate | 1 round; cancels sprint |
| **Reload** (`UZGameplayAbility_Reload`) | Loads the magazine from the inventory's ammo after the weapon's reload time (R / gamepad left face button) | Full magazine, no ammo in the inventory | Jog speed at most meanwhile |
| **Melee attack** (`UZGameplayAbility_MeleeAttack`) | Enemies only: attack animation, then damage to everyone hostile in front | | Server only |

Every ability is blocked while dead.

**Tags** are labels the abilities use to find and block each other:
- `Ability.Sprint`, `Ability.Aim`, `Ability.Dodge`, `Ability.Fire`, `Ability.Reload`, `Ability.MeleeAttack`: on each ability, e.g. Aim cancels anything tagged `Ability.Sprint`.
- `State.Sprinting`, `State.Aiming`, `State.Dodging`, `State.Firing`, `State.Reloading`, `State.Attacking`: on the character while that ability runs, e.g. Fire is blocked by `State.Reloading`.
- `State.Dead`: on a character when its health reaches 0. It blocks every ability.

## Weapons

- **Weapon Data Assets** are in `Content/Weapons`: **DA_Pistol** (semi-automatic, 12 rounds) and **DA_Rifle** (automatic, 30 rounds). Open one to change its damage, fire rate, spread, range, recoil, magazine size, reload time and **Ammo Item** (which inventory item reloading uses).
- **Weapons are items** (`ID_Pistol`, `ID_Rifle`): the player carries them in the inventory, and the server spawns the weapon in the hands when one is equipped. See ITEMS.md.
- **Your weapon keys (1/2/3)** call `Set Weapon Type`. It takes out the inventory's weapon of that type. Without a matching weapon (e.g. Unarmed) only the animation set changes.
- **The weapons are invisible for now:** there are no weapon meshes in the project yet. Set a Skeletal Mesh or Static Mesh in the data asset to show one. **Attach Socket** says where it goes on the character (default `weapon_r`, or `hand_r` if the mesh doesn't have it), **Attach Offset** lines it up, and **Muzzle Socket** marks where the shots start.
- **Fire and reload animations** are optional: set **Fire Montage** / **Reload Montage** in the data asset.

**A new weapon:** right-click in `Content/Weapons` → **Miscellaneous → Data Asset** → **ZWeaponData** and fill it in. Then make an item with a **Weapon** fragment pointing to it (ITEMS.md → How to add things). Its **Weapon Type** decides which animation set it uses and which key equips it.

## Enemies

- **`AZAICharacter`** is the base class for every AI character (zombies, other monsters, human NPCs). Each one has its own health, team, AI ranges and melee numbers (Class Defaults: **Survival|Stats**, **AI**, **AI|Melee**).
- **`AZZombieCharacter`** is the first enemy type: the Quinn mannequin with the combat template's animations and punch, until zombie models exist. 60 health, slower than a jogging player, 15 damage per hit.
- **The AI** (`AZAIController`, server only) goes for the closest living player within its aggro range, walks there and attacks when close. With a **Nav Mesh** in the level it walks around obstacles; without one it walks straight at you.
- **`AZEnemySpawner`**: place it in a level to keep spawning enemies around it (Max Alive, Spawn Interval, Spawn Radius, Total To Spawn, Enemy Class).

**A new enemy type:** right-click in the Content Browser → **Blueprint Class** → search **ZZombieCharacter** (or **ZAICharacter** for something completely different) → change its mesh, animation Blueprint, attack montage and numbers. A special attack (e.g. a spitter's acid) is a new ability added to **Default Abilities**.

**Teams** (`EZTeam`): Players, Monsters, Humans. Characters on the same team can't damage each other (no friendly fire). Friendly human NPCs use the Players team.

## Damage, health and death

- **All damage goes through `UZCombatLibrary::ApplyDamage`**, which Blueprints can use too (e.g. traps). It checks the teams, lowers the target's health through a GameplayEffect on **IncomingDamage**, and plays the hit effect.
- **Health and stamina** are attributes in `UZAttributeSet`: Health, MaxHealth, Stamina, MaxStamina. **Only the server changes them.** Health is sent to everyone, stamina only to its own player (for the HUD).
- **A player at 0 health** falls as a ragdoll and puts the weapon away; the items stay in the inventory. After **5 seconds** they respawn at a Player Start with full health and stamina. The body disappears 10 seconds later.
- **An enemy at 0 health** falls as a ragdoll, stops its AI, drops its loot (its **Loot Table**) and disappears after 10 seconds. Its Blueprint can react with the **On Died** event, which also says who killed it (e.g. to count kills later).
- **Player Starts:** each level needs at least one; one per player is best. If they're all crowded, the character is moved to a free spot nearby instead of failing to spawn.

## Effects and sounds (GameplayCues)

The code already sends these events to every player. Nothing is shown or heard until a **GameplayCue Notify** asset with the same tag exists, in `Content/GameplayCues`:

| Tag | When | Parameters |
|---|---|---|
| `GameplayCue.Weapon.Fire` | A weapon fired | Location = muzzle, Normal = shot direction |
| `GameplayCue.Weapon.Impact` | A shot hit something | Location = impact point, Normal = surface |
| `GameplayCue.Character.Hit` | A character took damage | Location = hit point, Raw Magnitude = damage |

To make one: in `Content/GameplayCues`, right-click → **Blueprint Class** → search **GameplayCueNotify_Static** → name it e.g. `GC_Weapon_Fire` → in its Class Defaults set **Gameplay Cue Tag** → override **On Execute** and use the parameters (e.g. **Spawn Sound at Location** with the Location).

Until then, the shooter sees **debug lines**: white = shot, yellow dot = hit, red dot = damage dealt (red only on the host, since only the server deals damage). Turn them off in the console with `Z.Weapon.DrawDebug 0`.

## HUD

`WBP_HUD` (in `Content/UI`) is filled in by C++ (`UZSurvivalHUDWidget`). The code finds the widgets by name, so the names must match exactly:

| Widget name | Type | Needed? | What the code does with it |
|---|---|---|---|
| `HealthBar` | Progress Bar | Yes | Health |
| `StaminaBar` | Progress Bar | Yes | Stamina |
| `AmmoText` | Text | Optional | "magazine / reserve", e.g. "12 / 60", while a weapon is in the hands, hidden otherwise |
| `Crosshair` | any (e.g. Image) | Optional | Shown while a weapon is in the hands |
| `HitMarker` | any (e.g. Image) | Optional | Flashes when a shot dealt damage (longer for a kill) |
| `StatusText` | Text | Optional | "You died. Respawning..." while dead |
| `InteractionText` | Text | Optional | "[E] Pick up Rifle Ammo x15" while looking at something to interact with |
| `InteractionProgress` | Progress Bar | Optional | Fills while holding the interact key on an interaction that takes time |

## What the other players see

| What | How it reaches them |
|---|---|
| Movement and rotation | The engine sends it |
| Looking up/down (aim offset) | The engine sends the aim pitch |
| Aiming (aim walk/idle) | `bIsAiming`, sent by the server |
| Weapon type and the weapon in the hands | `CurrentWeaponType` and `EquippedWeapon`, set through `SetWeaponType` |
| Shots, impacts, hits | GameplayCues, sent by the server |
| Attack, fire and reload animations | The ability system plays them for everyone |
| Death (ragdoll) | `bIsDead`, sent by the server |
| Enemies | They replicate like any character; only their decisions stay on the server |

## Where to change values

| Value | Where |
|---|---|
| Jog, sprint, aim walk and walk speeds, max sprint angle | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Movement** |
| Stamina drain and regen per second | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Stats** |
| Starting items (weapons, ammo, ...) | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Inventory** |
| What enemies drop | The loot table in `Content/Items/LootTables` (set on the enemy: **AI\|Loot**) |
| Weapon damage, fire rate, spread, ammo, reload time, recoil | The weapon's Data Asset in `Content/Weapons` |
| Max health and max stamina of players (100) | `UZAttributeSet` constructor in C++ |
| Enemy health, team, speed, ranges, melee damage | `AZZombieCharacter` in C++, or a Blueprint child's Class Defaults |
| Respawn delay (5 s) and dead body time (10 s) | BP_ThirdPersonGameMode → Class Defaults → **Respawn** |
| Dodge cost, speed and duration, minimum stamina to sprint | The ability's C++ constructor (or a Blueprint child of the ability) |

## Testing

**2 players:** in the editor, click the three dots ⋮ next to Play, then:
- **Number of Players:** 2
- **Net Mode:** Play As Listen Server

The **Server** window is the host. The **Client** window is the second player.

**Console commands** (open the console with the key under Esc while playing; in multiplayer, type them in the Server window):

| Command | What it does |
|---|---|
| `ZSpawnZombies 3` | Spawns 3 zombies 8 m in front of the camera |
| `ZKillEnemies` | Kills every enemy |
| `ZHeal` | Fills your health and stamina |
| `ZHurt 50` | Takes 50 health (`ZHurt 0` shows your health) |
| `Z.Weapon.DrawDebug 0` / `1` | Hides / shows the shot debug lines |
| `log LogZSurvivalGame Verbose` | Writes every shot, reload, pickup and inventory change to the Output Log |

The item commands (`ZGiveItem`, `ZShowInventory`, `ZDropItem`, `ZUseItem`, `ZSpawnPickup`) are in ITEMS.md.

## Plan: what isn't built yet

Not decided in detail. This is the direction, so new systems fit what's already there.

- **Weapon looks:** weapon meshes, muzzle flash, sounds and impacts (GameplayCues), fire and reload animations.
- **Hit reactions:** enemies flinch or stagger when hit (an ability or GameplayCue triggered by damage).
- **Downed and revive:** players go down first and teammates can revive them, before dying for real.
- **Smarter AI:** AI perception (sight and hearing) instead of distance, monsters and human NPCs fighting each other, patrols and ranged attacks, possibly with a StateTree.
- **Weapon mods and damage types** (acid, electric): GameplayEffects with tags (Blueprint children of `UZGameplayEffect_Damage`). Effects everyone sees are GameplayCues.
- **Armor weight tiers:** attributes changed by the armor's GameplayEffect, e.g. slower dodges for heavy armor.
- **Inventory screen, crafting, harvesting, storage:** built on the item system (plan in ITEMS.md).
- **Spawn protection:** a few seconds without damage after respawning, so enemies at a Player Start can't kill a player again right away.
- **Hub noise/heat and sieges:** values only the server keeps, sent to everyone through the GameState. Spawners can be started by a siege.
- **Hosting and joining over the internet:** right now only in the editor. Real hosting needs an online service (Steam or Epic Online Services).
