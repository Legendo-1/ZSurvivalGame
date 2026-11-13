"""Puts BP_GironaCathedral (the cathedral shell and its side portal, one actor) back into
Lvl_CathedralDistrict_Art at the spot it had on 2026-11-11, or takes it out again.

Open Lvl_CathedralDistrict_Art, then in the Output Log (Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/place_cathedral_ue.py").read())            # place it
  MODE = "remove"; exec(open(r"<project>/SourceArt/Cathedral/place_cathedral_ue.py").read())   # remove it

By hand: drag Content/Buildings/Cathedral/BP_GironaCathedral into the level and set
Location (21590, 3450, 1300), Rotation (0, 0, -180), Scale 1. Save the level afterwards.
"""
import unreal

BP_PATH = "/Game/Buildings/Cathedral/BP_GironaCathedral"
LOCATION = unreal.Vector(21590.0, 3450.0, 1300.0)
ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=-180.0)
LABEL = "BP_GironaCathedral"

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mode = globals().get("MODE", "place")


def log(msg):
    unreal.log(f"[Cathedral] {msg}")


placed = [a for a in eas.get_all_level_actors() if a.get_class().get_name() == "BP_GironaCathedral_C"]
if mode == "remove":
    for a in placed:
        eas.destroy_actor(a)
    log(f"removed {len(placed)} cathedral actor(s); save the level to keep the change")
elif placed:
    log(f"already in the level at {placed[0].get_actor_location()}, nothing done")
else:
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
    actor = eas.spawn_actor_from_class(bp_class, LOCATION, ROTATION)
    actor.set_actor_label(LABEL)
    log(f"placed at {actor.get_actor_location()} yaw {actor.get_actor_rotation().yaw:.0f}; save the level to keep it")
