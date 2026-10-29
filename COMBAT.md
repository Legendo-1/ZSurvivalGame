# Combat

How melee combat works (light and heavy combos, hitboxes, poise and staggers), the souls-like rules for when you can act (being locked into an action, and the input buffer), dodging, lock-on, and how to make melee weapons and attack animations. Guns are in ITEMS.md, sounds and effects in AUDIO.md.

**Last updated:** 2026-10-29 (lock-on)

## Controls

| Button | With a melee weapon | With a gun |
|---|---|---|
| **Left mouse** | Light attack. Press again during the swing for the next attack of the combo | Fire (hold for automatic guns) |
| **Right mouse** | Heavy attack | Aim (hold) |
| **Dodge** | Dodge | Dodge |
| **Middle mouse** (gamepad: click the right stick) | Lock on to the enemy nearest the middle of the screen, or let go | The same |
| **3** | Take out (or put away) your melee weapon | |

A melee weapon goes in the **Melee** equipment slot (ITEMS.md → Equipment). New characters start with a **Machete** (`ID_Machete`) there.

## Locked in: when you can act

Like in Dark Souls, once you start something you're locked into it.

- **Attacks, dodges and shots are actions.** While one runs you're **busy**: you can't start another attack, dodge, reload, switch weapons or jump. For a shot it's only a moment: its **Shot Recovery Time** (0.15 s, never longer than the time between two shots).
- **Pressing a button while busy isn't wasted.** It waits in the **input buffer** and happens as soon as it's allowed. If that takes longer than 0.4 seconds, it's forgotten. Only the newest press waits: pressing dodge, then attack, keeps the attack.
- **Windows** are the moments an action may be cut short. They're marked in the attack animations (see "Making an attack animation" below):
  - **Combo Window:** the next attack of the combo may start here. An attack pressed earlier in the swing waits for it.
  - **Dodge Window:** a dodge may start here (usually the recovery at the end of an attack). A dodge pressed earlier waits for it.
  - Without a window, the next action waits until the attack is over.
- **A stagger blocks everything** until it's over. Presses made during it wait in the buffer, so you can dodge right out of a stagger.
- **Reloading** waits until you're free, but it isn't an action itself: a dodge still cuts a reload short.
- **No walking during an attack.** The attack's animation moves you (root motion).
- **Facing:** during the start of an attack (its windup) you turn quickly toward where the camera looks, so you can still aim it. After that you keep the direction, even if the camera moves, until the attack's **Dodge Window** opens; from then on you turn with the camera again. An attack without a Dodge Window keeps its direction to the end.
  - The windup is the attack's **Turn Window** marker. Without one, it's the first 0.2 seconds of the attack (**Attack Turn Time**).
  - How fast you turn then: **Attack Turn Rate** (900 degrees per second: half a turn in 0.2 s).

## Combos

- **Light combo:** light attack 1 → 2 → 3, then it starts again at 1. **Heavy combo:** the same with the heavy attacks.
- **Press during the attack** (before or inside its Combo Window): the next attack of the combo follows.
- **Press after the Combo Window has closed:** the press waits until the attack is over, then the combo starts again from the first attack.
- **Switching between light and heavy:** a heavy attack pressed in a light attack's Combo Window plays the first heavy attack (and the other way around). An attack can pick a different one: **Next Light Attack** / **Next Heavy Attack** in the moveset (below).
- **Stamina:** each attack costs stamina (12 for the machete's light attacks, 18 for the third, 25 for the heavy one). Any stamina left lets an attack start, even if it takes the last of it. With none left, nothing happens. Stamina comes back 1 second after you last used it (attacks, dodges, sprinting): **Stamina Regen Delay** on BP_ThirdPersonCharacter → Class Defaults → **Survival|Stats**.

## Hitboxes: what an attack hits

- An attack only hits during its **Weapon Hitbox** notify in the animation: the frames of the swing.
- During those frames, the weapon's **blade** sweeps along: the line between the sockets **BladeStart** and **BladeEnd** on the weapon's mesh, as thick as **Blade Radius**. A weapon without those sockets hits with a ball around the right hand instead (**Fallback Hitbox Socket**, **Fallback Hitbox Radius**).
- **Each enemy is hit once per hitbox.** An animation with two Weapon Hitbox notifies (e.g. a double slash) can hit the same enemy twice.
- **It hits bodies, not the invisible capsule around them**, so the hit knows the bone (e.g. the head). Several enemies in the swing are all hit. Walls don't stop a swing.
- **Kicks and punches:** a Weapon Hitbox notify can also hit with other bones, e.g. `foot_r` for a kick: select the notify → Details → **Extra Sockets**. Untick **Use Weapon** for a kick without the weapon.
- **To see the hitboxes:** console `Z.Melee.DrawDebug 1` draws them green and the hits red (on your own screen).

## Poise and staggers

- **Every character has poise:** how much it takes before it staggers. Players 40, zombies 30.
- **Each hit takes some poise** (its poise damage). At 0 the character **staggers**: it reels for a moment and can't do anything, and whatever it was doing (an attack, a reload) is cut short. Its poise is full again right after.
- **Poise comes back** all at once after 3 seconds without poise damage. So steady pressure staggers, single hits now and then don't.
- **The machete's poise damage** is 15 per hit, times the attack's **Poise Damage Multiplier** (heavy attack ×2.5 = 37.5). Two light hits stagger a zombie, one heavy hit does.
- **Bullets** have **Poise Damage** 0, so guns don't stagger. Set it on a gun's data asset to change that (e.g. a shotgun).
- **Zombies wear down your poise too** (15 per hit): three hits in a row stagger you.
- **More poise:** Max Poise is a stat like the others, so traits and armor can raise it (PROGRESSION.md, ITEMS.md → Wearable).
- **The stagger animation** is the character's **Stagger Montage**: BP_ThirdPersonCharacter → Class Defaults → **Combat**, a zombie's under **AI|Stagger**. Without one, the character stands still for 0.6 seconds. A player's stagger animation must stay in place (root motion off); an enemy's may move it. Its sound and effect: the character's **Stagger Cue** (AUDIO.md).
- **A boss that never staggers:** clear **Abilities → Stagger Ability** in its Class Defaults (or give it a high Max Poise).

## Dodging

- A dodge dashes you in the direction you're moving (forward when standing still) and costs 25 stamina.
- **Invulnerable frames:** for the first 0.2 seconds, hits pass through you (no damage, no poise damage). That's **Invulnerable Time** on the Dodge ability.
- Mid-attack you can only dodge in the attack's **Dodge Window**; mid-shot right after the shot.
- **A dodge animation** (a roll) can be set as **Dodge Montage** on the Dodge ability. It must stay in place: the dodge moves you itself.

## Lock-on

- **Middle mouse** (gamepad: click the right stick) locks on to the enemy nearest the middle of the screen, if:
  - it's at most **2.5 m** from your character (**Lock On Distance**),
  - it's within **20°** of the middle of the screen (**Lock On Angle**),
  - and no wall hides it.
- A **white dot** sits on the target while you're locked on. Press the key again to let go.
- **While locked on:**
  - Your character faces the target. **A** and **D** circle around it, **S** steps away, **W** goes toward it.
  - The camera follows the target.
  - Attacks turn toward the target during their windup (instead of toward the camera).
  - With a gun, firing without aiming goes at the target (the middle of the screen is on it). **Aiming** (right mouse with a gun) lets go, so you aim yourself (**Release Lock On When Aiming**).
- **Switching to another enemy:** turn the camera toward it (mouse or right stick). Once it's near the middle of the screen and in range, the lock moves to it.
  - Small movements don't switch: you have to turn at least **10°** away from the current target (**Switch Target Angle**).
  - When you stop turning, the camera drifts back onto the target.
- **The lock lets go by itself when:**
  - you turn the camera more than **35°** away from the target (**Break Lock Angle**) and there's no other enemy where you turned to,
  - the target gets farther than **6 m** from you (**Lose Lock Distance**),
  - walls hide the target for more than **1 second** (**Lose Sight Time**). Other characters in the way don't count,
  - the target dies. The lock then jumps to another enemy near the middle of the screen, if there's one (**Switch Target On Kill**),
  - you die.
- **2.5 m is short:** Dark Souls locks on from about 15–20 m. If lock-on feels like it rarely finds anything, raise **Lock On Distance** (and **Lose Lock Distance** with it).
- **Other players** can't be locked on to.

**Lock-on points on enemies:**
- Every enemy has one on its chest: the **LockOnTarget** component, 40 cm above the middle of its capsule. The camera aims at it and the dot sits on it.
- **For a taller or shorter enemy:** open its Blueprint → **Components** → select **LockOnTarget** → move it up or down in the viewport.
- **For a big enemy (a boss), more points:** **Components → Add → Lock On Target**. To make it follow a body part, select it → **Details → Sockets → Parent Socket** → pick a bone (e.g. the head). Each point is its own target, so you can switch between them like between enemies.
- **A point that can't be targeted:** untick **Can Be Locked On** on it. During play, the host can switch it with the Blueprint node **Set Can Be Locked On** (e.g. a boss that flies away).

## Settings

**Edit → Project Settings → Game → Combat:**

| Setting | What it does | Now |
|---|---|---|
| **Input Buffer Time** | How long a press made while busy waits for its turn | 0.4 s |
| **Poise Recovery Delay** | Seconds without poise damage before poise is full again | 3 s |
| **Default Stagger Duration** | How long a stagger lasts without a Stagger Montage | 0.6 s |
| **Melee Reach Tolerance** | Extra distance the host accepts for a client's melee hit (others see you a little late) | 250 cm |
| **Combo Timing Tolerance** | How late the host still accepts the next attack of a client's combo | 1 s |
| **Attack Turn Rate** | How fast an attack turns toward the camera during its windup | 900 degrees per second |
| **Attack Turn Time** | For attacks without a Turn Window: how long from the start they turn | 0.2 s |
| **Lock On Distance** | How far from your character lock-on finds enemies | 250 cm |
| **Lock On Angle** | How far from the middle of the screen lock-on finds enemies | 20° |
| **Lock On Distance Weight** | When two enemies are near the middle, how much the farther one counts against it (higher prefers the closer one) | 2 |
| **Lose Lock Distance** | The lock lets go when the target gets farther than this | 600 cm |
| **Switch Target Angle** | How far you turn the camera from the target before the lock can move to another enemy | 10° |
| **Break Lock Angle** | Turning the camera this far from the target, with no other enemy there, lets go | 35° |
| **Lose Sight Time** | The lock lets go when walls hide the target this long | 1 s |
| **Lock On Camera Speed** | How tightly the camera follows the target | 10 |
| **Lock On Camera Return Speed** | How fast the camera drifts back onto the target after you turned it | 3 |
| **Switch Target On Kill** | When the target dies, lock on to the next enemy near the middle of the screen | on |
| **Release Lock On When Aiming** | Aiming a gun lets go of the lock | on |

## Making a melee weapon

A melee weapon is three assets: a **moveset** (its attacks), its **weapon data** (stats and looks) and the **item** you carry. Weapons of the same kind (all one-handed swords, say) can share one moveset.

1. **The moveset:** in `Content/Weapons/Movesets`, right-click → **Miscellaneous → Data Asset** → pick **ZMeleeMoveset** → name it `MS_...` (e.g. `MS_TwoHanded`). Open it:
   - **Light Attacks → +** for each attack of the light combo, in order. For each one:
     - **Montage:** its animation (see the next section).
     - **Section:** leave empty, unless one montage holds several attacks: then the name of this attack's section.
     - **Damage Multiplier** and **Poise Damage Multiplier:** the weapon's damage and poise damage times this (1 = as is, 2 = double).
     - **Stamina Cost:** what it costs.
     - **Next Light Attack / Next Heavy Attack:** leave at 0 for the usual order. A number picks which attack follows in this attack's Combo Window (1 = the first of that list).
   - **Heavy Attacks → +** the same for the heavy combo.
   - **Save.**
2. **The weapon data:** in `Content/Weapons`, right-click → **Miscellaneous → Data Asset** → **ZMeleeWeaponData** → name it `DA_...` (e.g. `DA_Axe`). Open it:
   - **Weapon:** Display Name. **Weapon Type:** Melee One Handed or Melee Two Handed (which walking animations; see ANIMATION.md). **Equipment Slot** is Melee already.
   - **Melee:** **Moveset** (step 1), **Damage** and **Poise Damage** per hit, **Stamina Cost Multiplier** (1.5 = every attack costs 50% more, for a heavy weapon), **Attack Speed** (1.2 = attacks play 20% faster).
   - **Looks:** **Static Mesh** or **Skeletal Mesh**, **Attach Socket** (the bone or socket of the character's hand that holds it) and **Attach Offset** to line it up with the hand. The placeholder attacks don't move the `weapon_r` bone, so with them use `hand_r` (as the machete does). Weapon animations from a pack usually move `weapon_r`.
   - **Hitbox:** the names of the blade sockets on the weapon's mesh (**Blade Start Socket**, **Blade End Socket**, see below), **Blade Radius** (the blade's thickness), and **Max Reach**: the farthest from you a hit can be (the host ignores hits farther away).
   - **Effects → Impact Cue:** the sound and effect where a hit lands (AUDIO.md).
   - **Upgrades:** like a gun's (upgrades raise its damage, CRAFTING.md).
3. **The item:** in `Content/Items/Weapons`, right-click → **Miscellaneous → Data Asset** → **ZItemDefinition** → `ID_...`. **Display Name**, **Item Tags**: `Item.Type.Weapon`, **Max Stack Size** 1, **Fragments → +** → **Weapon** → its **Weapon Data**: your `DA_...`.
4. **Check it:** right-click the moveset and the weapon data → **Asset Actions → Validate Assets**. It tells you about a missing montage, an attack without a hitbox, a wrong section name, a missing moveset.
5. **Try it:** `ZGiveItem ID_...` (or add it to Starting Items), equip it, press **3**.

## The blade sockets

The hitbox follows the blade between two sockets on the weapon's mesh.

1. Open the weapon's **Static Mesh** (double-click it).
2. **Window → Socket Manager** → **Create Socket** → name it `BladeStart`. In **Details**, move it to where the blade starts (just above the guard or the hand). The gizmo in the viewport moves it too.
3. **Create Socket** again → `BladeEnd`, at the tip.
4. **Save.**

For a skeletal mesh: open it → **Skeleton Tree** → right-click the bone → **Add Socket** → rename it `BladeStart` (and the same for `BladeEnd`).

A hammer or a club: BladeStart and BladeEnd along its head. Other names work too: set them in the weapon data's **Hitbox** settings.

## Making an attack animation

An attack is an **Anim Montage** with three kinds of markers (notify states) in it.

1. **The montage:** right-click the attack animation → **Create → Create AnimMontage**. Name it `AM_...` and put it next to the animation (the machete's are in `Characters/Mannequins/Anims/Melee`).
2. **Moving with the attack:** an attack may step forward. In the *animation's* **Asset Details → Root Motion**, tick **Enable Root Motion**. The character then moves as the animation does (and doesn't walk during the attack). Only the *moving* is used: if the animation turns the character, that turn is ignored and the character keeps facing where the camera looks. So the animation's **root** bone should only move, not turn. Animations retargeted from Mixamo often put the hips' turning into the root bone; then the whole swing looks stiffer in the game than in the animation preview (see NOTES.md → Melee).
3. **The slot:** attacks are whole-body, so they keep the slot `DefaultGroup.DefaultSlot` (the montage panel, top left). Not UpperBody.
4. **The markers:** open the montage. In the **Notifies** track at the bottom, right-click → **Add Notify State...** and pick:
   - **Weapon Hitbox:** over the frames where the swing should hit. Drag its two ends to the start and the end of the swing.
   - **Combo Window:** where the next attack may start. Usually from the end of the hitbox to near the end of the animation.
   - **Dodge Window:** where a dodge may cut the attack short. Usually the recovery after the swing.
   - **Turn Window** (optional): where the attack still turns toward the camera. Usually from the start to just before the swing. Without one, the attack turns during its first 0.2 seconds. After it the attack keeps its direction until the Dodge Window.
   Each marker can get its own track: right-click the track name **Notifies → Add Notify Track** (the machete's montages use the tracks **Hitbox** and **Windows**).
5. **Save**, and add the montage to a moveset.

**Tips:**
- The last 0.25 seconds of a montage are its **blend out**. The attack is over when it starts, so markers in that part don't matter.
- A Combo Window that starts before the hitbox ends lets the next attack cut the swing off before it hits.
- **Hit reactions and staggers** are plain montages without markers. A player's stagger must stay in place (no root motion).

## Sounds and effects

| Event | Tag | Where it's set |
|---|---|---|
| A melee hit lands | `GameplayCue.Weapon.MeleeImpact` | The weapon data's **Impact Cue** |
| A character staggers | `GameplayCue.Character.Stagger` | The character's **Stagger Cue** (Class Defaults → **Effects → Character Cues**) |

The target's own **Hit Cue** plays with every hit too. How to make the sounds and effects: AUDIO.md.

## Multiplayer

- **Your attacks start right away on your own machine** and on the host a moment later. Your machine finds what you hit and tells the host, which checks it (close enough, not hit too often by this attack) and decides the damage and poise damage.
- **The host decides staggers.** Your machine learns about a stagger a moment after it happened.
- **The host checks every combo:** an attack only counts if it can follow the attack before. Timing is left to your machine, which sees the windows exactly.
- Everyone sees your attacks, dodges and staggers (the animations come from the host).
- **Lock-on runs on your own machine.** Your character faces the target through the camera direction your machine sends with every move anyway, so the host turns and moves you the same way (no rubber-banding while circling an enemy). The host and the other players are told which enemy you're locked on to (for animations later).

## Console commands

| Command | What it does |
|---|---|
| `ZSpawnDummy 120` | A zombie that stands still (no AI), 120 cm in front of you, to practice on; `ZSpawnDummy 120 all` puts one in front of every player (*host*) |
| `ZStagger` | Staggers you; `ZStagger players` every player, `ZStagger enemies` every enemy (*host*) |
| `ZCombat` | Writes your combat state to the Output Log: the attack, what waits in the buffer, busy or staggered, open windows, lock-on (target, distance, how far you face away from it), poise and stamina |
| `ZWeaponStats` | The stats of the weapon in your hands (for a melee weapon: damage, poise damage, attack speed, stamina cost, reach) |
| `ZSpawnDummyAt 220 -55` | Like `ZSpawnDummy`, but 55° to the left of straight ahead (positive: to the right), to try switching targets (*host*) |
| `ZLockOn` | Like the lock-on key, and writes what you're locked on to |
| `ZTurn -20` | Turns your camera 20° to the left (positive: to the right; a second number turns it up), like moving the mouse |
| `Z.Melee.DrawDebug 1` | Draws the hitboxes (green) and the hits (red) |

## The placeholder content

So you can try everything before real weapon animations exist:

- `AM_Melee_Light_1`, `_2`, `_3` and `AM_Melee_Heavy_1` (`Characters/Mannequins/Anims/Melee`): the mannequin's unarmed punches and kicks as attacks, with hitboxes and windows (their hitboxes also hit with the left hand and the right foot). Kept as examples: `MS_OneHanded` now uses three light and three heavy attacks from the sword and shield pack (`Characters/Mannequins/Anims/SwordAndShield`), until better one-handed animations without a shield are found.
- `AM_Stagger_Front` (`Characters/Mannequins/Anims/Stagger`): a hit reaction, as the stagger for players and zombies.
- `MS_OneHanded` (`Content/Weapons/Movesets`): the machete's moveset.
- `DA_Machete` and `ID_Machete`: the machete, damage 25 and poise damage 15 per hit. Its mesh `SM_PlaceholderBlade` (`Content/Weapons/Meshes`) is a stretched box with the two blade sockets, held in the right hand.

To replace them, follow "Making a melee weapon" and "Making an attack animation" above: a real mesh with blade sockets, and real attack montages in the moveset.

## Plan: what isn't built yet

- **Hyper armor:** a window in heavy attacks where a stagger doesn't interrupt them (a new marker and a check in the stagger).
- **Charged heavy attacks:** hold the right mouse button to charge.
- **Blocking, parrying, backstabs.**
- **Enemies with melee weapons:** AI attacks still use a simple sweep in front of them; the hitbox attacks can be used by AI later (they already work on the server).
- **A poise bar** on the HUD, and **armor weight** making dodges slower.
