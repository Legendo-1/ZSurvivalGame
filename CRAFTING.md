# Crafting

How crafting works: combining items, tools, crafting stations and tradeskills, weapon upgrades and mods, and how to add new recipes.

**Last updated:** 2026-10-27 (tradeskills and recipes found are saved with the character, see SAVING.md)

## How players craft

- **There's no recipe list.** Players find recipes themselves by combining items and seeing what happens, like in Anarchy Online. Nothing in the game shows a recipe.
- **A combination works** when the chosen items are exactly the kinds of items a recipe takes (nothing missing, nothing extra). The inventory then needs enough of each, the tools have to be good enough, a station has to be nearby if the recipe needs one, and the tradeskills have to be high enough.
- **The server crafts it**, which takes the recipe's Craft Time. Combining the same items again makes another; a count can craft several in a row.
- **The first time** a player makes a recipe it's a discovery: "New discovery!" and extra tradeskill experience.
- **Some recipes change an item instead of making one:** upgrading a weapon, installing or taking off mods, repairing a tool. The item they change is kept (their **Target**). They're made once per combine, so each change gets its own message, e.g. "Your Rifle is now +2."
- **Combining happens in the inventory screen** (I or Tab, see UI.md): drag one item onto another, or put up to 6 items in the combine slots, choose how many and press Combine. The console command `ZCombine` does the same.

What players are told when it doesn't work. It never names the recipe, only what was tried:

| Message | Why |
|---|---|
| Nothing comes of combining these. | These kinds of items aren't a recipe |
| Something could come of this, but you need more. | Right items, not enough of one of them |
| Your tools aren't good enough for this. | The tool's quality is too low, or it's broken |
| You need Electronics 20 for this. You have 8. | A tradeskill is too low |
| This needs a Workbench nearby. | The recipe needs a crafting station |
| Your Rifle can't be upgraded any further. / The Long Barrel doesn't fit your Pistol. | A recipe that changes an item can't do it to that one; it says why |

## Tradeskills

- **Mechanics, Electronics, Chemistry, Medicine and Weaponsmithing.** Each player has their own; they're kept when dying and saved with the character, like the recipes found (SAVING.md).
- **A recipe can need tradeskill levels, and that's a hard limit:** below it, it can't be made at all.
- **Crafting is how they go up.** Every craft gives experience in the recipe's tradeskills. Easy recipes teach less and less: 25 levels above a recipe it teaches nothing ("There's nothing more to learn from making this."). So players have to move on to harder recipes to keep improving.
- **Levels:** reaching level L takes 10 × L^1.5 experience in total (level 10 = 316, level 25 = 1250, level 100 = 10000). Change the numbers in **Project Settings → Game → Progression**.
- **Traits help** (PROGRESSION.md): **Studious** gives more tradeskill experience, and **Scavenger** gives a chance that a craft makes its results twice ("Lucky: you made extra.").

## Tools

- **A tool is an item with a Tool fragment** (its **Quality**: 1 basic, 2 precision, 3 advanced, ...) and a tag for what kind of tool it is (e.g. `Item.Tool.Screwdriver`).
- **Recipes ask for a kind of tool and a minimum quality.** Better tools open up harder recipes, and a quality 2 screwdriver also does everything a quality 1 one does.
- **Tools are kept, but wear out:** with a Durability fragment, they lose the recipe's Durability Cost each craft and break at 0. Turn off **Breaks When Empty** to keep broken tools.
- **Worn tools can be repaired:** any tool + Duct Tape x2 brings it back to full durability (Mechanics 5). A tool that broke and was removed is gone.
- **A recipe can also use a tool up** (Usage: Consumed), e.g. a screwdriver that's turned into a precision screwdriver.

## Weapon upgrades and mods

- **Upgrading:** a weapon + Weapon Parts x3 + Scrap Metal x5 at a workbench makes it +1, up to its weapon data's **Max Upgrade Level** (5). Each level adds **Damage Per Upgrade Level** (10%) damage.
- **Each upgrade needs more Weaponsmithing:** 10 per level the weapon already has. +0 → +1 works for anyone, +4 → +5 needs Weaponsmithing 40 ("Upgrading your Rifle to +5 needs Weaponsmithing 40. You have 12."). A hard limit, like recipe skill levels.
- **Mods** are items made with Weaponsmithing (see the recipes below). Each goes into one **slot** (Barrel, Sight or Magazine). A weapon holds one mod per slot, and only has the slots its weapon data lists (**Mod Slots**).
- **Installing:** a weapon + a mod + a screwdriver (tool). A mod already in that slot comes back to the inventory. **Taking mods off:** a weapon + a screwdriver puts every mod back in the inventory. Neither needs a tradeskill (making the mod is the hard part) or gives experience, so installing and removing over and over doesn't train anything.
- **A weapon keeps its upgrades and mods** when it's put away, dropped and picked up again.
- **The stats:** each is the weapon data's value × (1 + the bonuses of its mods and upgrades). Then the holder's traits go on top (damage, recoil, reload speed; PROGRESSION.md). When a magazine gets smaller (an Extended Magazine comes off), the extra rounds go back into the reserve ammo.

| Mod | Slot | Does |
|---|---|---|
| Compensator | Barrel | Recoil −25% |
| Long Barrel | Barrel, rifles only | Damage +8%, Range +25%, Spread −10% |
| Reflex Sight | Sight | Spread −30% |
| Extended Magazine | Magazine | Magazine Size +40%, Reload Time +10% |

## Crafting stations

- **BP_Workbench** (`Content/Crafting/Stations`): recipes whose Required Station is `Crafting.Station.Workbench` only work within 4 m of one (its Crafting Radius).
- **Standing nearby is enough.** Using it with the interact key opens the inventory screen, which shows "At the Workbench".
- **A new kind of station:** a Blueprint child of `ZCraftingStation` with a new `Crafting.Station.*` tag in Station Tags, a Display Name and its own mesh.

## The recipes there are

| Recipe | Combine | Makes | Needs |
|---|---|---|---|
| `RC_Bandage` | Cloth x3 | Bandage | Medicine 0 |
| `RC_Medkit` | Bandage x2 + Duct Tape | Medkit (heals 60) | Medicine 10 |
| `RC_SalvageRadio` | Broken Radio + screwdriver (tool, 2 durability) | Electronic Parts x2, Scrap Metal | Electronics 0 |
| `RC_Gunpowder` | Bone Fragment x2 + Rotten Meat | Gunpowder x2 | Workbench, Chemistry 0 |
| `RC_PistolAmmo` | Scrap Metal + Gunpowder | Pistol Ammo x12 | Workbench, Mechanics 0 |
| `RC_RifleAmmo` | Scrap Metal x2 + Gunpowder x2 + ammo press (tool) | Rifle Ammo x15 | Workbench, Mechanics 15 |
| `RC_Screwdriver` | Scrap Metal x2 + Duct Tape | Screwdriver | Workbench, Mechanics 0 |
| `RC_AmmoPress` | Scrap Metal x6 + Duct Tape x2 + screwdriver (tool) | Ammo Press | Workbench, Mechanics 10 |
| `RC_PrecisionScrewdriver` | Electronic Parts + Scrap Metal x2 + a screwdriver (used up) | Precision Screwdriver (quality 2) | Workbench, Mechanics 20 |
| `RC_CircuitBoard` | Electronic Parts x3 + Duct Tape + quality 2 screwdriver (tool) | Circuit Board | Workbench, Electronics 20 |
| `RC_WeaponParts` | Scrap Metal x3 + Electronic Parts | Weapon Parts x2 | Workbench, Mechanics 5 (also trains Weaponsmithing) |
| `RC_Compensator` | Weapon Parts x2 + Duct Tape | Compensator | Workbench, Weaponsmithing 5 |
| `RC_ExtendedMag` | Weapon Parts x2 + Scrap Metal x3 + Duct Tape | Extended Magazine | Workbench, Weaponsmithing 10 |
| `RC_LongBarrel` | Weapon Parts x3 + Scrap Metal x4 | Long Barrel | Workbench, Weaponsmithing 15 |
| `RC_ReflexSight` | Circuit Board + Weapon Parts + quality 2 screwdriver (tool) | Reflex Sight | Workbench, Weaponsmithing 20, Electronics 15 |
| `RC_UpgradeWeapon` | any weapon (target) + Weapon Parts x3 + Scrap Metal x5 | the weapon +1 | Workbench, Weaponsmithing 10 per level it has |
| `RC_InstallMod` | any weapon (target) + any mod + screwdriver (tool, 1 durability) | the mod on the weapon | The weapon has the mod's slot |
| `RC_RemoveMods` | any weapon (target) + screwdriver (tool, 1 durability) | the mods back in the inventory | Mods on the weapon |
| `RC_RepairTool` | any tool (target) + Duct Tape x2 | the tool at full durability | Mechanics 5 |

New items for them (`Content/Items`): Screwdriver, Precision Screwdriver and Ammo Press (tools), Duct Tape, Electronic Parts, Gunpowder, Circuit Board and Weapon Parts (materials), Broken Radio (salvage), Medkit, and the four mods (`Items/Mods`).

## How to add things

**A new recipe:**
1. In `Content/Crafting/Recipes`, right-click → **Miscellaneous → Data Asset → ZCraftingRecipe**. Name it `RC_...`.
2. **Inputs:** one entry per kind of item. Set **Item** (or **Item Tags** for any item of a kind, e.g. `Item.Tool.Screwdriver`), **Count** and **Usage** (Consumed, Tool for a tool that's kept, or Target for the item the recipe changes). For tools also **Min Tool Quality** and **Durability Cost**.
3. **Required Station** (optional), **Skill Requirements** (a tradeskill and its minimum level), **Skill Experience** per craft and **Discovery Experience** for the first time.
4. **Results** and **Craft Time**.
5. **Outcomes** (optional), for a recipe that changes its Target input: click **+** and pick one. **Upgrade Target** (a stat goes up, e.g. the upgrade level), **Install Mod**, **Remove Mods** or **Repair Target**. A recipe needs Results, Outcomes or both; with Outcomes it needs exactly one input with Usage Target, Count 1.
6. **Effects** (optional): **Working Cue** plays for as long as the recipe is being made (only with a Craft Time; at the station if it needs one), **Done Cue** when something was made. They start as `GameplayCue.Crafting.Working` and `GameplayCue.Crafting.Done`, which every recipe shares. A child tag (e.g. `GameplayCue.Crafting.Working.Chemistry`) gives a recipe its own sound, an empty one none. How to make the sounds: AUDIO.md.

Nothing else needs to list it: the game finds every recipe in `Content/Crafting` by itself, and deleting the asset removes the recipe. **Developer Notes** are for the team only. Right-click the asset → **Asset Actions → Validate Assets** checks it, e.g. for missing results, a tool quality the item doesn't have, or Outcomes without a Target.

Two recipes that take exactly the same kinds of items can't both be found (the first one that can be crafted wins). The log warns about it when the game starts: "take the same kinds of items".

**A new tool:** an item with the tags `Item.Type.Tool` and `Item.Tool.<Kind>`, a **Tool** fragment (Quality) and usually a **Durability** fragment.

**A new weapon mod:** an item in `Content/Items/Mods` with the tag `Item.Type.WeaponMod` and a **Weapon Mod** fragment: its **Slot**, its **Modifiers** (a stat and a bonus: 0.15 = +15%, −0.25 = −25%) and optionally **Allowed Weapon Types**. Then a recipe that makes it. `RC_InstallMod` takes any item tagged `Item.Type.WeaponMod`, so installing it needs nothing else. A new slot is a new `Weapon.ModSlot.*` tag, added to the weapons' **Mod Slots**.

**A new kind of change** (e.g. sharpening a blade): a C++ child of `UZRecipeOutcome` with `CanApply` (can it happen, and if not, why) and `Apply`. It shows up in every recipe's Outcomes list, and existing recipes don't change.

**A new kind of tool, station or tradeskill:** a new gameplay tag (`Item.Tool.*`, `Crafting.Station.*` or `Tradeskill.*`), in C++ (`ZGameplayTags`) or in **Project Settings → Gameplay Tags**. A new tradeskill counts from then on, with no other change.

## Multiplayer

- **The player's machine only asks** ("combine these items"); the server checks everything again, takes the ingredients, wears the tools, changes the target and gives the results.
- **Tradeskills, discoveries and the current craft go only to that player.** Each player has their own.
- **Stations are found by distance on the server,** with a little slack for players who just moved.
- **Upgrades and mods replicate with the weapon's item,** to its owner. The weapon in the hands also sends its item to its owner, so their machine works out spread, recoil, the magazine and reload time with the upgrades and mods.

## Console commands

Open the console with the key under Esc. Commands marked *host* only work in the host's (Server) window.

| Command | What it does |
|---|---|
| `ZCombine ID_BrokenRadio ID_Screwdriver` | Combines these items from your inventory; a number at the end crafts that many, e.g. `ZCombine ID_Scrap ID_Gunpowder 3` |
| `ZSkills` | Writes your tradeskills, discoveries and what you're crafting to the Output Log |
| `ZSetSkill Electronics 20` | Sets one of your tradeskills (*host*) |
| `ZGiveCraftingKit` | Gives you materials and a screwdriver to try recipes with; `ZGiveCraftingKit all` gives every player one (*host*) |
| `ZSpawnWorkbench` | A workbench 2 m in front of you; `ZSpawnWorkbench all` for every player (*host*) |
| `ZSetDurability ID_Screwdriver 1` | Sets the durability of your first item of that kind, e.g. to see it break (*host*) |
| `ZWeaponStats` | Writes the stats of the weapon in your hands to the Output Log: upgrade level, mods, damage, spread, recoil, magazine, reload time |
| `ZGiveItem ID_Mod_Compensator 1 all` | Gives every player an item, e.g. mods to install (*host*) |
| `ZForgetDiscoveries` | Forgets your discoveries, so the first-time bonus happens again (*host*) |
| `ZListRecipes` | Writes every recipe to the Output Log (for you, players never see this) |
| `ZDelay 3 ZSkills` | Runs a command after some seconds (for automated test runs) |
| `ZOnClients ZSkills` | Runs a command on every client's machine (*host*, for automated test runs) |

## Plan: what isn't built yet

- **Mods you can see:** a mesh per mod on the weapon's sockets (now they only change the stats).
- **Saving** tradeskills and discoveries.
- **Harvesting** (trees, scrap) for materials, and more stations (forge, chemistry set).
