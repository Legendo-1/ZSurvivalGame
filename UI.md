# UI

How the inventory screen and the HUD work, how to change their keys, and how to give them your own look.

**Last updated:** 2026-10-28 (the ammo and the crosshair only show for guns)

## The inventory screen

- **Open and close it** with **I** or **Tab** (gamepad: the **View** button). It also opens when you use a crafting station with the interact key. **Esc** and gamepad **B** close it too (in the editor, Esc may stop Play instead; I and Tab always work).
- **While it's open** the character stands still and can't shoot, and the mouse cursor shows. The game doesn't pause (it's co-op).
- **Far left, the equipment window:** a slot for each thing you can equip, in groups: **Weapons** (short gun, long gun, melee), **Armor** (head, torso, legs) and **Jewelry** (neck, 4 rings). An empty slot shows its name in grey. At the top: your armor and how much damage gets through. How equipment works: ITEMS.md → Equipment.
- **Left:** your items, sorted by type (weapons, armor, jewelry, mods, ammo, consumables, tools, materials, salvage). **Equipped** marks what you have equipped, **In hands** the weapon you're holding, and the bar at the bottom of a slot is its durability.
- **Middle:** what the item under the mouse (or the selected one) is: its description and stats, e.g. a weapon's damage and what's in its mod slots, a mod's bonuses, where armor is worn and what it gives, durability, healing. Below that, buttons for the selected item (**Use** / **Equip** / **Unequip** / **Read**, **Drop**, **Add to combine**) and the **combine area**.
- **Right:** the character: level and experience, trait points, every trait with a **+** button, **Reset traits**, the tradeskills, and how many recipes you've found.

| Mouse | What it does |
|---|---|
| Click an item | Selects it (the buttons work on it) |
| Right-click or double-click | Uses it: equips a weapon or armor, uses a bandage, reads a tome. On something equipped: takes it off |
| Drag onto an equipment slot | Equips it in that slot (only where it fits: the slot lights up green). Onto a slot with a weapon in it, a mod is tried with the weapon |
| Drag out of an equipment slot | Takes it off (dropped onto another slot of its kind, e.g. ring 1 onto ring 3: the two swap). It's never dropped on the ground from there |
| Drag onto another item | Tries the two together |
| Drag onto the combine area | Puts it in the combine slots |
| Drag outside the window | Drops the whole stack on the ground |
| Click an item in the combine slots | Takes it out again |

**Gamepad:** the D-pad or left stick moves between items and equipment slots, **A** selects, **X** uses (in the equipment window: takes it off), **Y** puts the item in the combine slots, **B** closes.

**Combining:** put up to 6 items in the combine slots, choose how many with **−** and **+**, and press **Combine**. Recipes stay secret: the screen never says what goes together, only what happened ("Nothing comes of combining these.", "You need Electronics 20 for this. You have 8."). The items stay in the slots, so the same combination can be made again; the ones that were used up drop out. A craft that takes time shows a progress bar and can be stopped with **Stop**. **At the Workbench** shows when a crafting station is close enough.

## Multiplayer

- **It only shows the player's own things:** their items, traits and tradeskills are sent to that player only.
- **Everything it does is a request the server checks:** using, equipping, dropping, combining, raising and resetting traits. The answer comes back like any other change, and the screen updates by itself.
- **Each player has their own screen.** Nothing on it is shared.

## Changing the keys

`Content/Input/IMC_Default` → **IA_Inventory**: add, remove or change keys there. The **Close** button shows the first keyboard key, e.g. "Close (I)", and the screen closes on every key mapped to it.

The weapon keys are there too: **IA_ShortGun** (1), **IA_LongGun** (2) and **IA_Melee** (3). Which slot each one takes out is set in `BP_ThirdPersonCharacter` → **Class Defaults → Input → Weapon Slot Inputs**.

## Your own look

The screen is made in C++ (`UZInventoryScreenWidget`) and has a complete built-in look. To make your own:

1. Content Browser → right-click → **User Interface → Widget Blueprint** → **Pick Parent Class**: search **ZInventoryScreenWidget** → name it e.g. `WBP_Inventory`.
2. Design it. The code finds widgets by their names (below); all of them are optional, so leave out what you don't want.
3. `BP_ThirdPersonPlayerController` → **Class Defaults** → **UI** → **Inventory Screen Class** → `WBP_Inventory`.

The same works for the parts: a Widget Blueprint with **ZItemSlotWidget**, **ZItemDetailsWidget**, **ZEquipmentPanelWidget**, **ZCharacterPanelWidget**, **ZTraitRowWidget** or **ZTradeskillRowWidget** as its parent class, set in **Item Slot Class**, **Trait Row Class** or **Tradeskill Row Class**. A Widget Blueprint without any widgets of its own gets the built-in look, so one can also just change settings (e.g. the slot colors).

**ZInventoryScreenWidget** (the screen)

| Name | Type | What it's for |
|---|---|---|
| `WindowPanel` | any | The window: an item dragged outside it is dropped on the ground |
| `ItemsPanel` | Wrap Box (or another panel) | The item slots go in here; put it in a Scroll Box |
| `ItemCountText` | Text | "15 items" |
| `ItemDetails` | ZItemDetailsWidget | The item under the mouse, or the selected one |
| `UseButton`, `UseButtonText` | Button, Text | Uses the selected item; the text becomes "Equip", "Use" or "Read" |
| `DropButton` | Button | Drops the selected stack |
| `AddToCombineButton` | Button | Puts the selected item in the combine slots |
| `CombineArea` | any | Items dragged onto it go in the combine slots |
| `CombinePanel` | Wrap Box (or another panel) | The combine slots go in here |
| `CombineHint` | any | Shown while the combine slots are empty |
| `CombineButton`, `ClearCombineButton` | Button | Combine; empty the slots |
| `CraftCountText`, `CraftCountDownButton`, `CraftCountUpButton` | Text, Buttons | How many to make: "x3", − and + |
| `CraftingProgressBar`, `CraftingStatusText`, `CancelCraftingButton` | Progress Bar, Text, Button | While crafting: "Crafting Bandage (2 left)" and Stop |
| `StationText` | Text | "At the Workbench" or "No crafting station nearby" |
| `ResultText` | Text | What the last combine did |
| `EquipmentPanel` | ZEquipmentPanelWidget | The equipment slots |
| `CharacterPanel` | ZCharacterPanelWidget | Level, traits, tradeskills |
| `CloseButton`, `CloseButtonText` | Button, Text | "Close (I)" |

**ZItemSlotWidget** (one item): `Background` (Border, tinted for hovered/selected/drop target), `IconImage`, `NameText`, `CountText`, `DurabilityBar`, `EquippedMarker` (shown while it's equipped; if it's a Text, it says "Equipped" or "In hands"). The colors are in its Class Defaults → **Appearance**.

**ZEquipmentPanelWidget** (the equipment window): `ArmorText` ("Armor 33: you take 75% damage"). The slots are Item Slot widgets you place yourself, so you can lay them out like a body (head at the top, rings at the hands):
1. In your Widget Blueprint, drag in a **Z Item Slot Widget** (Palette → search "Z Item Slot") for each slot.
2. Select one → Details → **Equipment → Equipment Slot**: **Type** (e.g. `Equipment.Slot.Ring`) and **Index** (which one of that kind, from 0: the third ring is 2). **Empty Text** is what an empty slot says (its name if left empty).
3. Without any widgets of its own, the window builds every slot from **Project Settings → Game → Equipment** by itself, in groups.

**ZItemDetailsWidget:** `NameText`, `TypeText`, `IconImage`, `DescriptionText`, `StatsText`. What's in `StatsText` comes from the item's fragments, so a new kind of item describes itself.

**ZCharacterPanelWidget:** `LevelText`, `ExperienceBar`, `ExperienceText`, `TraitPointsText`, `TraitsPanel` (panel for the trait rows), `ResetTraitsButton` (hidden when resetting isn't allowed), `TradeskillsPanel` (panel for the tradeskill rows), `DiscoveriesText`.

**ZTraitRowWidget:** `NameText`, `RankText`, `EffectText`, `RaiseButton`. **ZTradeskillRowWidget:** `NameText`, `LevelText`, `LevelProgressBar`.

**For sounds and animations** each has Blueprint events: **On Selection Changed** and **On Crafting Result** (screen; **New Discovery** is true the first time a combine makes something), **On Item Changed** and **On State Changed** (slot), **On Trait Changed**, **On Skill Changed**.

**Icons:** an item's Data Asset has an **Icon**; slots and details show it once it's set (no item has one yet).

**From Blueprints** the screen can do everything the mouse does: **Select Item**, **Use Item**, **Equip Item**, **Unequip Item**, **Drop Item**, **Combine Items**, **Add To Combine**, **Combine Chosen Items**, **Set Craft Count**, **Close Screen**. The player controller has **Open Inventory**, **Close Inventory**, **Toggle Inventory** and **Is Inventory Open**.

## Sounds

UI sounds play only for you, "in your ears" (2D), so they need no attenuation.

1. **The ready-made ones:** `BP_ThirdPersonPlayerController` → **Class Defaults → UI → Sounds**:
   - **Inventory Open Sound / Inventory Close Sound**
   - **Crafted Sound:** a combine made something
   - **Discovery Sound:** a combine made something for the first time (plays instead of Crafted Sound)
   - **Craft Failed Sound:** a combine made nothing (stopping a craft is quiet)

   An empty setting plays nothing.
2. **Buttons** in your own Widget Blueprint: select the button → **Style → Pressed Sound / Hovered Sound**.
3. **Anything else**, e.g. a click when an item is selected: the Blueprint events above → **Play Sound 2D**.

Sounds in the world (shots, pickups, crafting at a station) are in AUDIO.md.

## HUD

`WBP_HUD` (in `Content/UI`) is filled in by C++ (`UZSurvivalHUDWidget`). The code finds the widgets by name, so the names must match exactly:

| Widget name | Type | Needed? | What the code does with it |
|---|---|---|---|
| `HealthBar` | Progress Bar | Yes | Health |
| `StaminaBar` | Progress Bar | Yes | Stamina |
| `AmmoText` | Text | Optional | "magazine / reserve", e.g. "12 / 60", while a gun is in the hands, hidden otherwise (also with a melee weapon) |
| `Crosshair` | any (e.g. Image) | Optional | Shown while a gun is in the hands |
| `HitMarker` | any (e.g. Image) | Optional | Flashes when a shot or a melee hit dealt damage (longer for a kill) |
| `StatusText` | Text | Optional | "You died. Respawning..." while dead |
| `InteractionText` | Text | Optional | "[E] Pick up Rifle Ammo x15" while looking at something to interact with |
| `InteractionProgress` | Progress Bar | Optional | Fills while holding the interact key on an interaction that takes time |
| `MessageText` | Text | Optional | The last few messages to the player, e.g. "Crafted Bandage x1." (if the HUD has none, one is added above the bottom middle of the screen) |

## Console commands

Open the console with the key under Esc while playing.

| Command | What it does |
|---|---|
| `ZInventoryUI` | Opens or closes the inventory screen, like its key |
| `ZInventoryUI select ID_Rifle` | Works the open screen like the mouse and writes what it shows to the Output Log (for automated tests). Also `use`, `drop`, `add`, `remove`, `clear`, `count 3`, `combine`, `drag ID_BrokenRadio ID_Screwdriver`, `equip ID_Jewelry_IronRing Ring 3` (dropping it on the third ring slot), `unequip ID_Jewelry_IronRing`, `raise Vigor`, `reset`, `show` |

## Plan: what isn't built yet

- **Item icons** (the Icon field is there).
- **The UI sounds themselves** (the settings are there) **and small animations** (the Blueprint events are there).
- **Dropping part of a stack** (Drop drops the whole stack; **Drop Item** already takes a count).
- **Comparing** a weapon with the one in the hands.
- **Storage chests:** the same item slots for the chest's inventory next to the player's.
- **Changing keys in a settings menu** (Enhanced Input's user settings).
