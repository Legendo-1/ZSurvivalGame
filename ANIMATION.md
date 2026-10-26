# Animation

How the character's animations are connected, from the mouse button to the pose you see.

**Last updated:** 2026-10-26 (Footstep notifies in the walk, jog and sprint animations)

The chain has three layers, each passing data to the next.

## 1. The character, in C++ (`ZSurvivalGameCharacter`)

The character knows **what the player is doing**. It knows nothing about animations.

| Function | Answers |
|---|---|
| `IsAiming()` | Is the right mouse button held? |
| `GetWeaponType()` | Unarmed, Pistol or Rifle? |
| `GetAimOffset()` | How far the camera points up/down compared to the character |

## 2. ABP_Manny's Event Graph: collects the data

Every frame, the Event Graph asks the character and writes the answers into **the ABP's own variables**:

```
Event Blueprint Update Animation
 → Cast To BP_ThirdPersonCharacter   (get hold of the character)
 → Set weapon type    ← Get Weapon Type
 → Set IsAiming       ← Is Aiming
 → Set AimPitch       ← Get Aim Offset → Break Rotator → Pitch
 → Set UseRifleAO     ← (weapon type == Rifle) AND IsAiming
```

**Why variables?** The AnimGraph can't ask the character itself. It only reads the variables. The Event Graph fills them in, and every graph and state in the ABP can read them.

## 3. ABP_Manny's AnimGraph: picks and builds the pose

It's read **from left to right**, and each node changes the pose a little:

```
Locomotion (state machine) → Main States (+ jumping/falling) → Slot → AO_Rifle → Control Rig → Output Pose
```

### Locomotion: what are the legs doing?

- **Idle state** (standing still) → **Blend Poses by weapon type** → on the **Rifle Pose** pin:
  - **Blend Poses by bool** (IsAiming)
    - False → relaxed rifle idle
    - True → `MF_Rifle_Idle_ADS`
- **Walk/Run state** (moving) → **Blend Poses by weapon type** → on the **Rifle Pose** pin:
  - **Blend Poses by bool** (IsAiming)
    - False → `BS_Rifle_Relaxed`
    - True → `BS_Rifle_Aim`

Inside the blend space, Direction and Speed pick which walk to play: forward, sideways, backward, etc.

### Slot

Where montages play: the weapon's Fire, Reload and Equip Montage (set in its Data Asset, see AUDIO.md). Right now it's the whole body, so a reload would stop the legs. "Montages on the upper body" below changes that.

### AO_Rifle: up/down aim

- It takes the pose from the left and **adds** an upper-body bend, based on **AimPitch**.
- It's only on when **UseRifleAO** is true, through the bool pin. Otherwise it passes the pose through unchanged.
- It works on top of both the aim idle and the aim walk, since it comes after them.

The AO poses only store a **difference from a base pose**. See "Aim offsets" in NOTES.md for the base pose fix.

### Control Rig

Foot IK places the feet on the ground. It comes last so it always gets the final say on the feet.

## Montages on the upper body

So the legs keep walking (and the feet keep their blend space) while the arms reload, take out a weapon or fire. Do this once in ABP_Manny:

1. **Make the slot:** open ABP_Manny → **Window → Anim Slot Manager** → **Add Slot** → name it `UpperBody` (in DefaultGroup) → **Save**.
2. **Keep a copy of the walking pose:** in the AnimGraph, right-click empty space → search **New Save cached pose** → name it `LocomotionPose`. Connect **Main States**' output into it (instead of into the Slot).
3. Right-click → search **Use cached pose 'LocomotionPose'**. Do it **twice**, so there are two of these nodes.
4. Right-click → **Layered blend per bone**. Connect:
   - the first **Use cached pose** into its **Base Pose**
   - the second one into a new **Slot** node (right-click → **Slot 'DefaultSlot'**; in its Details set **Slot Name** to `DefaultGroup.UpperBody`), and that Slot into **Blend Poses 0**
5. Select the **Layered blend per bone** → Details → **Layer Setup → Index [0] → Branch Filters → +** → **Bone Name** `spine_01`, **Blend Depth** `0`. Tick **Mesh Space Rotation Blend**.
6. Connect the **Layered blend per bone** into the old **Slot** node (DefaultSlot, kept for whole-body montages like a dodge roll later), so the chain is: Layered blend per bone → Slot (DefaultSlot) → AO_Rifle → Control Rig → Output Pose. **Compile** and **Save**.
7. **In each upper-body montage** (reload, take out, fire): open it → in the montage panel, the slot on the left says `DefaultGroup.DefaultSlot` → click it → pick `DefaultGroup.UpperBody` → **Save**.

Whole-body montages (a dodge roll, a death animation) keep `DefaultGroup.DefaultSlot`.

**A take-out (equip) montage must stay in place:** in its animation's **Asset Details**, **Enable Root Motion** stays off. One that moves the character isn't played (AUDIO.md explains why).

## Footsteps

Every walking, jogging and sprinting animation in the blend spaces above has **Footstep** notifies on a **Footsteps** track, one where each foot touches the ground. They play the footstep sound for the surface under that foot. A new walk or run animation needs them too: AUDIO.md → Footsteps has the steps.

## Summary

> **C++** says *what's happening* → **Event Graph** copies it into *variables* → **AnimGraph** uses the variables to *pick animations* (state machine + Blend Poses) and then *adjust* the pose (AO, foot IK).

Pistol will work the same way later: one more Blend Poses by bool on the **Pistol Pose** pin, and an `AO_Pistol` with its own bool.
