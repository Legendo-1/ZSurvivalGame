"""Places the cathedral in Lvl_CathedralDistrict_Art at the spot the Meshy model had on 2026-11-11.

Open Lvl_CathedralDistrict_Art, then in the Output Log (Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/v3/place_cathedral_v3_ue.py").read())
Set CATHEDRAL_MODE before the exec line to choose what happens (the editor remembers it between runs):
  CATHEDRAL_MODE = "v3"        (default) the rebuilt cathedral, BP_GironaCathedral_V3
  CATHEDRAL_MODE = "original"  the untouched Meshy model from Version_0_2
  CATHEDRAL_MODE = "remove"    no cathedral
Whatever cathedral is in the level is taken out first. Save the level afterwards.

By hand: drag the asset into the level and set Location (21180, 3430, 1300), Rotation (0, 0, 90), Scale 1.
"""
import unreal

BP_PATH = "/Game/Buildings/Cathedral/Version_0_3/BP_GironaCathedral_V3"
ORIGINAL = "/Game/Buildings/Cathedral/Version_0_2/Meshy_AI_girona_cathedral_righ_1111012459_image-to-3d-texture"
LOCATION = unreal.Vector(21180.0, 3430.0, 1300.0)
ROTATION = unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0)

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
mode = globals().get("CATHEDRAL_MODE") or "v3"


def log(msg):
    unreal.log(f"[Cathedral] {msg}")


def is_cathedral(actor):
    if actor.get_class().get_name() == "BP_GironaCathedral_V3_C":
        return True
    comp = actor.get_component_by_class(unreal.StaticMeshComponent)
    mesh = comp.get_editor_property("static_mesh") if comp else None
    return isinstance(actor, unreal.StaticMeshActor) and mesh is not None and mesh.get_path_name().split(".")[0] == ORIGINAL


old = [a for a in eas.get_all_level_actors() if is_cathedral(a)]
for a in old:
    log(f"removed {a.get_actor_label()}")
    eas.destroy_actor(a)
if mode == "v3":
    actor = eas.spawn_actor_from_class(eal.load_blueprint_class(BP_PATH), LOCATION, ROTATION)
    actor.set_actor_label("BP_GironaCathedral_V3")
    log(f"placed the rebuilt cathedral at {actor.get_actor_location()}, yaw {actor.get_actor_rotation().yaw:.0f}")
elif mode == "original":
    actor = eas.spawn_actor_from_object(eal.load_asset(ORIGINAL), LOCATION, ROTATION)
    actor.set_actor_label("Meshy_AI_girona_cathedral_original")
    log(f"placed the original Meshy model at {actor.get_actor_location()}, yaw {actor.get_actor_rotation().yaw:.0f}")
else:
    log("no cathedral placed")
log("save the level to keep the change")
