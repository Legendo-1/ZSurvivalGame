# Editor tasks

**Last updated:** 2026-10-29 (task 11: the golem is 1.1 times the pack's size now (new blend space speeds), and its attacks' hitboxes to tune)

A temporary guide for the work you do in the editor. It stays out of git and gets deleted when everything here is done. AUDIO.md and ANIMATION.md explain the same systems for the long run.

## Where things stand

| # | Task | Status |
|---|---|---|
| 1 | Pistol sounds | Done |
| 2 | Fire, reload and take-out animations on the upper body | Done |
| 3 | Weapon meshes (the guns you see in the hands) | **To do**: needs a gun model |
| 4 | Impact effects (what shows where a bullet hits) | **To do**: needs an effect and sounds |
| 5 | Melee attack animations (real swings instead of punches) | **In progress**: sword and shield attacks are in; most still need their markers (5F) |
| 6 | The machete's mesh, with its blade sockets | **To do**: needs a machete model |
| 7 | Walking and standing with a melee weapon | **In progress**: idle and run are in; better one-handed animations later |
| 8 | Melee hit and stagger sounds | **To do**: needs sounds (the melee hit's notify is made, with its camera shake) |
| 9 | Lock-on, hit feedback and spacing: try them and tune | **To do** |
| 10 | A navmesh in the level, and trying the enemies' new AI | **To do**: the level's files have no navmesh yet (if you added one, it wasn't saved) |
| 11 | The Stone Golem: walking sideways, and trying it | **To do** |

The tasks don't depend on each other: do them in any order. Task 6 is easiest after task 5 (the attacks decide how the machete sits in the hand).

---

## Words used in this guide

- **Content Browser**: the file panel at the bottom of the editor (**Ctrl+Space** opens it). Paths like `Content/Weapons/DA_Rifle` mean: in the Content Browser, open the `Content` folder, then `Weapons`, then the asset `DA_Rifle`.
- **Details panel**: the settings list that shows when you open an asset or select something. Names joined by **→** (like **Looks → Skeletal Mesh**) are a section heading, then the setting inside it. Click the small arrow next to a heading to open it.
- **Data asset** (`DA_`): a file that only holds settings. `DA_Rifle` holds everything about the rifle: damage, magazine size, mesh, sounds. The C++ code reads it; you change it in the editor.
- **Skeletal Mesh** (`SK_`, `SKM_`): a 3D model with bones, so parts of it can move (a pistol's slide, a magazine). **Static Mesh** (`SM_`): a model in one solid piece. Both work for a gun here.
- **Socket**: a named point stuck to a bone or a mesh, like a pin. Code finds it by its name, for example "where is the tip of the barrel?".
- **Notify** (`GCN_`, short for GameplayCue Notify): a small Blueprint that says *what plays* for an event: the sounds, the effects, the bullet hole. The C++ code only sends the event ("a shot hit something here"), and every notify whose tag matches plays on every player's screen. Notifies must be in `Content/GameplayCues`, or the game doesn't find them.
- **Tag**: the event's name, like `GameplayCue.Weapon.Impact`. A notify plays when its **Gameplay Cue Tag** is exactly the event's tag.
- **Niagara System** (`NS_`): a particle effect (dust, sparks, a muzzle flash). Older packs sometimes have **Cascade** effects instead (named `P_`, type "Particle System"): those don't work in a notify, so pick packs with Niagara.
- **Attenuation** (`ATT_`): how a sound gets quieter with distance. Without one, every player hears a sound at full volume anywhere on the map.
- **Physical Material** (`PM_`): tells the game what a surface is made of (Metal, Wood, ...). The footsteps already use them, and the impact effects can use them too.
- **Fab**: Epic's store for models, sounds and effects. Many things are free.

## Working together

- **When I ask you to close the editor:** **File → Save All** (Ctrl+Shift+S), then close it. I build and test the C++ with it closed. I'll say when you can open it again.
- **If the editor asks to rebuild modules when it opens:** choose **No** and tell me in the chat. It means the C++ changed and I should build it.
- **Don't press the Live Coding button** (bottom right of the editor, or Ctrl+Alt+F11) while I'm changing C++. It could compile something I'm in the middle of.
- **When you finish a task:** Save All, and tell me. I put it into git on its own.

## Testing with two players

The **⋮** (three dots) next to the green Play button → **Number of Players** `2` → **Net Mode** → **Play As Listen Server** → click Play. Two windows open: the first is the host, the second is a client. Everything in tasks 3 and 4 shows in both windows without any extra work: check both anyway.

---

## 3. Weapon meshes

**Why:** you hold invisible guns right now, because `DA_Rifle` and `DA_Pistol` have no mesh set. Shooting works without one (shots go from the camera), but you can't see the gun, and the shot sound and muzzle flash start at the gun's position.

**How it works in the game:** when you take out a weapon, the game puts its mesh in the character's right hand, on the hand's `weapon_r` bone. The reload and take-out animations move that bone, so the gun moves with the hands.

### 3A. Get a gun model

**From Fab (free):**
1. Go to **fab.com** in your browser and sign in with your Epic account.
2. Search "rifle" (later "pistol"). On the left, filter **Price: Free** and **Format: Unreal Engine**.
3. Open one you like → **Add to My Library**.
4. Open the **Epic Games Launcher** → **Unreal Engine** (left side) → **Library** tab → scroll down to **Fab Library** → find the gun → **Add to Project** → pick **ZSurvivalGame**. If the project isn't in the list, tick **Show all projects** and pick it anyway.
5. In the editor, the pack appears as a new folder in the Content Browser. Open it and find the mesh: its icon shows the gun, and hovering over it shows "Skeletal Mesh" or "Static Mesh".

**From Lyra (free from Epic):** the **Lyra Starter Game** (in the launcher under **Samples**) has a rifle and pistol made for this same character skeleton and for the same `MM_` animations we use, so they fit the hands best. It's a big download (several GB). Create a Lyra project, open it, find its gun mesh in the Content Browser, right-click it → **Asset Actions → Migrate** → pick this project's `Content` folder. Migrate copies the gun with everything it needs (materials, textures).

### 3B. Mark the tip of the barrel (the `Muzzle` socket)

**Why:** the shot sound and the muzzle flash play at a socket named `Muzzle` on the gun. Without it they play at the gun's center (usually the grip).

**First check if the gun already has one:** open the mesh and look for a socket near the barrel's tip in the **Skeleton Tree** (skeletal mesh) or the **Socket Manager** (static mesh). Gun packs often have one, named for example `Muzzle` or `MuzzleFlash`. If it has a different name, keep it and type that name into the data asset's **Muzzle Socket** in step 3C.6.

**If it has none, add one:**
- **Skeletal Mesh:** double-click it to open it → **Skeleton Tree** panel on the left → right-click the bone closest to the barrel (or the top bone if you're not sure) → **Add Socket** → right-click the new socket → **Rename Socket** → `Muzzle`.
- **Static Mesh:** double-click it to open it → **Window → Socket Manager** → **Create Socket** → name it `Muzzle`.

Then move it: select the socket, press **W** in the viewport so arrows appear, and drag them until the socket sits at the very tip of the barrel. Only its position matters (the effects point in the direction you shoot by themselves). **Save**.

### 3C. Put the gun in the hand

**Why a socket for this too:** every gun model is made a little differently (where its grip is, which way it points). A socket on the character's hand bone holds the gun at the right spot, and you can see the gun while you move it. Each gun gets its own socket.

1. Open the character's skeleton: `Content/Characters/Mannequins/Meshes/SK_Mannequin`.
2. **Skeleton Tree** → type `weapon_r` in its search box → right-click **weapon_r** → **Add Socket** → right-click the new socket → **Rename Socket** → `Rifle_Socket` (for the pistol: `Pistol_Socket`).
3. Right-click the socket → **Add Preview Asset** → pick your gun mesh. The gun appears at the hand. It's only for viewing here: it doesn't put the gun in the game yet.
4. Give the character a holding pose, so you can see where the hands are: **Window → Preview Scene Settings** → **Animation** section → **Preview Controller**: `Use Specific Animation` → **Animation**: `MF_Rifle_Idle_ADS` (for the pistol: `MF_Pistol_Idle_ADS`).
5. Select the socket in the Skeleton Tree. In the viewport, **W** moves it and **E** rotates it. Move and rotate until the grip sits inside the right hand, with the barrel pointing forward. If the gun is far too big or small, the model has the wrong size: **R** scales the socket too. **Save**.
6. Open `Content/Weapons/DA_Rifle` → **Looks** section:
   - **Skeletal Mesh** *or* **Static Mesh** (whichever kind your gun is; leave the other empty) → your gun.
   - **Attach Socket** → `Rifle_Socket` (type it exactly, with the capital letters).
   - **Attach Offset**: leave it at 0. The socket does that job, and you can see it there.
   - **Muzzle Socket**: `Muzzle`, or the gun's own name for it from 3B.
   - **Save**.
7. The same for the pistol with `Pistol_Socket` and `DA_Pistol`.

**About the left hand:** it doesn't grab the gun by itself. The animations put it where Lyra's guns have their front grip. With a Lyra gun it fits; with another gun, place the gun so the right hand holds it well, and the left hand may sit a bit off. Making the left hand follow the gun (hand IK) can come later.

### 3D. Muzzle flash (optional)

If the gun pack, or an effects pack, has a muzzle flash (a Niagara System):
1. Open `Content/GameplayCues/GCN_Weapon_Fire` (the rifle's shot) → **GCN Effects → Burst Effects → Burst Particles** → **+** → open the new entry → **Niagara System**: the muzzle flash. **Compile**, **Save**.
2. The same in `GCN_Weapon_Fire_Pistol` for the pistol.

It plays at the `Muzzle` socket, pointing the way you shoot. It stays where it started and doesn't follow the gun if you move, which is fine for a flash this short.

### Check

Play with two players. Take out the rifle with **2** (the pistol with **1**). Then:
- The gun is in the hand, in both windows.
- Walk, aim, reload: the gun stays in the hand.
- Shoot: the sound (and the flash) comes from the barrel's tip.

**If something's off:**
- **The gun is in the hand but at the wrong angle or spot:** adjust the socket in `SK_Mannequin` (step 3C.5). The game reads it again the next time you press Play.
- **The gun sits in the middle of the hand, ignoring your socket:** the **Attach Socket** name in the data asset doesn't match the socket's name. The game then uses the `hand_r` bone instead.
- **No gun at all:** the mesh isn't set in the data asset, or you set it in the wrong one (**Skeletal Mesh** for a skeletal mesh, **Static Mesh** for a static mesh).

---

## 4. Impact effects

**Why:** when a bullet hits a wall, nothing shows right now. The game already sends the event `GameplayCue.Weapon.Impact` on every hit, to every player, with three things: **where** it hit, **which way the surface faces**, and **what the surface is made of** (its Physical Material: Concrete, Metal, Wood, ...). There's just no notify for it yet, so nothing plays. You make that notify here.

### 4A. What you need from Fab (filter Price: Free, Format: Unreal Engine)

- **An impact effect** (a Niagara System): search "bullet impact" or "impact vfx". A puff of dust is the most useful first one, since it fits every surface.
- **Impact sounds**: search "bullet impact sounds".
- **A bullet hole** (optional): a *decal material*. A decal is a picture projected onto a surface, like a sticker. Effect packs often include bullet-hole decals: look for a material whose name contains "Decal" or "Hole". Making your own:
  1. Right-click in the Content Browser → **Material** → name it `M_BulletHole` → open it.
  2. Click empty space in the graph. In **Details**: **Material Domain** `Deferred Decal`, **Blend Mode** `Translucent`.
  3. Drag a bullet-hole picture (a texture with a see-through background) into the graph. Connect its **RGB** pin to **Base Color** and its **A** pin (the see-through part) to **Opacity**. **Save**.

**Attenuation for the impact sounds**, so they fade with distance:
1. `Content/Audio` → right-click empty space → **Audio → Sound Attenuation** → name it `ATT_Impact`.
2. Open it → **Attenuation Distance**: **Inner Radius** `200` (full volume within 2 m), **Falloff Distance** `2800` (silent at about 30 m). **Save**.
3. Open each impact sound → click empty space in its graph (for a Sound Cue) or look in the Details (for a Sound Wave) → **Attenuation → Attenuation Settings** → `ATT_Impact` → **Save**. Many sounds at once: select them all in the Content Browser → right-click → **Asset Actions → Edit Selection in Property Matrix** → find **Attenuation Settings** and set it there.

### 4B. Make the notify

1. Go to `Content/GameplayCues` → right-click empty space → **Blueprint Class**.
2. A window opens. Click **All Classes** at the bottom to open the list → type `GameplayCueNotify_Burst` in the search → select it → **Select**.
3. Name the new Blueprint `GCN_Weapon_Impact` and double-click it to open it.
4. **Class Defaults** (button at the top, if the Details panel doesn't show the settings yet) → **Gameplay Cue Tag** → `GameplayCue.Weapon.Impact`.
5. **GCN Effects → Burst Effects:**
   - **Burst Particles** → **+** → open the new entry `[0]` → **Niagara System**: your impact effect.
   - **Burst Sounds** → **+** → open `[0]` → **Sound**: your impact sound.
   - **Burst Decal** (optional, for the bullet hole) → **Decal Material**: your bullet hole. **Decal Size**: `5, 5, 5`. These are half sizes in cm, so 5 makes a hole 10 cm across. Tick **Override Fade Out** → **Fade Out Start Delay** `10`, **Fade Out Duration** `2`: each hole stays 10 seconds and then fades out over 2 seconds, so they don't pile up.
6. **Compile** (top left) → **Save**.

**Check:** Play, shoot a wall and the floor. Dust (and a sound and a hole) appear where the bullet hit, in both windows.

**If it doesn't show:** see "When something doesn't show" at the bottom.

**If the dust sprays the wrong way** (along the wall instead of out of it): the effect is placed so its *front* points out of the surface. Most impact effects are made that way, but some are made to spray *upward*, and then they only look right on floors. Try another effect, or tell me which one and I'll look at it.

### 4C. Different effects per surface (when you have more effects)

**Why:** sparks on metal, splinters on wood, dust everywhere else. Every entry under **Burst Particles** and **Burst Sounds** can be limited to some surfaces. Two lists decide that, and an empty list means "no limit":
- **Allowed Surface Types**: the entry plays *only* on these surfaces.
- **Rejected Surface Types**: the entry plays everywhere *except* on these.

**Example: sparks on metal.**
1. In `GCN_Weapon_Impact` → **Burst Particles** → **+** → a new entry → **Niagara System**: sparks.
2. In that entry, tick the small box to the left of **Spawn Condition Override** (it turns the setting on) → open **Spawn Condition Override** → **Allowed Surface Types** → **+** → `Metal`. Now the sparks only play on metal.
3. The dust entry `[0]` → tick its **Spawn Condition Override** box → **Rejected Surface Types** → **+** → `Metal`. Now the dust plays everywhere except on metal, so metal doesn't get both.
4. Do the same for Wood (splinters), Glass, and so on: each new effect gets its surface under **Allowed**, and the dust gets it under **Rejected**.
5. **Compile**, **Save**.

Leave **Locally Controlled Source**, **Locally Controlled Policy** and **Chance To Play** inside the override as they are.

**Bullet holes:** **Burst Decal** is a single entry, not a list, so a notify has one bullet hole for all surfaces. Its **Spawn Condition Override** can still turn it off on some surfaces (see Flesh below).

**Which floors have a surface:** only floors and walls whose material has a Physical Material. Everything else counts as **Default** (no surface): it gets the entries that have no Allowed list, so the dust. AUDIO.md → Footsteps → "Making a floor sound like metal" shows how to give a floor or wall its surface.

### 4D. Blood on characters instead of dust (optional)

**Why:** bullets also hit players and zombies. Their bodies have no surface yet, so they'd get dust and bullet holes. Giving them a **Flesh** surface lets them have blood instead.

1. **Edit → Project Settings** → on the left, **Engine → Physics** → scroll to **Physical Surface**. There's a list with Concrete, Dirt, ... Wood. In the first empty line (**SurfaceType11**), type `Flesh`. It saves by itself.
2. `Content/Physics` → right-click empty space → **Physics → Physical Material** → name it `PM_Flesh` → open it → **Surface Type**: `Flesh` → **Save**.
3. Open `Content/Characters/Mannequins/Rigs/PA_Mannequin` (the bodies the game uses to hit characters). Click once in the viewport, then press **Ctrl+A** to select all bodies. In **Details**, find **Simple Collision Physical Material** → `PM_Flesh` → **Save**. This covers players and zombies, since both use this mannequin.
4. In `GCN_Weapon_Impact`:
   - A new **Burst Particles** entry with a blood effect → **Spawn Condition Override** → **Allowed Surface Types**: `Flesh`.
   - Add `Flesh` to the **Rejected Surface Types** of the dust, of the wall sounds, and of **Burst Decal** (tick its **Spawn Condition Override** first).
   - **Compile**, **Save**.

**Check:** shoot a zombie: blood, no dust, no bullet hole. Shoot a wall: dust and a hole as before.

**Pain sounds** don't go here. They belong to the character's **Hit Cue** (`GameplayCue.Character.Hit`), which only plays when the hit actually did damage (a teammate's bullet hits you but does no damage, so no pain sound).

---

## 5. Melee attack animations

**Why:** the machete's attacks are the mannequin's unarmed punches and kicks for now (`AM_Melee_Light_1` to `_3` and `AM_Melee_Heavy_1`). They already have their hitboxes and windows, so everything works, but they don't look like swinging a blade. COMBAT.md explains how attacks work.

### 5A. Get the animations

1. **fab.com** → search "sword animations" or "melee animations" → filter **Price: Free** and **Format: Unreal Engine** → **Add to My Library** → in the launcher: **Fab Library** → **Add to Project** (like task 3A).
2. **Pick a pack made for the UE5 mannequin** (Manny/Quinn): the description says "UE5 Mannequin" or the animations play on `SK_Mannequin`. Other skeletons need retargeting first (like the Mixamo animations, NOTES.md).
3. You want: **3 light attacks** that flow into each other (a combo), **1 or 2 heavy attacks**, and maybe **a roll** for the dodge.

### 5B. Make the montages

For each attack animation:
1. Right-click it → **Create → Create AnimMontage** → name it e.g. `AM_Machete_Light_1` → move it to `Content/Characters/Mannequins/Anims/Melee`.
2. **Does the attack step forward?** Open the *animation* (not the montage) → **Asset Details → Root Motion** → tick **Enable Root Motion** if the character moves forward in it. Then the attack moves the character in the game too.
3. Leave the montage's slot at `DefaultGroup.DefaultSlot` (the whole body).

### 5C. Add the markers

Open `AM_Melee_Light_1` first to see an example: the **Hitbox** track has a red **Weapon Hitbox**, the **Windows** track a **Combo Window** and a **Dodge Window**. Then in your montage:
1. In the **Notifies** area at the bottom, right-click the track name **1** → **Add Notify Track** twice. Right-click each new track's name → **Rename**: `Hitbox` and `Windows`.
2. Play the montage slowly (the speed buttons under the viewport) and find the frames where the blade swings through the target.
3. **Hitbox** track: right-click at the start of the swing → **Add Notify State...** → **Weapon Hitbox**. Drag its right end to the end of the swing.
4. **Windows** track: right-click → **Add Notify State...** → **Combo Window**, from the end of the swing to a little before the end of the animation. Another one: **Dodge Window**, over the recovery after the swing.
5. **Save**.

### 5D. Put them in the moveset

1. Open `Content/Weapons/Movesets/MS_OneHanded`.
2. **Light Attacks → [0] → Montage** → `AM_Machete_Light_1`; **[1]** → `_2`; **[2]** → `_3`. **Heavy Attacks → [0] → Montage** → your heavy attack. Adjust **Stamina Cost** and the multipliers if you like.
3. **Save**. Right-click it → **Asset Actions → Validate Assets**: no errors means every attack has a montage and a hitbox.

### 5E. Which bone holds the weapon

Weapon animations usually move a bone named `weapon_r` together with the right hand. The placeholder punches don't, so the machete is held by `hand_r` for now. To check your new animations: open one → **Skeleton Tree** → click `weapon_r` → play the animation: if `weapon_r` moves with the hand, use it in task 6.

**Check:** Play, press **3**, type `ZSpawnDummy 120`, click: the new swings play, and each hit writes "Melee hit by ..." to the Output Log (`log LogZSurvivalGame Verbose` first).

---

### 5F. Finish the markers on your attacks

When I checked `MS_OneHanded` on 2026-10-29:
- **Only Light Attack 1 and Heavy Attack 2 (the kick) have a Weapon Hitbox.** The others (Light 2 and 3, Heavy 1 and 3) never hit anything. They have no Combo Window or Dodge Window either, so the next attack only starts when they're over, and you can't dodge out of them.
- **Light Attacks has a 4th entry without a montage.** After Light Attack 3 the combo goes to it and nothing happens. Delete it: open `MS_OneHanded` → **Light Attacks** → the arrow next to entry **3** (they count from 0) → **Delete**.

For each attack montage without markers: open it and add **Weapon Hitbox**, **Combo Window** and **Dodge Window** (task 5C has the steps). Then right-click `MS_OneHanded` → **Asset Actions → Validate Assets**: it lists what's still missing.

## 6. The machete's mesh

**Why:** the machete is a stretched grey box (`SM_PlaceholderBlade`) for now. The hitbox follows the blade between two sockets on the mesh, so the real mesh needs them too.

### 6A. Get a model

**fab.com** → search "machete" (or any blade) → **Price: Free**, **Format: Unreal Engine** → add it to the project (like task 3A).

### 6B. The blade sockets

1. Open the mesh (double-click it).
2. **Static Mesh:** **Window → Socket Manager** → **Create Socket** → name it `BladeStart`. **Skeletal Mesh:** **Skeleton Tree** → right-click the blade's bone → **Add Socket** → **Rename Socket** → `BladeStart`.
3. Move it (**W** in the viewport) to where the blade starts, just above the handle.
4. Make a second socket, `BladeEnd`, at the tip. **Save**.

### 6C. Put it in the hand

Like the guns in task 3C, with a socket on the hand you can see the machete on:
1. Open `Content/Characters/Mannequins/Meshes/SK_Mannequin` → **Skeleton Tree** → search `hand_r` (or `weapon_r`, if your animations move it: 5E) → right-click it → **Add Socket** → **Rename Socket** → `Machete_Socket`.
2. Right-click the socket → **Add Preview Asset** → your machete.
3. **Window → Preview Scene Settings → Animation → Preview Controller**: `Use Specific Animation` → **Animation**: one of your attack animations. Pause it where the arm swings.
4. Select the socket: **W** moves, **E** rotates. Put the handle in the fist, blade out of the thumb side. **Save**.
5. Open `Content/Weapons/DA_Machete` → **Looks**:
   - **Static Mesh** (or **Skeletal Mesh**) → your machete; clear the other one.
   - **Attach Socket** → `Machete_Socket`.
   - **Attach Offset**: set it back to all zeros (Location 0, Rotation 0, Scale 1); it lined up the placeholder box, and the socket does that now.
   - **Save**.
6. `ID_Machete` → **Pickup Mesh** → your machete (how it looks on the ground), **Pickup Mesh Scale** `1, 1, 1`.

**Check:** Play, **3**, console `Z.Melee.DrawDebug 1`, swing: green balls run along the blade during the hit. If they're at the hand instead, the socket names in the mesh and in DA_Machete → **Hitbox** don't match.

---

## 7. Walking and standing with a melee weapon

**Why:** ABP_Manny picks the walking animations by weapon type (Unarmed, Pistol, Rifle). Melee One Handed and Melee Two Handed have no pins yet, so the machete uses the node's **Default Pose**.

1. Open `ABP_Manny` → **AnimGraph** → double-click **Locomotion** → double-click the **Idle** state.
2. Select the **Blend Poses (EZWeaponType)** node → right-click it → **Add pin for element** → **Melee One Handed**. Again for **Melee Two Handed** if you have two-handed animations.
3. Connect an idle animation to the new pin (a melee idle from your pack, or the unarmed idle for now).
4. The same in the **Walk/Run** state, with a walking blend space (your pack's, or the unarmed one).
5. **Compile**, **Save**.

**Check:** Play, **3**: standing and walking use the new animations.

---

## 8. Melee hit and stagger sounds

**Why:** the game already sends the events, nothing plays yet. AUDIO.md → "Making a sound for an event" has the steps; these are the events:
- `GameplayCue.Weapon.MeleeImpact`: a melee hit lands (a slash or a thud, maybe blood). `GCN_Weapon_MeleeImpact` (in `Content/GameplayCues`) exists already with a small camera shake: open it → **GCN Effects → Burst Effects** → **Burst Sounds → +** for the sound and **Burst Particles → +** for blood. Leave **Burst Camera Shake** as it is. Raw Magnitude is the damage, Physical Material the body's surface (task 4D's Flesh).
- `GameplayCue.Character.Stagger`: a character staggers (a grunt). Make `GCN_Character_Stagger`.
- **The swing's whoosh:** a **Play Sound** notify in each attack montage where the swing starts (AUDIO.md → "Other sounds in animations").

**Check:** console `AbilitySystem.DisplayGameplayCues 1`, hit the dummy: the tags show, and the sounds play.

---

## 9. Lock-on, hit feedback and spacing: try them and tune

**Why:** both are built and tested without a screen; how they feel is up to you. COMBAT.md → "Lock-on" and "Hit feedback" explain everything.

**Lock-on:**
1. Play, `ZSpawnDummy 200` and `ZSpawnDummyAt 220 -55` in the console.
2. Press the **middle mouse button**: a white dot on the zombie in front, the camera follows it.
3. Move the mouse slowly to the left: the dot moves to the other zombie. Turn far away: it lets go.
4. To change how it feels: **Edit → Project Settings → Game → Combat → Lock-On**. Most likely you'll want a longer **Lock On Distance** than 2.5 m (Dark Souls: about 15–20 m; raise **Lose Lock Distance** with it), and maybe a different **Break Lock Angle** (how far you turn before it lets go).
5. **Your own marker (optional):** open `WBP_HUD` → drag an **Image** into the Canvas Panel → rename it `LockOnMarker` → in **Details**: **Alignment** 0.5, 0.5, **Size** e.g. 16 x 16, and your picture in **Brush → Image**. Compile, Save. It replaces the white dot.

**Hit feedback:**
1. **Hit-stop per attack:** open `MS_OneHanded` → each attack → **Hit Stop Time**. All are 0.08 s now; try 0.12 for the heavy attacks. **Hit Stop Anim Rate** (Project Settings → Combat → **Hit Feedback**) is how slow the swing gets (0 = frozen).
2. **The flash:** **Hit Flash Color** and **Hit Flash Time** in the same place. To turn the flash off, clear **Hit Flash Material**.
3. **The camera shake:** open `Content/Effects/CS_MeleeHit` → **Root Shake Pattern** → **Pitch** / **Yaw** → **Amplitude** (bigger = stronger), **Duration** (how long).

**Check:** hit the dummy with the machete: the swing pauses for a blink, the zombie flashes white, your camera shakes a little.

**Spacing:**
1. `ZSpawnDummy 300`, walk straight into the zombie: you stop about a step away from it.
2. Attack: every attack of your combo should hit from there. If one still swings past, make the gap bigger: BP_ThirdPersonCharacter → **Components** → **Spacing** → **Extra Radius** (45 now; try 55). If it feels too far, make it smaller.
3. For each enemy type later: its Blueprint → **Spacing** → **Extra Radius** (bigger enemies, bigger gap).

---

## 10. A navmesh in the level, and trying the enemies' new AI

**Why:** enemies now find their way with the navmesh, but `Lvl_ThirdPerson` has none, so they walk straight at you and get stuck behind walls or other players. AI.md explains the new AI.

**The navmesh:**
1. Open `Lvl_ThirdPerson`.
2. **Window → Place Actors** (if the panel isn't open) → **Volumes** → drag a **Nav Mesh Bounds Volume** into the level.
3. In **Details → Brush Settings**, make it big enough to cover everywhere enemies should walk (X and Y about the size of the level, Z a bit higher than the highest floor).
4. Press **P** in the viewport: the walkable floor turns green. Press **P** again to hide it.
5. **Save** the level.

**Try the AI:**
1. Play, `ZSpawnZombies 1 700`: the zombie walks to you and attacks. Walk behind a wall: it walks around it (with the navmesh).
2. `ZSpawnEnemyAt 900 180 1`: a zombie behind you that looks away. It stands still until you get close or hit it.
3. `ZAIStrafe 1`, then let a zombie attack you: between attacks it circles you. `ZAIStrafe 0` turns it off.
4. `Z.AI.Debug 1`: every enemy shows its state over its head.

**Check:** `ZAIStates` lists each enemy with its state and target.

---

## 11. The Stone Golem: walking sideways, and trying it

**Why:** the golem is ready to fight (AI.md → The Stone Golem), but its animation Blueprint is the pack's demo one, which only walks and runs forward. When the golem circles you, it slides sideways. The pack has the golem's own walks in all four directions; a blend space with them makes it walk sideways while it faces you.

**Try it first:** Play, open the console, type `ZSpawnEnemyAt 600 0 0 BP_Enemy_StoneGolem`. It walks up, swings its hammer, and circles you between swings. Machete it about 6 times quickly: it staggers. `Z.AI.Debug 1` shows what it's doing.

### 11A. Your own copy of its animation Blueprint

So an update of the pack can't overwrite your changes.

1. Content Browser → `StoneGolem/Demo` → right-click `ABP_StoneGolem` → **Duplicate** → name it `ABP_Enemy_StoneGolem`.
2. Drag it onto the `Enemies/StoneGolem` folder → **Move Here**.
3. Open `BP_Enemy_StoneGolem` → **Components** panel → select **Mesh (CharacterMesh0)** → **Details** → **Anim Class** → `ABP_Enemy_StoneGolem`. **Compile**, **Save**.

### 11A2. Turn off its foot IK

The golem's own animations don't move the foot IK bones, so the foot IK keeps its legs stiff and parallel in attacks (and would in the new walks). In `ABP_Enemy_StoneGolem` → **AnimGraph** → the **Control Rig** node (`CR_Mannequin_BasicFootIK`) → **Details** → **Alpha** `0`. **Compile**, **Save**.

### 11B. The blend space

1. In `Enemies/StoneGolem`: right-click → **Animation** → **Blend Space** → pick `SKEL_StoneGolem_Skeleton` → name it `BS_StoneGolem_Locomotion`.
2. Open it. In **Asset Details → Axis Settings**:
   - **Horizontal Axis**: Name `Direction`, Minimum Axis Value `-180`, Maximum `180`.
   - **Vertical Axis**: Name `Speed`, Minimum `0`, Maximum `440`.
3. Drag these animations from the **Asset Browser** (bottom right) onto the grid. Drop them anywhere, then click each point and type the exact values in its details:

   | Animation | Direction | Speed |
   |---|---|---|
   | `AS_StoneGolem_idle1` | five times: -180, -90, 0, 90, 180 | 0 |
   | `AS_StoneGolem_walkF` | 0 | 150 |
   | `AS_StoneGolem_walkR` | 90 | 150 |
   | `AS_StoneGolem_walkL` | -90 | 150 |
   | `AS_StoneGolem_walkB` | twice: -180 and 180 | 125 |
   | `AS_StoneGolem_jogF` | 0 | 440 |
   | `AS_StoneGolem_jogR` | 90 | 440 |
   | `AS_StoneGolem_jogL` | -90 | 440 |
   | `AS_StoneGolem_jogB` | twice: -180 and 180 | 440 |

   The speeds are for our golem's size: it's 1.1 times bigger than the animations were made for, so its walk covers 150 cm a second.
4. Hold **Ctrl** and move the mouse over the grid to preview. **Save**.

### 11C. Use it in the animation Blueprint

1. Open `ABP_Enemy_StoneGolem` → **My Blueprint** panel → **Variables** → **+** → name it `Direction`, type **Float**.
2. **Event Graph**: find where **GroundSpeed** is set (it comes from the character's velocity). Right-click → **Calculate Direction**:
   - **Velocity**: the same velocity GroundSpeed comes from.
   - **Base Rotation**: from the character reference the graph already uses (e.g. **Character**) → **Get Actor Rotation**.
   - Drag **Direction** into the graph → **Set**, connect the result, and connect its white execution pin right after the GroundSpeed **Set**.
3. **AnimGraph** → double-click the state machine → double-click the walking state → select the **Blend Space Player** (it shows `BS_MM_WalkRun`) → **Details** → **Blend Space** → `BS_StoneGolem_Locomotion`. Its inputs are now **Direction** and **Speed**: drag the **Direction** variable in (Get) and connect it to Direction, and **GroundSpeed** to Speed.
4. **Compile**, **Save**.

**Check:** spawn it again and let it swing once: when it circles you, it walks sideways while facing you, and walks forward when it comes at you.

### 11D. Tune it (optional)

In `BP_Enemy_StoneGolem` → **Class Defaults**:
- **Too slow or too fast:** Character Movement → **Max Walk Speed** (240 now; 150 is its plain walk, 440 its jog). Circling: AI|Combat → Combat Settings → **Strafe Speed** (150 matches its sideways walk).
- **Its attacks:** AI|Attacks → **Attacks**: range, damage, cooldown and how often each is picked (**Weight**). AI.md → Attacks and The Stone Golem explain them. E.g. **Weight** 0 on an attack you don't like.
- **When a swing hits:** open `AM_StoneGolem_Attack1` (to 5) in `Enemies/StoneGolem` → the red **Weapon Hitbox** bar on the **Hitbox** track is when the hammer can hit. Drag its ends to the frames where the hammer swings through, **Save**. To see it while playing: console `Z.Melee.DrawDebug 1` (green: where the hammer swept, red: a hit).
- **The rest:** Survival|Stats, AI and AI|Combat → Combat Settings; AI.md has a table of what each does.
- **Footsteps:** its walks have no Footstep notifies yet: AUDIO.md → Footsteps.
- **Loot:** AI|Loot → **Loot Table** (it drops nothing yet).

---

## Tasks 1 and 2 (done)

What you made, in case you do the same for a new weapon later:

- **Task 1, pistol sounds:** `GCN_Weapon_Fire_Pistol` (the pistol's shot), `GCN_Weapon_Equip_Pistol` and `GCN_Weapon_Equip_Rifle` (taking each weapon out), `ATT_Gunshot` and `ATT_WeaponHandling` (how far shots and handling sounds carry). A new weapon gets its own shot sound the same way: duplicate `GCN_Weapon_Fire`, set its tag to `GameplayCue.Weapon.Fire.<Weapon>`, swap the sound. AUDIO.md has the details.
- **Task 2, animations on the upper body:** ABP_Manny plays montages in the **UpperBody** slot, so the legs keep walking. The montages `AM_Rifle_Fire/Reload/Equip` and `AM_Pistol_Fire/Reload/Equip` are set in the data assets (**Looks → Fire/Reload/Equip Montage**), and the reload montages have their sounds as **Play Sound** notifies. A new weapon needs three montages set to the `DefaultGroup.UpperBody` slot: ANIMATION.md → "Montages on the upper body", step 7.

---

## When something doesn't show

- **Turn on the event log:** while playing, press the key under Esc to open the console, type `AbilitySystem.DisplayGameplayCues 1`, press Enter. Each event now writes its tag where it happens, on screen and in the Output Log.
  - **The tag shows, but nothing plays:** the notify's **Gameplay Cue Tag** doesn't match exactly, or the notify isn't directly in `Content/GameplayCues`.
  - **The tag doesn't show:** the event isn't sent. Tell me.
- **Output Log** (**Window → Output Log**): warnings show up there in yellow or red, for example a montage made for the wrong skeleton, or a take-out animation that moves the character.
- **A sound plays at full volume everywhere:** it has no attenuation (see 4A).
