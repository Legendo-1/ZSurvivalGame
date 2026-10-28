# **Action-First Zombie Survival Game Architecture & Roadmap**

## **1\. World Structure: The "Hub & Spoke" Model**

To maintain rapid iteration speed and clean performance without procedural generation, structure the world around a single static Hub with branching hand-crafted zones:

               ┌───────────────────────────┐  
               │    THE HUB (Safehouse)    │  
               │    Crafting & Upgrades    │  
               └─────────────┬─────────────┘  
                             │ (Fast Travel / Gate)  
        ┌────────────────────┼────────────────────┐  
        ▼                    ▼                    ▼  
┌──────────────┐     ┌────────────────┐     ┌───────────────┐  
│ Zone 1: City │     │ Zone 2: Sub    │     │ Zone 3: High  │  
│  Districts   │     │ Base / Tunnels │     │ Security Zone │  
└──────┬───────┘     └────────────────┘     └───────────────┘  
       │ (Linear Progression w/ Loop-Back Shortcuts)  
       ▼  
┌──────────────┐  
│  Boss Arena  │ ──► Unique Part Drop ──► Return to Hub  
└──────────────┘

* **The Hub (Safehouse):** A single static level (e.g., a high-rise, subway depot, or police precinct) where players craft gear, cook food, process raw materials and upgrade their weapons. It's a safe place between expeditions, not a base to defend.  
* **Hand-Crafted Zones:** Linear-to-branching static maps focusing heavily on loop-back shortcuts (e.g., kicking down a ladder, unlocking a one-way security door) for world connectivity without massive open-world terrain overhead.
* **Not in the game:** base building and base defense (sieges, barricades, noise/heat, turrets). Decided 2026-10-28.  

## **2\. Core Mechanics Breakdown**

### **A. The Farming Loop (Monster Parts vs. Natural Scavenging)**

Split the item economy into two distinct buckets to balance survival gathering with active combat loops:

| Resource Bucket | Source / Nodes | Primary Use & Mechanics |
| :---- | :---- | :---- |
| **Environmental Resources** | Trees, Scrap Metal, Water, Urban Plants | Used for basic ammo, cooking and basic armor, gathered with durability tools (Axes, Crowbars). |
| **Common Zombie Drops** | Standard & Swarm Zombies | Rotting Meat, Bone Fragments, Cloth, and Low-Grade Scrap dropped via automatic proximity/inventory looting to keep combat pacing fast. |
| **Elite & Boss Parts** | Mutated Specials & Area Bosses | Unique Weapon Mods, High-Tier Firearms, and Heavy Mutant Carapace Armor harvested using locational destruction (e.g., break an elite's armored shoulder to drop Hardened Plate; sever a spitter's gland to drop a Corrosive Mod Core). |

### **B. Combat & Camera Perspective**

* **Third-Person Shooter (TPS) with Over-the-Shoulder Aiming:** Remnant-style movement setup. Free rotation when moving normally, strafing and zooming when aiming down sights.  
* **Stamina & Dodge System:** Ties dodge-rolling to stamina, with heavier crafted armor reducing dodge speed across distinct roll weight tiers (Light, Medium, Heavy).

### **C. Weapon & Mod Attachment Architecture**

Built around a modular C++ system rather than separate class files per gun:

* **Base Weapon Class:** Handles base stats (Damage, Fire Rate, Spread, Reload Speed).  
* **Mod Socket Component:** Provides open slots on standard firearms.  
* **Crafted Mods:** Unique parts harvested from Elite Zombies (e.g., Electric Pulse Gland, Acid-Spitter Sac) crafted into Active Mod Attachments at the Hub. Attaching a mod alters secondary fire modes or damage types (e.g., Acid Core on an AK-47 produces corrosive splash projectiles).

## **3\. Development Roadmap & Technical Status**

### **Phase 1: Core Combat & Movement (IN PROGRESS)**

* \[x\] TPS Character Controller with stamina-based sprinting and directional dodge-rolling.  
* \[x\] Enhanced Input bindings (IA\_Sprint, IA\_Dodge, IA\_Aim).  
* \[x\] Remnant-style aiming system (IA\_Aim via Right Mouse Button):  
  * Normal: Free movement character orientation.  
  * Aiming: Camera-facing orient with camera zoom and speed slowdown (300 walk speed, sprint blocked).  
  * Modular parameters in Blueprint (Aim Walk Speed, Aim Arm Length, Aim Zoom Speed).  
  * Unified rotation control via UpdateRotationMode().  
* \[x\] Basic AI Zombie melee chase logic and attack range detection (AI base class for zombies, monsters and human NPCs, enemy spawner).  
* \[x\] Multiplayer (listen server, up to 4 players) with the Gameplay Ability System: sprint, aim, dodge, fire and reload as abilities; health and stamina on the PlayerState.  
* \[x\] Hitscan shooting and reloading with pistol and rifle weapon data assets, headshots, teams (no friendly fire).  
* \[x\] Death as a ragdoll; players respawn after a few seconds.  
* \[x\] Rifle aim strafe animations (2D BlendSpace for walking sideways/backwards when aiming) and aim offset.  
* \[x\] Sound and effect events (GameplayCues) for shots, impacts, reloads, taking out a weapon, hits, deaths, level-ups, pickups, used items and crafting, set per weapon, character, item and recipe; UI sounds; a take-out animation per weapon (see AUDIO.md).  
* \[x\] Footsteps that match the surface (10 surfaces from Physical Materials) and the gait (walk, jog, sprint), with start, stop, jump and landing sounds, for players and enemies (see AUDIO.md).  
* \[x\] Fire, reload and take-out animations on the upper body (the legs keep walking), and sounds for the shots (the pistol has its own), taking out the weapons and reloading (see ANIMATION.md and AUDIO.md).
* \[x\] Souls-like melee combat: melee weapons (a machete for now) with light and heavy combos from a shared moveset, hitboxes that follow the blade during the swing, poise and staggers for players and enemies, invulnerable dodge frames, and locked-in actions with an input buffer for attacks, dodges, shots, reloads and weapon switches (see COMBAT.md).  
* \[ \] **Open Phase 1 Items:**  
  * Pistol aim strafe animations and aim offset.  
  * Weapon meshes, and the remaining sounds and effects (impacts, melee hits, staggers, deaths, pickups, crafting).
  * One-handed melee animations without a shield (attacks and walking use a sword and shield pack for now, and most walking directions are missing), and a dodge roll animation.

### **Phase 2: Item & Mod Data Architecture (IN PROGRESS)**

* \[x\] Item data assets in C++ with fragments (weapon, consumable), an inventory on the PlayerState that only the server changes, reserve ammo, pickups, the interact key and zombie loot (see ITEMS.md).  
* \[x\] Crafting by combining items (hidden recipes, Anarchy Online style) with tools of different quality that wear out, crafting stations, and tradeskills that are a hard requirement and level up by crafting (see CRAFTING.md).  
* \[x\] Character progression, Remnant style: levels from kills give trait points (also from bosses and hidden Tomes of Knowledge), spent on traits that change stats (health, stamina, damage, recoil, reload, dodge, crafting) and can be reset (see PROGRESSION.md).  
* \[x\] Weapon mod slots and upgrades: crafted mods (Barrel, Sight, Magazine) that change a weapon's stats, installed and taken off with a screwdriver; upgrade levels that each need more Weaponsmithing (see CRAFTING.md).  
* \[x\] Inventory screen: items, combining by dragging or with combine slots, the character panel with traits and tradeskills; opened with I/Tab or at a crafting station (see UI.md).  
* \[x\] Equipment, Remnant style: one short gun, one long gun and one melee weapon (keys 1, 2, 3), head, torso and leg armor, a necklace and four rings, in an equipment window next to the inventory; armor lowers damage, and armor and jewelry change stats like traits; saved with the character (see ITEMS.md).  
* \[x\] Saving, Remnant style: each player's character (level, traits, items with their stats and mods, tradeskills, recipes found) on their own PC, taken into any game and checked by the host; the world's progress in the host's save; save slots ready for a menu; crash-safe files with a backup (see SAVING.md).  
* \[ \] Mods that change fire modes and damage types (e.g. an Acid Core from an elite), and mods you can see on the weapon.  
* \[ \] More melee weapons (knives, axes, two-handed weapons), armor you can see on the character, and armor weight (light, medium and heavy dodges).

### **Phase 3: Loot, Harvest, & Crafting Loops**

* Interactable environmental harvesting nodes (trees, scrap metal cars).  
* Elite/Boss part drop tables triggered on death or dismemberment.  
* Hub crafting UI and recipe execution pipelines.

### **Phase 4: Level Assembly**

* Construct 1 complete gray-box zone with a loop-back shortcut to checkpoint.  

## **MVP Deliverable Target**

1 Hand-Crafted Level \+ 1 Hub \+ 2 Gun Types \+ 1 Boss Zombie dropping a unique Mod Part.