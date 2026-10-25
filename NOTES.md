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

### Aim offsets

- **AO_Rifle's center pose had the wrong base pose.** `MM_Rifle_Idle_ADS_AO_CC` (Manny) had `MF_Rifle_Idle_ADS` (Quinn) as Base Pose Animation, so the character was rotated in game even when looking straight ahead. Fixed by setting its Base Pose Animation to `MM_Rifle_Idle_ADS_AO_CC`. `_CU` and `_CD` still have `MF_Rifle_Idle_ADS` as their base and look fine in game.

## Known issues

- **Pistol run, right knee on slopes:** the right knee bends outward when the right foot lands on a slope while running with the pistol. Something in the pistol animation causes it, not the IK settings.
- **Rifle aim, left shoulder on backward walks:** the left shoulder moves oddly in `MF_Rifle_Walk_Bwd`, `Bwd_Left` and `Bwd_Right`. It's in the animations themselves. Probably the upper arm, but no single value in Sequencer stands out, and rotating one bone moves the others too.
