# Crafting

How crafting works: combining items, tools, crafting stations and tradeskills, and how to add new recipes.

**Last updated:** 2026-10-26 (crafting by combining items, tools, stations and tradeskills added)

## How players craft

- **There's no recipe list.** Players find recipes themselves by combining items and seeing what happens, like in Anarchy Online. Nothing in the game shows a recipe.
- **A combination works** when the chosen items are exactly the kinds of items a recipe takes (nothing missing, nothing extra). The inventory then needs enough of each, the tools have to be good enough, a station has to be nearby if the recipe needs one, and the tradeskills have to be high enough.
- **The server crafts it**, which takes the recipe's Craft Time. Combining the same items again makes another; a count can craft several in a row.
- **The first time** a player makes a recipe it's a discovery: "New discovery!" and extra tradeskill experience.
- For now, combining is done with the console command `ZCombine`. The inventory screen (dragging one item onto another, combine slots) comes next.

What players are told when it doesn't work. It never names the recipe, only what was tried:

| Message | Why |
|---|---|
| Nothing comes of combining these. | These kinds of items aren't a recipe |
| Something could come of this, but you need more. | Right items, not enough of one of them |
| Your tools aren't good enough for this. | The tool's quality is too low, or it's broken |
| You need Electronics 20 for this. You have 8. | A tradeskill is too low |
| This needs a Workbench nearby. | The recipe needs a crafting station |

## Tradeskills

- **Mechanics, Electronics, Chemistry, Medicine and Weaponsmithing.** Each player has their own; they're kept when dying (not saved between game sessions yet).
- **A recipe can need tradeskill levels, and that's a hard limit:** below it, it can't be made at all.
- **Crafting is how they go up.** Every craft gives experience in the recipe's tradeskills. Easy recipes teach less and less: 25 levels above a recipe it teaches nothing ("There's nothing more to learn from making this."). So players have to move on to harder recipes to keep improving.
- **Levels:** reaching level L takes 10 × L^1.5 experience in total (level 10 = 316, level 25 = 1250, level 100 = 10000). Change the numbers in **Project Settings → Game → Progression**.

## Tools

- **A tool is an item with a Tool fragment** (its **Quality**: 1 basic, 2 precision, 3 advanced, ...) and a tag for what kind of tool it is (e.g. `Item.Tool.Screwdriver`).
- **Recipes ask for a kind of tool and a minimum quality.** Better tools open up harder recipes, and a quality 2 screwdriver also does everything a quality 1 one does.
- **Tools are kept, but wear out:** with a Durability fragment, they lose the recipe's Durability Cost each craft and break at 0. Turn off **Breaks When Empty** to keep broken tools (for repairing later).
- **A recipe can also use a tool up** (Usage: Consumed), e.g. a screwdriver that's turned into a precision screwdriver.

## Crafting stations

- **BP_Workbench** (`Content/Crafting/Stations`): recipes whose Required Station is `Crafting.Station.Workbench` only work within 4 m of one (its Crafting Radius).
- **Standing nearby is enough.** Using it with the interact key says the player is at the workbench; later it will open the inventory screen.
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

New items for them (`Content/Items`): Screwdriver, Precision Screwdriver and Ammo Press (tools), Duct Tape, Electronic Parts, Gunpowder and Circuit Board (materials), Broken Radio (salvage) and Medkit.

## How to add things

**A new recipe:**
1. In `Content/Crafting/Recipes`, right-click → **Miscellaneous → Data Asset → ZCraftingRecipe**. Name it `RC_...`.
2. **Inputs:** one entry per kind of item. Set **Item** (or **Item Tags** for any item of a kind, e.g. `Item.Tool.Screwdriver`), **Count** and **Usage** (Consumed, or Tool for a tool that's kept). For tools also **Min Tool Quality** and **Durability Cost**.
3. **Required Station** (optional), **Skill Requirements** (a tradeskill and its minimum level), **Skill Experience** per craft and **Discovery Experience** for the first time.
4. **Results** and **Craft Time**.

Nothing else needs to list it: the game finds every recipe in `Content/Crafting` by itself, and deleting the asset removes the recipe. **Developer Notes** are for the team only. Right-click the asset → **Asset Actions → Validate Assets** checks it, e.g. for missing results or a tool quality the item doesn't have.

Two recipes that take exactly the same kinds of items can't both be found (the first one that can be crafted wins). The log warns about it when the game starts: "take the same kinds of items".

**A new tool:** an item with the tags `Item.Type.Tool` and `Item.Tool.<Kind>`, a **Tool** fragment (Quality) and usually a **Durability** fragment.

**A new kind of tool, station or tradeskill:** a new gameplay tag (`Item.Tool.*`, `Crafting.Station.*` or `Tradeskill.*`), in C++ (`ZGameplayTags`) or in **Project Settings → Gameplay Tags**. A new tradeskill counts from then on, with no other change.

## Multiplayer

- **The player's machine only asks** ("combine these items"); the server checks everything again, takes the ingredients, wears the tools and gives the results.
- **Tradeskills, discoveries and the current craft go only to that player.** Each player has their own.
- **Stations are found by distance on the server,** with a little slack for players who just moved.

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
| `ZForgetDiscoveries` | Forgets your discoveries, so the first-time bonus happens again (*host*) |
| `ZListRecipes` | Writes every recipe to the Output Log (for you, players never see this) |
| `ZDelay 3 ZSkills` | Runs a command after some seconds (for automated test runs) |
| `ZOnClients ZSkills` | Runs a command on every client's machine (*host*, for automated test runs) |

## Plan: what isn't built yet

- **Inventory screen:** drag one item onto another to combine them, combine slots for more items, and the tradeskills.
- **Trait points** that make crafting better (more results, faster learning).
- **Weapon upgrades and weapon mods,** made with Weaponsmithing.
- **Saving** tradeskills and discoveries.
- **Harvesting** (trees, scrap) for materials, and more stations (forge, chemistry set).
