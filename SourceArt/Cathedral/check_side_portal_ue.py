"""Checks SM_Girona_SidePortal after import and cleans up what the import left behind.

Paste into the editor's Output Log in Python mode:
  exec(open(r"<project>/SourceArt/Cathedral/check_side_portal_ue.py").read())

- Reports triangles, Nanite, material slots and collision (convex hulls are counted separately
  from boxes/spheres/capsules).
- Deletes the placeholder materials Stone / Trim / Door that the FBX import created in
  /Game/Buildings/Cathedral, but only if nothing references them.
- If the mesh ended up with no collision at all, falls back to convex decomposition.
"""
import unreal

MESH_PATH = "/Game/Buildings/Cathedral/SM_Girona_SidePortal"
JUNK = ["/Game/Buildings/Cathedral/Stone", "/Game/Buildings/Cathedral/Trim", "/Game/Buildings/Cathedral/Door"]

eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(msg):
    unreal.log(f"[SidePortal] {msg}")


mesh = eal.load_asset(MESH_PATH)
log(f"triangles LOD0: {mesh.get_num_triangles(0)}, LODs: {mesh.get_num_lods()}")
log(f"nanite enabled: {mesh.get_editor_property('nanite_settings').get_editor_property('enabled')}")
for i, sm in enumerate(mesh.get_editor_property("static_materials")):
    mi = sm.get_editor_property("material_interface")
    log(f"slot {i} '{sm.get_editor_property('material_slot_name')}' = {mi.get_path_name() if mi else None}")

convex = sub.get_convex_collision_count(mesh)
simple = sub.get_simple_collision_count(mesh)
log(f"collision: {convex} convex hulls, {simple} boxes/spheres/capsules, "
    f"complexity {sub.get_collision_complexity(mesh)}")
if convex + simple == 0:
    log("no collision found, generating 12 convex hulls by decomposition")
    sub.set_convex_decomposition_collisions_with_notification(mesh, 12, 16, 100000, True)
    log(f"collision now: {sub.get_convex_collision_count(mesh)} convex hulls")

for path in JUNK:
    if not eal.does_asset_exist(path):
        continue
    cls = eal.load_asset(path).get_class().get_name()
    refs = [r for r in eal.find_package_referencers_for_asset(path, load_assets_to_confirm=True)
            if r != path]
    if refs:
        log(f"kept {path} ({cls}), referenced by {refs}")
    else:
        eal.delete_asset(path)
        log(f"deleted unused placeholder {path} ({cls})")

eal.save_asset(MESH_PATH, only_if_is_dirty=True)
log("CHECK DONE")
