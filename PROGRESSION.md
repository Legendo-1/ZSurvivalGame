# Progression

How character levels, trait points and traits work, and how to add new traits.

**Last updated:** 2026-10-28 (the Armor stat; equipment changes the same stats as traits)

## The idea

- **Power comes from traits and gear, not from stats that lock a play style.** Traits are small bonuses (more health, less recoil, faster reloads, ...). Later, weapon upgrades and mods carry most of the power.
- **Trait points can be taken back and spent again,** so trying another build never ruins a character.
- **Tradeskills are separate:** they only go up by crafting (see CRAFTING.md).

## Levels

- **Every player gets experience when an enemy dies**, the same amount for everyone (co-op). Each enemy has an **Experience Reward** (Class Defaults → **AI|Rewards**), 10 for a zombie.
- **Level L takes** 100 × (L − 1)^1.5 experience in total: level 2 = 100, level 10 = 2700, level 50 (the highest) = 34300.
- **Each level gives 1 trait point.**
- All of these numbers are in **Project Settings → Game → Progression**.
- **A new level plays `GameplayCue.Character.LevelUp`** on the player, for everyone near them, e.g. a fanfare and a glow. How to make it: AUDIO.md.

## Trait points

- **Where they come from:**
  - levels
  - bosses: an enemy's **Trait Point Reward** (Class Defaults → **AI|Rewards**, e.g. 1 to 3 for a boss; every player gets them)
  - items with a **Grants Progression** fragment, like the **Tome of Knowledge** (1 point). Hide them in secret places.
- **Spending:** in the inventory screen's character panel (I or Tab, see UI.md), the **+** next to a trait. Each rank costs its **Points Per Rank** (1 for now).
- **Taking them back:** the **Reset traits** button in the same panel. Turn it off with **Allow Trait Reset** in the Progression settings (the button is hidden then).
- **Max Trait Points** limits how many a player can earn in total (0 = no limit).

## The traits there are

All go up to rank 10. The bonus is per rank.

| Trait | Category | Bonus per rank |
|---|---|---|
| `TR_Vigor` Vigor | Survival | Max Health +10 |
| `TR_Endurance` Endurance | Survival | Max Stamina +10 |
| `TR_SecondWind` Second Wind | Survival | Stamina Regen +8% |
| `TR_Footwork` Footwork | Survival | Dodge Distance +4% |
| `TR_Marksman` Marksman | Combat | Damage +3% |
| `TR_SteadyHands` Steady Hands | Combat | Recoil −5% |
| `TR_QuickHands` Quick Hands | Combat | Reload Speed +5% |
| `TR_Scavenger` Scavenger | Crafting | Crafting Yield Chance +3% (a lucky craft gives the results twice) |
| `TR_Studious` Studious | Crafting | Tradeskill Experience +10% |

## How it works

- **Traits are Data Assets** (`ZTraitDefinition`) in `Content/Progression/Traits`. The game finds them by itself (Asset Manager).
- **Each trait has Modifiers:** a stat (an attribute) and how much one rank adds.
- **All of a player's trait bonuses are one gameplay effect** on their ability system, put on by the server whenever their traits change. Equipment has its own effect the same way, so traits and equipment add up.
- **When max health or max stamina goes up, the current value goes up by the same share** (full health stays full).

The stats traits can change (armor and jewelry change the same ones while they're worn, see ITEMS.md → Equipment). They're attributes in `ZAttributeSet`, and 1 means normal:

| Stat | What it does |
|---|---|
| `MaxHealth`, `MaxStamina` | Health and stamina (normally 100) |
| `DamageMultiplier` | Weapon damage |
| `RecoilMultiplier` | Weapon recoil (lower kicks less) |
| `ReloadSpeedMultiplier` | How fast reloading is (higher is faster) |
| `DodgeDistanceMultiplier` | How far a dodge goes |
| `StaminaRegenMultiplier` | How fast stamina comes back |
| `CraftingYieldChance` | Chance (0 to 1) that a craft gives its results twice (normally 0) |
| `TradeskillExperienceMultiplier` | Tradeskill experience from crafting |
| `Armor` | Lowers the damage taken (normally 0): damage × 100 / (100 + Armor), so 100 armor halves it. Mostly from worn armor |

## How to add things

**A new trait:**
1. In `Content/Progression/Traits`, right-click → **Miscellaneous → Data Asset → ZTraitDefinition**. Name it `TR_...`.
2. Fill in **Display Name**, **Description**, **Category** (`Trait.Category.Survival`, `.Combat` or `.Crafting`), **Max Rank** and **Points Per Rank**.
3. **Modifiers → +:** pick the **Attribute** and the **Value Per Rank**. For multipliers 0.05 means +5% per rank; use a minus for less (e.g. −0.05 recoil).

Right-click the asset → **Asset Actions → Validate Assets** checks that the attribute is one traits can change.

**A trait for something new** (e.g. melee damage or move speed): a new attribute in `ZAttributeSet` (C++), added to `GetTraitAttributes`, and the code that does the thing reads it. From then on any trait can use it.

**A boss:** in the enemy's Class Defaults → **AI|Rewards**, set **Trait Point Reward** (and a bigger **Experience Reward**).

**A hidden tome:** drag a **Z Pickup** into a secret spot in the level and set its **Item** to `ID_TomeOfKnowledge`.

## Multiplayer

- **The server** decides experience and trait points, raises and resets traits (the player's machine only asks) and puts on the effect.
- **The player's machine** gets its own level, points and traits. The stats come as attributes, so recoil, reload time and dodge distance are the same on both machines.
- **Each player has their own.** Kills give every player experience.
- **Saved with the character** on the player's own PC: experience, bonus trait points and traits (SAVING.md). Loading a character doesn't show level-up messages; the host checks the traits don't cost more points than the character has.

## Console commands

Open the console with the key under Esc. Commands marked *host* only work in the host's (Server) window.

| Command | What it does |
|---|---|
| `ZTraits` | Writes your level, trait points, every trait and your stats to the Output Log |
| `ZRaiseTrait Vigor` | Spends points on one rank (the trait's asset name, with or without `TR_`) |
| `ZResetTraits` | Takes back all your spent points |
| `ZGiveXP 500` | Gives you experience; `ZGiveXP 500 all` gives every player some (*host*) |
| `ZGiveTraitPoints 3` | Gives you trait points; `ZGiveTraitPoints 3 all` for every player (*host*) |

## Plan: what isn't built yet

- **Unlocking traits** by doing things (e.g. a trait that appears after beating a boss).
- **Where resets happen:** maybe only at the hub, or with a rare item.
