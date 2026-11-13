"""Places one house in the open level (Lvl_CathedralDistrict_Art) and hides the old houses it replaces.

Run through SourceArt/Houses/tools/ue_house.py (it sets HOUSE_NAME). Reads "place" from the house spec:
  "place": {"location": [x, y, z], "yaw": deg, "hide": ["<old actor label>", ...]}
location is the world position (cm) of the house-space origin (front-left corner at street level); yaw follows the
rule in the ledger, section 3 (yaw = the direction the front faces - 90).

  - the old actors are hidden, never deleted: temporarily hidden in the editor, hidden in game, no collision. Their
    labels stay in the spec and the ledger, so it can be undone (HOUSE_UNDO = True shows them again and removes the
    new house);
  - the house is the actor House_<Folder> (BP_House_<Folder>); an existing one is moved, not duplicated;
  - a NavMeshBoundsVolume House_<Folder>_NavBounds covers the house from the street to the roof, so the NavMesh
    reaches every floor (the district volume stops at z 2800);
  - the level is saved.
"""
import json
import os

import unreal

FOLDER = HOUSE_NAME  # noqa: F821 (set by ue_house.py)
UNDO = bool(globals().get("HOUSE_UNDO", False))
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
spec = json.load(open(os.path.join(PROJECT, "SourceArt", "Houses", "specs", FOLDER + ".json"), encoding="utf-8"))
pl = spec["place"]
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
LABEL, NAV = f"House_{FOLDER}", f"House_{FOLDER}_NavBounds"


def log(msg):
    unreal.log(f"[HousePlace] {msg}")


actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
for lab in pl.get("hide", []):
    a = actors.get(lab)
    if a is None:
        log(f"WARNING: no actor '{lab}' to hide")
        continue
    a.set_is_temporarily_hidden_in_editor(not UNDO)
    a.set_actor_hidden_in_game(not UNDO)
    a.set_actor_enable_collision(UNDO)
    for c in a.get_components_by_class(unreal.PrimitiveComponent):    # no collision is not enough: the NavMesh
        c.set_editor_property("can_ever_affect_navigation", UNDO)     # generator must leave the old house out too
    log(f"{'showed' if UNDO else 'hid'} {lab}")

if UNDO:
    for lab in (LABEL, NAV):
        if lab in actors:
            eas.destroy_actor(actors[lab])
            log(f"removed {lab}")
else:
    loc = unreal.Vector(*pl["location"])
    rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=pl["yaw"])
    house = actors.get(LABEL)
    if house is None:
        house = eas.spawn_actor_from_class(eal.load_blueprint_class(f"/Game/Buildings/Houses/{FOLDER}/BP_House_{FOLDER}"), loc, rot)
        house.set_actor_label(LABEL)
    else:
        house.set_actor_location_and_rotation(loc, rot, False, False)
    o, e = house.get_actor_bounds(False)
    log(f"{LABEL} at {loc}, yaw {pl['yaw']}; bounds x {o.x - e.x:.0f}..{o.x + e.x:.0f}, y {o.y - e.y:.0f}..{o.y + e.y:.0f}, z {o.z - e.z:.0f}..{o.z + e.z:.0f}")
    nav = actors.get(NAV)
    if nav is None:
        nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, o, unreal.Rotator(0, 0, 0))
        nav.set_actor_label(NAV)
    nav.set_actor_location(o, False, False)
    # the brush of a spawned volume is a 200 cm cube: scale it to the house plus 3 m round it
    nav.set_actor_scale3d(unreal.Vector((e.x + 300) / 100.0, (e.y + 300) / 100.0, (e.z + 200) / 100.0))
    no, ne = nav.get_actor_bounds(False)
    log(f"{NAV}: bounds z {no.z - ne.z:.0f}..{no.z + ne.z:.0f}")
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
log("level saved")
log("DONE")
