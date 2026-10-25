# Multiplayer & abilities

How the player's actions, health and stamina work in multiplayer, and the plan for what isn't built yet.

**Last updated:** 2026-10-25 (Gameplay Ability System and multiplayer setup added)

## The basic idea

- **One player hosts** (listen server). The host's game is the **server** and decides what really happens. The other players are **clients**.
- **Prediction:** when you press a key, your own game does the action right away so it feels instant. The server does the same action and confirms it, or corrects you if it disagrees.
- **Replication:** the server sends values (health, weapon type, ...) to the clients, so everyone sees the same thing.

## Who owns what

| What | Lives on | Why |
|---|---|---|
| Ability system, health, stamina | **PlayerState** (`AZSurvivalGamePlayerState`) | Every player has one on every machine, and it survives respawns |
| Body, camera, input | **Character** (`AZSurvivalGameCharacter`, BP_ThirdPersonCharacter) | The part you control and see |
| Sprint, aim and speed limit states | **Movement component** (`UZCharacterMovementComponent`) | Sent to the server with every move, so the server moves you at the same speed |
| Weapon type | **Character**, replicated | Everyone needs it for the animations |

## What happens when you press a key

Sprint as an example:

1. **Sprint key** → `DoSprintStart` → `PressAbilityInput(Sprint)`.
2. The ability system finds the ability whose **InputID** is Sprint and checks whether it may start: not aiming, no speed limit, more than 10 stamina.
3. **Your game runs it right away:** the Sprint ability tells the movement component "wants to sprint", so you speed up immediately.
4. **The server runs it too.** Every move you send also says "sprinting", so the server moves you at the same speed and doesn't pull you back.
5. **The server drains stamina** and sends the new value to you, so your HUD bar goes down.
6. **Releasing the key** ends the ability on both. Running out of stamina: the server ends it and tells your game.

```
Key → Character (DoSprintStart) → Ability system (on PlayerState) → Sprint ability → Movement component → speed
                                         ↑                                                   ↓
                                   server confirms                         flags sent to the server with every move
```

## The abilities

| Ability | What it does | Blocked by | Cost |
|---|---|---|---|
| **Sprint** (`UZGameplayAbility_Sprint`) | Sprint speed while the key is held | Aiming, speed limit, 10 stamina or less | Stamina per second, drained by the server |
| **Aim** (`UZGameplayAbility_Aim`) | Aim walk speed, faces the camera, camera zoom, while the right mouse button is held | | Cancels sprint |
| **Dodge** (`UZGameplayAbility_Dodge`) | Quick dash in the movement direction (forward when standing still) | Less stamina than the cost | 25 stamina |

Every ability is blocked while dead.

**Tags** are labels the abilities use to find and block each other:
- `Ability.Sprint`, `Ability.Aim`, `Ability.Dodge`: on each ability, e.g. Aim cancels anything tagged `Ability.Sprint`.
- `State.Sprinting`, `State.Aiming`, `State.Dodging`: on the player while that ability runs, e.g. Sprint is blocked by `State.Aiming`.
- `State.Dead`: on the player when health reaches 0. It blocks every ability.

## Health and stamina

- They're **attributes** in `UZAttributeSet`: Health, MaxHealth, Stamina, MaxStamina.
- **Only the server changes them.** Health is sent to everyone (for teammates' health bars later), stamina only to its own player (for the HUD).
- **IncomingDamage** is a helper attribute: damage effects add to it, and the attribute set turns it into lost health.
- **Health reaches 0:** the player gets `State.Dead`. Death animation and respawn aren't built yet.

## What the other players see

| What | How it reaches them |
|---|---|
| Movement and rotation | The engine sends it |
| Looking up/down (aim offset) | The engine sends the aim pitch |
| Aiming (aim walk/idle) | `bIsAiming`, sent by the server |
| Weapon type | `CurrentWeaponType`, set through `SetWeaponType` |

## Where to change values

| Value | Where |
|---|---|
| Jog, sprint, aim walk and walk speeds, max sprint angle | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Movement** |
| Stamina drain and regen per second | BP_ThirdPersonCharacter → Class Defaults → **Survival\|Stats** |
| Max health and max stamina (100) | `UZAttributeSet` constructor in C++ |
| Dodge cost, speed and duration, minimum stamina to sprint | The ability's C++ constructor (or a Blueprint child of the ability) |

## Testing with 2 players

In the editor, click the three dots ⋮ next to Play, then:
- **Number of Players:** 2
- **Net Mode:** Play As Listen Server

The **Server** window is the host. The **Client** window is the second player.

## Plan: what isn't built yet

Not decided in detail. This is the direction, so new systems fit what's already there.

- **Firing and reloading:** abilities, like Sprint. The client predicts the shot effects, the server checks hits and applies damage with a GameplayEffect on IncomingDamage. Reload can use the speed limit.
- **Weapons:** the equipped weapon sets the weapon type instead of the 1/2/3 keys. Weapon stats come from data assets.
- **Weapon mods and damage types** (acid, electric): GameplayEffects with tags. Effects everyone sees (acid splash, sparks) are GameplayCues.
- **Armor weight tiers:** attributes changed by the armor's GameplayEffect, e.g. slower dodges for heavy armor.
- **Zombies, other monsters and human NPCs:** one shared AI character base class with its own ability system (replication mode Minimal) and attributes. Each type differs through its abilities and data, e.g. a spitter's acid attack is an ability. Their AI only runs on the server.
- **Death and revive:** an ability that starts when the player gets `State.Dead`. Respawn or revive removes it again.
- **Inventory and crafting:** an inventory that only the server changes, replicated with `FFastArraySerializer`. Items and recipes as data assets.
- **Hub noise/heat and sieges:** values only the server keeps, sent to everyone through the GameState.
- **Hosting and joining over the internet:** right now only in the editor. Real hosting needs an online service (Steam or Epic Online Services).
