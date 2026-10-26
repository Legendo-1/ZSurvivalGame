# Items & inventory

How items, the inventory, pickups, loot and interacting work, and how to add new ones.

**Last updated:** 2026-10-26 (items, inventory, pickups, loot, interacting and reserve ammo added)

## The pieces

| Piece | What it is | Where |
|---|---|---|
| **Item definition** (`UZItemDefinition`) | One Data Asset per item: name, description, icon, tags, stack size, auto pickup, look on the ground, and its **fragments** | `Content/Items`, named `ID_...` |
| **Fragment** (`UZItemFragment`) | A feature of an item. **Weapon** makes it a weapon, **Consumable** makes it usable (heals) | Inside the item definition |
| **Item instance** (`UZItemInstance`) | One item in an inventory (or one stack). Holds what's special about it as stats, e.g. a weapon's magazine ammo (`Item.Stat.MagazineAmmo`) | Made by the server |
| **Inventory** (`UZInventoryComponent`) | The player's items as stacks. On the **PlayerState**, so it survives dying | On the PlayerState |
| **Pickup** (`AZPickup`) | Items lying in the world: loot, dropped items, or placed in a level | Spawned, or dragged into a level |
| **Loot table** (`UZLootTable`) | What an enemy can drop, each entry with a chance | `Content/Items/LootTables`, named `LT_...` |
| **Interaction** (`IZInteractable`, `UZInteractionComponent`) | The interact key: anything with the interface can be used with it | Interface on things, component on the character |

## The items there are

| Item | Type | Stack | Picked up by |
|---|---|---|---|
| `ID_Pistol`, `ID_Rifle` | Weapon (Weapon fragment → DA_Pistol / DA_Rifle) | 1 | Interact key |
| `ID_Ammo_Pistol`, `ID_Ammo_Rifle` | Ammo | 999 | Walking over it |
| `ID_Cloth`, `ID_Scrap`, `ID_BoneFragment`, `ID_RottenMeat` | Crafting material | 99 | Walking over it |
| `ID_Bandage` | Consumable, heals 25 | 10 | Interact key |

- **Starting items:** pistol, rifle, 36 pistol ammo, 90 rifle ammo, 2 bandages. Change them in BP_ThirdPersonCharacter → Class Defaults → **Survival|Inventory → Starting Items**. A player gets them once; after dying they keep their inventory.
- **Zombie loot** (`LT_Zombie`): rifle ammo 6–15 (50%), pistol ammo 4–10 (40%), cloth 1–2 (40%), scrap 1 (30%), bone fragments 1–2 (30%), rotten meat 1 (25%). The loot disappears after 2 minutes.
- On the ground, items are placeholder shapes for now: boxes for weapons and ammo, balls for materials, a small cylinder for bandages. Set **Pickup Mesh** in an item to change it.

## Weapons and ammo

- **Weapons are items.** Your weapon keys (1/2/3) call `Set Weapon Type`, which takes the first weapon of that type out of the inventory. The server then spawns the weapon in the hands, and removes it again when it's put away.
- **Each weapon item keeps its magazine** when it's put away or dropped, and a new weapon starts full.
- **Reloading takes ammo from the inventory.** The weapon data's **Ammo Item** says which (DA_Rifle → `ID_Ammo_Rifle`). Leave Ammo Item empty for unlimited reserve ammo.
- **The HUD's AmmoText** shows "magazine / reserve", e.g. `24 / 60`.

## Interacting

- **Look at something** (crosshair) or stand next to it, and press **E** (gamepad: **Y**). The HUD's `InteractionText` shows what it does, e.g. `[E] Pick up Rifle Ammo x15`.
- **Some interactions take time:** the key is held, and the HUD's `InteractionProgress` fills. Nothing uses it yet; reviving and harvesting will.
- **The server checks** that the player is close enough and nothing is in between, so no grabbing through walls.

**A new interactable** (e.g. a door Blueprint):
1. In its Blueprint: **Class Settings → Interfaces → Add → ZInteractable**.
2. Implement **Get Interaction Text** and **Interact** (and **Can Interact** / **Get Interaction Duration** if needed). Interact runs on the server.
3. Give one of its collisions **Collision Presets → Custom**, with **Interaction** set to **Block**. Players can only find things that block the Interaction trace channel.

## Using and dropping items

- The inventory has **UseItem** and **DropItem** (and the character has **EquipItem**) for an inventory screen later. For now there are console commands.
- **Using** a bandage heals 25 and uses one up. At full health nothing happens and the bandage is kept.
- **Dropped items** land in front of the player and are picked up with the interact key, not by walking over them. A dropped weapon keeps its magazine.

## Multiplayer

- **Only the server changes inventories.** Using, dropping, equipping and interacting are requests from the player's machine that the server checks.
- **Each player's inventory goes only to that player**, so others can't see what you carry. The item instances and their stats come along.
- **Pickups are sent to everyone**, so all players see the same items on the ground.

## Console commands

Open the console with the key under Esc while playing. Commands marked *host* only work in the host's (Server) window.

| Command | What it does |
|---|---|
| `ZShowInventory` | Writes your inventory to the Output Log (any window: shows that player's copy) |
| `ZGiveItem ID_Ammo_Rifle 30` | Gives you items (*host*) |
| `ZDropItem ID_Ammo_Rifle 10` | Drops some of your items in front of you |
| `ZUseItem ID_Bandage` | Uses one of your items |
| `ZSpawnPickup ID_Cloth 3 200` | Spawns a pickup like enemy loot, 200 cm in front of you; 0 = at your feet (*host*) |
| `ZHurt 50` | Takes 50 health, to try a bandage; `ZHurt 0` shows your health (*host*) |

A wrong item name lists all the items there are. Numbers can be left out in the editor: then the count is 1 and the distance 200 (e.g. `ZDropItem ID_Pistol`).

## How to add things

- **A new item:** in `Content/Items/<type folder>`, right-click → **Miscellaneous → Data Asset → ZItemDefinition**. Name it `ID_...` and fill in Display Name, Item Tags, Max Stack Size and Auto Pickup. Add its features under **Fragments → +**.
- **A new weapon:**
  1. Make a weapon Data Asset (**ZWeaponData**) with its stats and **Ammo Item**.
  2. Make an item with a **Weapon** fragment pointing to it.
  3. Add the item to **Starting Items** or a loot table.
- **A new kind of item feature** (armor, durability, mods, ...): a new fragment class in C++ (a child of `UZItemFragment`). Existing items don't change.
- **A new loot table:** Data Asset **ZLootTable** in `Content/Items/LootTables`, then set it on an enemy: Class Defaults → **AI|Loot → Loot Table**.
- **Items placed in a level:** drag a **Z Pickup** into the level and set its **Item** (item and count) in Details.

## Plan: what isn't built yet

- **Inventory screen:** it can use `GetItems`, the `OnInventoryChanged` event, `UseItem`, `DropItem` and `EquipItem`.
- **Carry limits:** `MaxSlots` on the inventory exists (0 = no limit for now); weight could come later.
- **Crafting:** recipes as data assets that use material items, crafted at stations (interactables) in the hub.
- **Harvesting:** trees and scrap as interactables that take time and give items.
- **Storage chests:** an inventory component on a chest, sent to the players using it.
- **Durability and weapon mods:** stats and fragments on the item instances.
- **Saving:** inventories saved by item ID (the Asset Manager already knows every item).
- **What dying costs:** for now you keep everything.
