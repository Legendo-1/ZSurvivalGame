# Animation

How the character's animations are connected, from the mouse button to the pose you see.

**Last updated:** 2026-10-25 (rifle aim walk and rifle aim offset added)

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

Where montages (firing, reloading) will be played later.

### AO_Rifle: up/down aim

- It takes the pose from the left and **adds** an upper-body bend, based on **AimPitch**.
- It's only on when **UseRifleAO** is true, through the bool pin. Otherwise it passes the pose through unchanged.
- It works on top of both the aim idle and the aim walk, since it comes after them.

The AO poses only store a **difference from a base pose**. See "Aim offsets" in NOTES.md for the base pose fix.

### Control Rig

Foot IK places the feet on the ground. It comes last so it always gets the final say on the feet.

## Summary

> **C++** says *what's happening* → **Event Graph** copies it into *variables* → **AnimGraph** uses the variables to *pick animations* (state machine + Blend Poses) and then *adjust* the pose (AO, foot IK).

Pistol will work the same way later: one more Blend Poses by bool on the **Pistol Pose** pin, and an `AO_Pistol` with its own bool.
