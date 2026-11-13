"""Switches off the collision of the old landmark church that still stands, invisible, inside the rebuilt
Girona cathedral in Lvl_CathedralDistrict_Art.

The old church was hidden when the cathedral replaced it, but its collision stayed: cubes tagged
'lda_collision' (CathedralBlock_*), the hidden church mesh and its crossing tower, vault and spire. They
stopped the player 1.4 m in front of the new main front, closed both side-door recesses and stood as an
unseen 88 m tower above the roof. The cathedral now has its own collision (SM_Cathedral_Core_liner).

Nothing is deleted. A piece with a named collision profile (BlockAll) gets the profile NoCollision and a
tag 'cath_old_profile:<name>' that remembers the old one: a named profile is applied again when the level
loads, so switching only "collision enabled" would not last. A piece with a custom profile keeps it and
only has its collision disabled.

To give the old pieces their collision back, set CATHEDRAL_OLD_COLLISION = True and run again.

Run in the open editor (Output Log, Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/v3/old_church_collision_ue.py").read())
Save the level afterwards.
"""
import unreal

NAMES = ("CathedralBlock_", "Cathedral_LandmarkChurch", "Cathedral_CrossingTower", "Cathedral_CrossingVault", "Cathedral_Spire")
TAG = "cath_old_profile:"
RESTORE = bool(globals().get("CATHEDRAL_OLD_COLLISION", False))
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
changed = []
for actor in eas.get_all_level_actors():
    label = actor.get_actor_label()
    if not label.startswith(NAMES):
        continue
    if actor.get_editor_property("root_component").get_editor_property("visible") and not RESTORE:
        unreal.log_warning(f"[OldChurch] {label} is visible, left alone")
        continue
    tags = [str(t) for t in actor.tags]
    remembered = [t[len(TAG):] for t in tags if t.startswith(TAG)]
    for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
        profile = str(comp.get_collision_profile_name())
        if RESTORE:
            if remembered and profile == "NoCollision":
                actor.modify(); comp.modify()
                comp.set_collision_profile_name(remembered[0])
                changed.append(label)
            elif profile == "Custom" and comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                actor.modify(); comp.modify()
                comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
                changed.append(label)
        elif profile == "Custom":
            if comp.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                actor.modify(); comp.modify()
                comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                changed.append(label)
        elif profile != "NoCollision":
            actor.modify(); comp.modify()
            if not remembered:
                tags.append(TAG + profile)
                remembered = [profile]
            comp.set_collision_profile_name("NoCollision")
            changed.append(label)
    if RESTORE:
        tags = [t for t in tags if not t.startswith(TAG)]
    if tags != [str(t) for t in actor.tags]:
        actor.modify()
        actor.set_editor_property("tags", tags)
unreal.log(f"[OldChurch] collision {'restored on' if RESTORE else 'switched off on'} {len(changed)}: {sorted(set(changed))}")
