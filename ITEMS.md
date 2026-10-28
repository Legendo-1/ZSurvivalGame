# Items & inventory

How items, the inventory, equipment, pickups, loot and interacting work, and how to add new ones. Crafting has its own guide: CRAFTING.md. The inventory screen: UI.md.

**Last updated:** 2026-10-28 (the machete, the first melee weapon; gun and melee weapon data)

## The pieces

| Piece | What it is | Where |
|---|---|---|
| **Item definition** (`UZItemDefinition`) | One Data Asset per item: name, description, icon, tags, stack size, auto pickup, look on the ground, and its **fragments** | `Content/Items`, named `ID_...` |
| **Fragment** (`UZItemFragment`) | A feature of an item. **Weapon** makes it a weapon, **Wearable** makes it armor or jewelry, **Weapon Mod** makes it a mod that's installed on weapons, **Consumable** makes it usable (heals), **Tool** makes it a tool for recipes (with a quality), **Durability** makes it wear out, **Grants Progression** gives trait points or experience when used | Inside the item definition |
| **Item instance** (`UZItemInstance`) | One item in an inventory (or one stack). Holds what's special about it: stats, e.g. a weapon's magazine ammo (`Item.Stat.MagazineAmmo`), upgrade level (`Item.Stat.UpgradeLevel`) or a tool's durability (`Item.Stat.Durability`), and the mods installed on it | Made by the server |
| **Inventory** (`UZInventoryComponent`) | The player's items as stacks. On the **PlayerState**, so it survives dying | On the PlayerState |
| **Equipment** (`UZEquipmentComponent`) | Which of the player's items are equipped, and in which slot. Equipped items stay in the inventory | On the PlayerState |
| **Pickup** (`AZPickup`) | Items lying in the world: loot, dropped items, or placed in a level | Spawned, or dragged into a level |
| **Loot table** (`UZLootTable`) | What an enemy can drop, each entry with a chance | `Content/Items/LootTables`, named `LT_...` |
| **Interaction** (`IZInteractable`, `UZInteractionComponent`) | The interact key: anything with the interface can be used with it | Interface on things, component on the character |

## The items there are

| Item | Type | Stack | Picked up by |
|---|---|---|---|
| `ID_Pistol`, `ID_Rifle` | Weapon (Weapon fragment → DA_Pistol / DA_Rifle) | 1 | Interact key |
| `ID_Machete` | Melee weapon (Weapon fragment → DA_Machete; COMBAT.md) | 1 | Interact key |
| `ID_Ammo_Pistol`, `ID_Ammo_Rifle` | Ammo | 999 | Walking over it |
| `ID_Cloth`, `ID_Scrap`, `ID_BoneFragment`, `ID_RottenMeat` | Crafting material | 99 | Walking over it |
| `ID_Bandage` | Consumable, heals 25 | 10 | Interact key |
| `ID_Medkit` | Consumable, heals 60 | 5 | Interact key |
| `ID_TomeOfKnowledge` | Gives a trait point when read (PROGRESSION.md) | 10 | Interact key |
| `ID_Screwdriver`, `ID_PrecisionScrewdriver` | Tool (screwdriver, quality 1 / 2, durability 20 / 40) | 1 | Interact key |
| `ID_AmmoPress` | Tool (ammo press, quality 1, durability 60) | 1 | Interact key |
| `ID_DuctTape`, `ID_ElectronicParts`, `ID_Gunpowder`, `ID_CircuitBoard`, `ID_WeaponParts` | Crafting material | 20–99 | Walking over it |
| `ID_BrokenRadio` | Salvage (something to take apart) | 5 | Interact key |
| `ID_Mod_Compensator`, `ID_Mod_LongBarrel`, `ID_Mod_ReflexSight`, `ID_Mod_ExtendedMag` | Weapon mod (`Content/Items/Mods`) | 5 | Interact key |
| `ID_Armor_ScrapHelmet`, `ID_Armor_LeatherJacket`, `ID_Armor_PaddedTrousers` | Armor (`Content/Items/Armor`): head Armor +8, torso +15, legs +10 | 1 | Interact key |
| `ID_Jewelry_BoneNecklace` | Necklace (`Content/Items/Jewelry`): Max Health +15 | 1 | Interact key |
| `ID_Jewelry_IronRing`, `ID_Jewelry_RunnersRing`, `ID_Jewelry_SteadyRing` | Rings: Armor +5, Stamina Regen +15%, Recoil -10% | 1 | Interact key |

- **Starting items:** pistol, rifle, machete, 36 pistol ammo, 90 rifle ammo, 2 bandages. Change them in BP_ThirdPersonCharacter → Class Defaults → **Survival|Inventory → Starting Items**. A player gets them once; after dying they keep their inventory. Starting weapons and clothes are equipped right away, as long as their slot is free.
- **The armor and jewelry** are examples to test with: nothing drops them yet. Give them with `ZGiveItem` (below), or add them to a loot table or the starting items.
- **Zombie loot** (`LT_Zombie`): rifle ammo 6–15 (50%), pistol ammo 4–10 (40%), cloth 1–2 (40%), scrap 1 (30%), bone fragments 1–2 (30%), rotten meat 1 (25%). The loot disappears after 2 minutes.
- On the ground, items are placeholder shapes for now (boxes, balls and cylinders). Set **Pickup Mesh** in an item to change it.
- What the crafting items are for is in CRAFTING.md.

## Equipment

A player has these slots (like Remnant 2):

| Group | Slots | Key |
|---|---|---|
| Weapons | **Short gun** (pistols, submachine guns, sawn-offs), **Long gun** (rifles, long shotguns, sniper rifles), **Melee** | 1, 2, 3 |
| Armor | **Head**, **Torso**, **Legs** | |
| Jewelry | **Neck**, **Ring** ×4 | |

- **One item per slot.** So you can't carry two long guns into a fight: the second one replaces the first (the first stays in your inventory).
- **Equipped items stay in the inventory.** The item list marks them **Equipped** (or **In hands** for the weapon you hold). Dropping an equipped item, or using it up in a craft, takes it off first.
- **Putting things on:** double-click or right-click an item, or select it and press **Equip**, and it goes in a free slot of its kind (if they're all taken: in place of the first one). Or drag it onto the slot you want, e.g. the third ring slot. Dragging a ring from one ring slot onto another swaps the two.
- **Taking things off:** double-click or right-click it in the equipment window, drag it out of the window, or select it and press **Unequip**. It stays in your inventory.
- **Weapons:** equipping a weapon also takes it out. **1** takes out your short gun, **2** your long gun, **3** your melee weapon. Pressing the key of the weapon you're holding puts it away. A key for an empty slot does nothing. Taking off the weapon you're holding puts it away.
- **Armor and jewelry change your stats** while you wear them, like traits do. Putting on something with more max health doesn't heal you: heal the gap yourself. Taking it off only costs health if you have more than the new max: **Armor** lowers the damage you take, and pieces can also add Max Health, Max Stamina, Stamina Regen, Recoil, Reload Speed, Damage, and so on. The equipment window shows your armor and how much damage gets through.
- **How armor works:** damage × 100 / (100 + Armor). 50 armor: you take 67%. 100 armor: 50%. 300 armor: 25%. Each point helps a little less than the one before, and you never become unhittable. The 100 is **Armor For Half Damage** in Project Settings → Game → Equipment. Traits can raise Armor too.
- **The slots are settings:** **Edit → Project Settings → Game → Equipment → Slot Types**. Each line is one kind of slot: its tag, how many (e.g. 4 rings), its name and its group in the equipment window.
- **Melee weapons** go in the Melee slot, e.g. the machete. With one in your hands, the mouse buttons attack instead of firing and aiming (COMBAT.md).

## Weapons and ammo

- **Weapons are items.** Taking one out (its key, or equipping it) makes the server spawn the weapon in the hands; putting it away removes it again. Only equipped weapons can be in the hands.
- **Each weapon item keeps its magazine, upgrade level and mods** when it's put away or dropped, and a new weapon starts full.
- **Reloading takes ammo from the inventory.** The weapon data's **Ammo Item** says which (DA_Rifle → `ID_Ammo_Rifle`). Leave Ammo Item empty for unlimited reserve ammo.
- **The HUD's AmmoText** shows "magazine / reserve", e.g. `24 / 60`. With a melee weapon in your hands the HUD hides the ammo and the crosshair.
- **Gun or melee:** a gun's data asset is a **ZRangedWeaponData** (DA_Pistol, DA_Rifle), a melee weapon's a **ZMeleeWeaponData** (DA_Machete). What they share (name, slot, looks, upgrades) is the same in both. Gun data made before the melee weapons existed was called ZWeaponData; it loads as ZRangedWeaponData by itself.
- **Upgrades and mods** change the weapon's stats (CRAFTING.md → Weapon upgrades and mods). `UZWeaponStatsLibrary` works out a weapon item's stats, also for an inventory screen, e.g. **Get Weapon Stat** (Damage) for an item that isn't in the hands. `ZWeaponStats` writes the stats of the weapon in your hands to the Output Log.

## Interacting

- **Look at something** (crosshair) or stand next to it, and press **E** (gamepad: **Y**). The HUD's `InteractionText` shows what it does, e.g. `[E] Pick up Rifle Ammo x15`.
- **Some interactions take time:** the key is held, and the HUD's `InteractionProgress` fills. Nothing uses it yet; reviving and harvesting will.
- **The server checks** that the player is close enough and nothing is in between, so no grabbing through walls.

**A new interactable** (e.g. a door Blueprint):
1. In its Blueprint: **Class Settings → Interfaces → Add → ZInteractable**.
2. Implement **Get Interaction Text** and **Interact** (and **Can Interact** / **Get Interaction Duration** if needed). Interact runs on the server.
3. Give one of its collisions **Collision Presets → Custom**, with **Interaction** set to **Block**. Players can only find things that block the Interaction trace channel.

## Using and dropping items

- **The inventory screen** (I or Tab, see UI.md): right-click an item to use it, drag it out of the window to drop it. It calls the inventory's **UseItem** and **DropItem**, which Blueprints can use too. There are also console commands.
- **Using** a bandage heals 25 and uses one up. At full health nothing happens and the bandage is kept.
- **Using** a weapon item equips it and takes it out. **Using** armor or jewelry puts it on. Using something that's equipped takes it off.
- **Dropped items** land in front of the player and are picked up with the interact key, not by walking over them. A dropped weapon keeps its magazine, upgrades and mods.

## Multiplayer

- **Only the server changes inventories and equipment.** Using, dropping, equipping and interacting are requests from the player's machine that the server checks (e.g. that the item is yours and goes in that slot).
- **Each player's inventory and equipment go only to that player**, so others can't see what you carry. The item instances and their stats come along. Everyone sees the weapon in your hands.
- **The stats from equipment** are worked out by the server, like traits, and reach your machine with your other stats.
- **Pickups are sent to everyone**, so all players see the same items on the ground.

## Console commands

Open the console with the key under Esc while playing. Commands marked *host* only work in the host's (Server) window.

| Command | What it does |
|---|---|
| `ZShowInventory` | Writes your inventory to the Output Log (any window: shows that player's copy) |
| `ZGiveItem ID_Ammo_Rifle 30` | Gives you items; `ZGiveItem ID_Ammo_Rifle 30 all` gives every player some (*host*) |
| `ZDropItem ID_Ammo_Rifle 10` | Drops some of your items in front of you |
| `ZUseItem ID_Bandage` | Uses one of your items |
| `ZSpawnPickup ID_Cloth 3 200` | Spawns a pickup like enemy loot, 200 cm in front of you; 0 = at your feet (*host*) |
| `ZHurt 50` | Takes 50 health, to try a bandage; `ZHurt 0` shows your health (*host*) |
| `ZEquip ID_Jewelry_IronRing` | Equips one of your items in a free slot of its kind; `ZEquip ID_Jewelry_IronRing 3` puts it in the third ring slot |
| `ZUnequip ID_Rifle` | Takes off one of your equipped items |
| `ZEquipment` | Writes what you have equipped, what's in your hands and your armor to the Output Log |

A wrong item name lists all the items there are. Numbers can be left out in the editor: then the count is 1 and the distance 200 (e.g. `ZDropItem ID_Pistol`).

## How to add things

- **A new item:** in `Content/Items/<type folder>`, right-click → **Miscellaneous → Data Asset → ZItemDefinition**. Name it `ID_...` and fill in Display Name, Item Tags, Max Stack Size and Auto Pickup. Add its features under **Fragments → +**.
- **A new gun:**
  1. In `Content/Weapons`, right-click → **Miscellaneous → Data Asset** → **ZRangedWeaponData**, with its stats and **Ammo Item**. Set **Equipment Slot**: Short Gun or Long Gun (a new one starts as Long Gun). Its animations are under **Looks**, its sounds and effects under **Effects** (AUDIO.md).
  2. Make an item with a **Weapon** fragment pointing to it.
  3. Add the item to **Starting Items** or a loot table.
- **A new melee weapon:** COMBAT.md → Making a melee weapon.
- **New armor or jewelry:**
  1. In `Content/Items/Armor` or `Content/Items/Jewelry`: right-click → **Miscellaneous → Data Asset → ZItemDefinition** → name it `ID_Armor_...` or `ID_Jewelry_...`.
  2. **Display Name**, **Description**, **Item Tags**: `Item.Type.Armor` or `Item.Type.Jewelry`. **Max Stack Size**: 1.
  3. **Fragments → +** → **Wearable**. Open it: **Slot** (e.g. `Equipment.Slot.Torso`), and under **Modifiers** one line per stat: **Attribute** (e.g. Armor, MaxHealth, RecoilMultiplier) and **Value** (e.g. 10, or -0.1 for -10% on a multiplier).
  4. Optional: **Effects → Equip Cue**, its own sound for putting it on (AUDIO.md).
  5. **Save**. Right-click it → **Asset Actions → Validate Assets** checks it.
- **A new kind of slot** (e.g. hands for gloves): **Project Settings → Gameplay Tags** → add `Equipment.Slot.Hands`. Then **Project Settings → Game → Equipment → Slot Types → +**: that tag, a count, a name and a group. The equipment window shows it by itself.
- **Sounds and effects for an item:** its Data Asset → **Effects → Pickup Cue** (picked up) and **Use Cue** (used up, e.g. a bandage). They start as `GameplayCue.Item.Pickup` and `GameplayCue.Item.Use`, which every item shares. Pick a child tag to give an item its own, or clear it for none. How to make the sounds: AUDIO.md.
- **A new tool or weapon mod:** see CRAFTING.md → How to add things.
- **A new kind of item feature** (a backpack, ...): a new fragment class in C++ (a child of `UZItemFragment`). Existing items don't change. Its `GetDescriptionLines` is what the inventory screen shows about it (e.g. "Heals 25").
- **A new loot table:** Data Asset **ZLootTable** in `Content/Items/LootTables`, then set it on an enemy: Class Defaults → **AI|Loot → Loot Table**.
- **Items placed in a level:** drag a **Z Pickup** into the level and set its **Item** (item and count) in Details.

## Plan: what isn't built yet

- **Carry limits:** `MaxSlots` on the inventory exists (0 = no limit for now); weight could come later.
- **Harvesting:** trees and scrap as interactables that take time and give items.
- **Storage chests:** an inventory component on a chest, sent to the players using it.
- **Weapon durability:** a Durability fragment on weapons, worn down by shooting (tools already have durability, and weapons already have upgrades and mods).
- **What dying costs:** for now you keep everything.
- **Armor you can see:** worn armor shown on the character (it's only stats for now).
- **Armor weight:** heavy armor making dodges slower (DESIGN.md: light, medium and heavy rolls).

The inventory is saved with the character, every item with its stats and mods (SAVING.md). Items are saved by their asset name, so renaming one needs a redirect (SAVING.md → Renaming or removing things saves use). A new fragment with its own stats should keep them within its limits when an item loads from a save: its `OnInstanceLoaded`.
