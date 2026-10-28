# Saving

How the game saves, Remnant style: each player keeps their character on their own PC and takes it into any game, and the host's save also holds the world. Also: save slots, a save menu later, world progress, renaming things saves use, and what's still needed for a Steam release.

**Last updated:** 2026-10-28 (what's equipped, and in which slot, is saved too)

## How it works

- **Your character is saved on your PC:** level and experience, trait points and traits, every item (with its durability, magazine, upgrade level and mods), what's equipped and in which slot (e.g. the third ring slot), the weapon in your hands, tradeskills and the recipes you found.
- **The host's save also holds the world:** what's been done in it (**world progress**, e.g. a boss beaten) and the level it's in.
- **Joining a friend:** your character comes with you. The host checks it against the game's rules first and changes what doesn't fit (see "What the host checks"). What you gain there is saved into your own save on your PC. Your own world stays as it was.
- **When it saves:** a few seconds after something changes (10 by default: picking up an item, experience, a trait, crafting, ...), when you save by hand (a menu later; for now the console command `ZSave`), when the game closes or the level changes, and for a player in someone else's game also when they leave or the host ends the game.
- **Not saved (yet):** health and stamina (a loaded game starts full), where you stand (you start at a Player Start; checkpoints later), items lying on the ground, enemies, and crafting in progress (nothing is used up until a craft is finished, so nothing is lost).

## Save slots

One **slot** is one character, with the world it hosts. The game uses:
1. the slot named on the command line (`-SaveSlot=Name`, for test runs), else
2. **the slot played most recently**, else
3. the first time: a new character in **Slot1**.

**Where the files are:** `Saved/SaveGames` in the project folder (in a packaged game: `%LOCALAPPDATA%\ZSurvivalGame\Saved\SaveGames`). Each slot is a `.sav` file with a `.sav.bak` next to it: the save before. If a save is ever damaged (a crash or power cut while saving), the game notices and uses the backup.

**Playing in the editor** uses your saves too, so every Play continues your character.
- **A new character every Play:** Project Settings → Game → **Saving** → untick **Use Saves In Editor**. Nothing is saved then.
- **A new character once:** type `ZNewCharacter` (or `ZNewCharacter Alex` with a name). It makes a new slot and starts the level again with it.
- **Two players in the editor:** the second window keeps its saves in `Saved/SaveGames/PIE1`, like a second PC. So the client has a character of its own, which it keeps between Plays too.

## Console commands

| Command | What it does |
|---|---|
| `ZSave` | Saves now (host: every player and the world; a joined player: their own character). Says "Game saved." |
| `ZSaveSlots` | Lists the slots on this PC: character, level, time played, when it was saved, which one is in use |
| `ZSaveInfo` | Writes what the slot in use holds to the Output Log: the character as last saved, and the world |
| `ZNewCharacter Alex` | A new character in a new slot, and the level starts again with it (the game running now is saved first) |
| `ZLoadSlot Slot2` | Continues a slot: the game running now is saved, then its level opens with that character |
| `ZDeleteSlot Slot2` | Deletes a slot for good (not the one in use) |
| `ZWorldProgress WorldProgress.Boss.StoneGolem` | Marks something as done in the world (host); `ZWorldProgress` on its own lists what's done |
| `ZBreakSave` | For testing: breaks the game's rules in the slot in use (huge numbers, items that don't exist, ...). The next Play shows what the host fixes. Only on a throwaway character (`ZNewCharacter Test` first): the fixed character is at the highest the game allows, e.g. level 50 with 999 rifle rounds |

## Settings

**Project Settings → Game → Saving:**

| Setting | What it does |
|---|---|
| **Storage Class** | Where saves are kept. Empty: files on this PC. Later another class could keep them elsewhere, e.g. on a server for online matches |
| **Save Delay** | Seconds from a change until the character is saved (10). Changes in that time are saved together |
| **Default Character Name** | The name of new characters ("Survivor") |
| **Character Arrival Timeout** | Seconds the host waits for a joining player's character (15). See "Multiplayer details" |
| **Use Saves In Editor** | Play in the editor with your saved character; off: a new character every Play, and nothing is saved |

## Making a save menu (later)

Everything a menu needs is ready, for Blueprints too: right-click in a Widget Blueprint's graph → **Get Game Instance Subsystem** → class **ZSaveSubsystem**, and drag from it.

| Node | What it does |
|---|---|
| **Get Save Slots** | Every slot on this PC, the most recently played first. Each has **Slot Name**, **Character Name**, **Level**, **Play Time**, **Last Saved**, **Map Name**, **Can Load** (false if it's damaged or from a newer version of the game) and **Is Active** |
| **Get Active Slot Info** | The slot in use |
| **Create Slot** (Character Name) | A new character in a new slot, used from now on. Returns its Slot Name |
| **Select Slot** (Slot Name) | Use this character from the next game on: for hosting, or to take it into a friend's game |
| **Load Slot** (Slot Name) | Continue this slot: its level opens, with you as the host |
| **Save Now** | Save the game running now |
| **Delete Slot** (Slot Name) | Delete a slot (not the one in use) |
| **On Game Saved** (event) | A save was written, e.g. to show a "Saving..." sign |
| **On Save Slots Changed** (event) | A slot was made, deleted or chosen: refresh the list |

A typical main menu: a list with a button per slot (Character Name and Level) that calls **Load Slot**; a **New Character** button with a text box that calls **Create Slot**, then **Load Slot** with what it returns; a **Delete** button per slot. In the pause menu: **Save** calls **Save Now**. Joining a friend comes with the online menus later: **Select Slot** first, so the right character goes along.

There's no rolling back: saves happen all the time, and loading a slot continues it from its last save (like Remnant).

## World progress (bosses beaten, shortcuts opened)

Things done for good in the host's world are **gameplay tags under `WorldProgress`**. They're saved with the host's game, and everyone in the game sees them.
1. **Make a tag:** Project Settings → **Gameplay Tags** → **Manage Gameplay Tags** → **+** → e.g. `WorldProgress.Boss.StoneGolem` or `WorldProgress.Shortcut.SubwayDoor`.
2. **When it happens** (on the server, e.g. in the boss's death event): **Get Game State** → **Cast To ZSurvivalGameState** → **Add World Progress** with the tag.
3. **Reacting to it** (e.g. a door that stays open once it was opened): in the door's **BeginPlay**, **Get Game State** → Cast → **Has World Progress**; and bind **On World Progress Changed** to react when it happens while playing.

## Renaming or removing things saves use

- **Items, traits and recipes are saved by their asset name** (e.g. `ID_Rifle`), not their folder: moving them to another folder is fine.
- **Renaming one** (e.g. `ID_Bandage` to `ID_Bandage_Small`): saves still have the old name. Add a redirect: Project Settings → Game → **Asset Manager** → **Primary Asset Id Redirects** → **+** → **Old** `Item:ID_Bandage`, **New** `Item:ID_Bandage_Small` (traits: `Trait:`, recipes: `Recipe:`).
- **An item or recipe the game doesn't have any more** stays in the save as it was, and comes back if it's added again. The log says so when the character loads.
- **Gameplay tags** (item stats, tradeskills, world progress): rename them in **Manage Gameplay Tags** (right-click → **Rename**), which adds a redirect by itself.

## What the host checks

A character from a save could come from an older version of the game, or be edited by hand. The host (also when you play alone) puts it back within the rules:
- experience up to the highest level, and bonus trait points that fit the limit
- trait ranks up to each trait's max; if the traits cost more points than the character has, they're reset (the points can be spent again)
- tradeskill experience up to the highest level; only real tradeskills
- only items the game has (others are kept in the save), at most a stack of each, at most 500 stacks
- durability up to the max, magazine ammo up to what the magazine holds with its mods, upgrades only as high as an upgrade recipe goes, and only mods that fit (others go back to the inventory)
- equipment: only items in a slot they go in, one per slot, and only slots players have (an item that doesn't fit its saved slot goes in a free one of its kind, or is just in the inventory). A save from before there was equipment: the weapon that was in the hands is equipped

If anything was changed, the player gets a message, and the host's Output Log lists what.

These checks keep a character within the game's rules, but a player can still edit their own save, e.g. give themselves items. That's normal for co-op with friends. Online matches with strangers (e.g. a 1v1 arena) would need characters kept on a server nobody can edit, through **Storage Class**, or equal gear in the arena.

## Multiplayer details

- **A joining player spawns once their character is in the game.** The host waits up to **Character Arrival Timeout** (15 s). If it doesn't arrive, they play a new character that isn't saved, and their own save stays as it was. They get a message.
- **Only the first character a player sends counts,** so sending an older save again can't bring back items they gave away.
- **While playing, the host builds each player's save** and sends it to them a few seconds after something changes; their machine writes it. So a save holds what the host's game says the player has.
- **When a joined player leaves, or the host ends the game,** the host can't send anything any more. So the player's own machine writes their character one last time, from what the host had sent it (its copy of the inventory, level, traits, ...). Only a character the host loaded from that save, once all of it has arrived: a new character played because theirs came too late never overwrites their save.
- **A save only goes into the slot it came from** (every character has an ID). If a player chooses another slot during a game, that game's progress isn't written into it.

## Plan for release (Steam)

This design stays for release: it's how co-op games like Remnant 2 and Valheim save. What's still needed, and when.

**When Steam goes into the game:**
- **Steam Cloud**, so a character goes along to another PC. Easiest is **Steam Auto-Cloud**: settings on the Steamworks website, no code. Sync the `*.sav` and `*.sav.bak` files in the save folder (`%LOCALAPPDATA%\ZSurvivalGame\Saved\SaveGames`), not the `*.tmp` ones. Steam syncs when the game starts and closes, and asks the player if two PCs disagree.
- **Saves per Steam account**, so two accounts on one PC (e.g. Family Sharing) don't share characters: the save folder becomes the player's Steam ID (the save system can already keep separate folders, like `-SaveProfile`).

**When the multiplayer menus are built:**
- **Joining over the internet:** Steam lobbies, invites and "join a friend" through the Steam Online Subsystem plugin. Joining already brings the character along, wherever the host is.
- **The save menu** (see "Making a save menu"): choosing a character before hosting or joining.

**Before release:**
- **Test a packaged build** (Shipping): it saves to `%LOCALAPPDATA%` instead of the project folder, and the test commands (`ZBreakSave`, ...) aren't in it.
- **Tell the player when saving fails**, e.g. a full disk (now it's only in the log).
- **Keep saves from every released version** and load them before each update: an update must never break old saves. A change old saves can't read as they are needs a new save version and a conversion step (see below).

**As the game grows:**
- **Checkpoints:** where the player continues (for now, a Player Start).
- **More of the world:** doors and shortcuts opened, chests looted. Simple "it happened" things are world progress tags already; objects with their own state (e.g. what's left in a storage chest) need saveable objects added to the world save.

**Decisions for later:**
- **Co-op with strangers (public matchmaking):** a player can edit their own save and bring a maxed character (within the rules) into a stranger's game, as in Remnant 2 or Elden Ring. Options: friends-only games by default, or light protection against editing saves (stops casual editing, not determined cheaters).
- **Online matches against strangers** (e.g. a 1v1 arena): need characters kept on a server nobody can edit (a new **Storage Class**), or the same gear for both players in the arena.

## For later: saving something new (C++)

1. A field in `FZCharacterSaveData` (things that go with the player) or `FZWorldSaveData` (the host's world), in `SaveGame/ZSaveTypes.h`.
2. Fill it in `UZPlayerSaveComponent::BuildCharacterData`, and put it back in `ApplyCharacter`, checked against the game's rules (for a component's own data, a `Restore...` function on the component, like `UZProgressionComponent::RestoreProgress`).
3. Older saves just don't have the new field and get its default. Only a change that old saves need converting for gets a new `EZSaveVersion` and a step in `UZSaveGame::UpgradeToLatest`.
