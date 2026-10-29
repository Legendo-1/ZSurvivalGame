# Notes

Things learned the hard way, and known issues. Remove a known issue once it's fixed.

## Gotchas & best practice

### Mixamo animations

- **Y Bot originals:** keep **Force Root Lock** and **Enable Root Motion** off.
- **Manny copies (MX_):** **Force Root Lock** on.
- Download from Mixamo with **In Place** checked.
- **Exception, `MX_Sprint_Edited`** (in `Anims/Unarmed/Sprint`): an unarmed Mixamo sprint, used in the pistol blend space. Force Root Lock is off, since turning it on tilted the body. It still moved forward even though it was downloaded In Place.
  - The root's forward movement was deleted in Sequencer (**Edit in Sequencer → FK Control Rig**). The sideways sway stays.
  - The root was rotated slightly left with one key in the animation editor's curve editor, so he runs straight ahead.
  - The feet were rotated with several keys in the curve editor, so they point less outward.

### Editing animations

- **Loops:** the last frame should have the same pose as frame 0. The animation asset treats them as the same moment, so the pose doesn't show twice.
- **Judge loops in the animation window or in game, not in Sequencer.** Sequencer plays the last frame and frame 0 separately, which looks like a pause that isn't really there.
- **Cutting a loop:** cut at an **L** sync marker and end at the next L marker after it, so the cut holds one full step cycle.
- **Edit in Sequencer** creates a `Driving_` Level Sequence next to the animation. Delete it once the changes are saved; the animation keeps them.

### Foot IK

- **Foot IK overrides foot rotation edits.** CR_Mannequin_FootIK (the Control Rig node in ABP_Manny) places each foot from the `ik_foot_l` / `ik_foot_r` bones, not from `foot_l` / `foot_r`. A rotation keyed on `foot_l` / `foot_r` shows in the animation preview but is undone in game.
- **Fix in use:** Rotation Alpha is 0 on both foot effectors in CR_Mannequin_FootIK. The IK still moves the feet to the ground, but their rotation comes from the animation.

### Blend spaces and the ABP

- **Keep idle out of the blend spaces.** The Idle state in ABP_Manny plays the idle animations directly. An idle sample combined with a changed Rate Scale on a jog made the character tilt when stopping.
- **Set CurrentWeaponType in BP_ThirdPersonCharacter's Class Defaults.** The ABP's variable default only affects the ABP preview, not the game.

### Melee

- **Bones only move on screen.** The engine doesn't update a character's bones while nobody sees it (the server's enemies, a test without a window). Melee hitboxes follow bones, so the melee trace component keeps them updating while a hitbox is active. Anything else that reads bone or socket positions off screen needs the same (the mesh's **Visibility Based Anim Tick Option**).
- **`weapon_r` isn't moved by every animation.** The mannequin's unarmed animations (the placeholder attacks) don't animate it, so it stays at the feet during them. The machete uses `hand_r` as its Attach Socket for that reason. Weapons that use `weapon_r` need animations that move it (the rifle and pistol ones do).
- **The right hand's bone axes:** X points toward the elbow, Z toward the thumb and index finger. A blade that sticks out of the fist runs along the hand's Z (the machete's Attach Offset: pitch 90).
- **Old gun data assets were `ZWeaponData`.** That class is now `ZRangedWeaponData` (the melee weapons got their own, `ZMeleeWeaponData`). A redirect in `Config/DefaultEngine.ini` (**CoreRedirects**) loads old ones; DA_Pistol and DA_Rifle are saved under the new name already. Keep the redirect for assets made elsewhere.
- **Attack animations that turn the root bone.** Attacks keep the character facing the camera and drop any turning from the animation's root motion. A retarget from Mixamo can put the hips' turning into the root bone (the root turns, the pelvis doesn't); in the game that turning is lost. Fix it in the IK Retargeter you used: in its root motion settings, turn off rotating the root with the pelvis, then export the animations again.
- **Testing melee:** `ZSpawnDummy 120` puts a still zombie in front of you. Enemies from `ZSpawnZombies` attack from about 1.5 m, farther than the placeholder machete reaches, so walk up to them.
- **Test runs without a window** create moves faster than a client can send them ("Hit limit of 96 saved moves"); `t.MaxFPS 60` on both test processes avoids it.

### Collision channels

- **PlayerSpacing and EnemySpacing** (Project Settings → Collision → Object Channels) belong to the spacing capsules around characters (COMBAT.md → Spacing). Leave them on **Ignore** for everything else: a wall or a pickup that blocks them would push characters around.

### AI

- **`Lvl_ThirdPerson` has no navmesh.** Enemies then walk straight at their target and get stuck behind walls (and behind other players), and at the foot of a ledge you jumped onto, even with a ramp next to it. AI.md → "Moving around" has the steps to add one.
- **`Z.AI.Debug` needs the 1:** `Z.AI.Debug 1`. Typed alone it only writes `Z.AI.Debug = "false"`.
- **A bigger enemy swings higher.** Scaling an enemy up scales its attack animations too: the Stone Golem's swings passed over the player's head at 1.25 times its size (3 of 11 hit), and hit the chest at 1.1 times (6 of 12). Before making a pack's enemy much bigger, try a few swings with `Z.Melee.DrawDebug 1`.
- **Gaps between attack ranges:** if no attack covers a distance (e.g. a lunge to 2.6 m and a charge from 3 m), the enemy walks in until its planned attack fits; earlier it could stand in the gap forever. AI.md → Attacks.
- **Shots go through parts of a new enemy:** hits only count on the body shapes of the mesh's Physics Asset, not on the mesh you see. A bulky mesh (a golem's head, armor) sticks out past them. AI.md → "A new kind of enemy" step 5. The Stone Golem has its own grown copy (`PHYS_StoneGolem_Enemy`).
- **Enemies only fight what they sense:** they no longer find every player within a radius through walls. A zombie behind you that looks away stays Idle until you get close, hit it, or it turns and sees you.

### Asset packs from Fab

- **Their demo files can be broken.** The Stone Golem pack's copy of `CR_Mannequin_Body` pointed to rig functions from its maker's computer. It failed to compile every time the golem loaded (its foot IK rig previews on the pack's mannequin, which uses that rig), and Play showed "Blueprint failed to compile: CR_Mannequin_Body". It now points to the project's own `Characters/Mannequins/Rigs/CR_Mannequin_Body` (the pack's file is a redirector). The same fix for another pack: right-click the broken asset → **Delete** → under **Replace References** pick the project's working copy → **Replace References**.
- **Walks that move the root bone.** The golem pack's demo walk and run (`MM_Walk_Fwd`, `MM_Run_Fwd`) move the root bone forward without root motion turned on, so the body walked away from its capsule and jumped back every loop. They have **Force Root Lock** on now. Check this on a pack's walks and runs (the animation's **Root Motion** settings).
- **Foot IK needs animations that move the ik_foot bones.** The Stone Golem's own animations (attacks, walks) never move `ik_foot_l` / `ik_foot_r`, so the foot IK rig in its animation Blueprint (`CR_Mannequin_BasicFootIK`) pulled its feet into the standing pose during attacks (stiff, parallel legs). Fix: that Control Rig node's **Alpha** to 0 (or remove it). Check a pack's animations for this before using its foot IK.
- **Keep your own versions out of the pack's folders** (e.g. `Content/Enemies/StoneGolem`), so updating a pack can't overwrite them.

### Materials on characters

- **They need Used with Skeletal Mesh** (material **Details → Usage**). `M_HitFlash` didn't have it: the editor turned it on by itself with a warning ("Overlay material with missing skel mesh usage flag"), but a packaged game would have shown the default checkered material instead. It's on now.

### Assets named only in settings

- **They aren't packed into a packaged game by themselves.** `M_HitFlash` (Project Settings → Game → Combat → **Hit Flash Material**) and `DA_Footsteps` (Footsteps → **Default Footstep Data**) are only named in settings. Before making a packaged build: Project Settings → **Packaging** → **Additional Asset Directories to Cook** → add `Content/Effects` and `Content/Audio/Footsteps`. Playing in the editor doesn't need it.

### Aim offsets

- **AO_Rifle's center pose had the wrong base pose.** `MM_Rifle_Idle_ADS_AO_CC` (Manny) had `MF_Rifle_Idle_ADS` (Quinn) as Base Pose Animation, so the character was rotated in game even when looking straight ahead. Fixed by setting its Base Pose Animation to `MM_Rifle_Idle_ADS_AO_CC`. `_CU` and `_CD` still have `MF_Rifle_Idle_ADS` as their base and look fine in game.

## Known issues

- **C++ builds failed after the Fab plugin update of 2026-10-29:** "Expecting to find a type to be declared in a module rules named 'FabToolset'". The Epic Games Launcher updated the engine's Fab plugin with a new part, FabToolset, that the engine's own build rules don't know about. Every editor build stopped with this error (Visual Studio, the editor's compile button, the "rebuild modules?" prompt when opening the project), in any project.
  - **Fixed on this PC** by taking FabToolset out of the engine's Fab plugin file, `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Fab\Fab.uplugin`. The original is next to it as `Fab.uplugin.backup-2026-10-29`. FabToolset never loads by itself (it's an add-on for an experimental engine feature); the Fab window works as before.
  - **If the error comes back** (after the launcher's **Verify**, or the next Fab update), or on another PC: close the editor, open that file in Notepad (as administrator if it won't save), and delete these lines in **"Modules"**: the `},` and `{` right before `"Name": "FabToolset"`, and everything from there down to the `]` and `}` that close its **"TargetAllowList"**. What's left in "Modules" is only the block for `"Name": "Fab"`. Save.
  - **Don't build with `-DisablePlugin=Fab`** to get around it: the build then also switches Fab off in the editor.
  - Remove this note once a Fab update builds without the fix.
  - **A harmless error in the Output Log since that update:** "LogPython: Error: ... ToolsetRegistry ... init_unreal.py ... has no attribute 'PythonTestRunner'". It comes from an experimental engine plugin the Fab update switched on, and changes nothing in the game.

- **Pistol run, right knee on slopes:** the right knee bends outward when the right foot lands on a slope while running with the pistol. Something in the pistol animation causes it, not the IK settings.
- **Small corrections at the start of a melee attack:** a client's position can be put back by 2 or 3 cm when an attack starts right after a combo step or a stagger. Normal for attacks that move the character with their animation, and hard to see.
- **Zombies outreach the placeholder machete:** they attack from about 1.5 m and their punch reaches 1.7 m, while the placeholder punches with a stretched box reach about 1 m. Real machete animations swing farther; otherwise lower the zombie's punch: **AI|Attacks → Attacks → 0 → Max Range** and **Fallback Hit → Reach** (it's set in C++, `AZZombieCharacter`, so a zombie Blueprint changes it).
- **Rifle aim, left shoulder on backward walks:** the left shoulder moves oddly in `MF_Rifle_Walk_Bwd`, `Bwd_Left` and `Bwd_Right`. It's in the animations themselves. Probably the upper arm, but no single value in Sequencer stands out, and rotating one bone moves the others too.
