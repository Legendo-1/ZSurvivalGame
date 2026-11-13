"""Imports the rebuilt cathedral parts (SM_Cathedral_*.fbx beside this script) into
/Game/Buildings/Cathedral/Version_0_3 and assigns the MI_Cath_* materials by slot name.

Run in the open editor (Output Log, Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/v3/import_cathedral_v3_ue.py").read())
To import only some parts, set CATHEDRAL_PARTS first:  CATHEDRAL_PARTS = ["SM_Cathedral_Front_doorbay"]
(the editor keeps Python variables between runs, so set CATHEDRAL_PARTS = None to go back to all parts)

The FBX files are built in the Meshy model's own coordinates (see build_front.py), so a part placed
with the same transform as the model lines up with it. Safe to re-run after rebuilding an FBX.

The parts keep the normals from the FBX. Letting Unreal recompute them made every build of a part
take about 165 s instead of 20 s, and the build scripts already set every hard and soft edge.
Lightmap UVs are off as well: the level is lit by Lumen.
"""
import glob
import os

import unreal

try:
    HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:   # run with exec(), where __file__ isn't set
    HERE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "SourceArt", "Cathedral", "v3")
DEST = "/Game/Buildings/Cathedral/Version_0_3"
MATERIALS = "/Game/CityArt/Materials/MI_Cath_{}"      # FBX slot "Ashlar" -> MI_Cath_Ashlar
PIPELINE = f"{DEST}/Tools/IP_CathedralImport"         # our copy of the engine's default import pipeline
NANITE = True

eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(msg):
    unreal.log(f"[CathImport] {msg}")


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
    except Exception as exc:    # property names differ between engine versions
        log(f"pipeline: could not set {prop}: {exc}")


def import_pipeline():
    """A saved pipeline asset with our settings: Interchange ignores a pipeline object that only lives in memory."""
    if not eal.does_asset_exist(PIPELINE):
        eal.duplicate_asset("/Interchange/Pipelines/DefaultAssetsPipeline", PIPELINE)
    pipeline = eal.load_asset(PIPELINE)
    try_set(pipeline.mesh_pipeline, "generate_lightmap_u_vs", False)
    try_set(pipeline.mesh_pipeline, "build_nanite", NANITE)
    try_set(pipeline.mesh_pipeline, "import_skeletal_meshes", False)
    try_set(pipeline.material_pipeline, "import_materials", False)
    try_set(pipeline.common_meshes_properties, "recompute_normals", False)
    try_set(pipeline.common_meshes_properties, "compute_weighted_normals", False)
    try_set(pipeline.common_meshes_properties, "recompute_tangents", True)
    eal.save_asset(PIPELINE, only_if_is_dirty=False)
    return pipeline


def import_fbx(fbx, name):
    """Import through Interchange with our own pipeline settings, so the asset is built once, the way we want it."""
    pipeline = import_pipeline()
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("destination_name", name)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(pipeline.get_path_name())])
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    return manager.import_asset(DEST, unreal.InterchangeManager.create_source_data(fbx), params)


names = globals().get("CATHEDRAL_PARTS") or [os.path.splitext(os.path.basename(p))[0] for p in sorted(glob.glob(os.path.join(HERE, "SM_Cathedral_*.fbx")))]
for name in names:
    fbx = os.path.join(HERE, name + ".fbx")
    path = f"{DEST}/{name}"
    before = set(str(a.package_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(DEST, recursive=False))
    import_fbx(fbx, name)
    mesh = eal.load_asset(path)
    if mesh is None:
        raise RuntimeError(f"{path} was not created from {fbx}")

    # Whatever the import ignored is set on the asset: first the build settings (one rebuild), then
    # Nanite and the materials together.
    build = sub.get_lod_build_settings(mesh, 0)
    wanted = {"recompute_normals": False, "compute_weighted_normals": False, "generate_lightmap_u_vs": False}
    if any(build.get_editor_property(k) != v for k, v in wanted.items()):
        log(f"{name}: the import ignored the pipeline; fixing {[k for k, v in wanted.items() if build.get_editor_property(k) != v]} on the asset")
        for k, v in wanted.items():
            build.set_editor_property(k, v)
        sub.set_lod_build_settings(mesh, 0, build)
    nanite = mesh.get_editor_property("nanite_settings")
    if nanite.get_editor_property("enabled") != NANITE:
        nanite.set_editor_property("enabled", NANITE)
        sub.set_nanite_settings(mesh, nanite, apply_changes=True)
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        mat = eal.load_asset(MATERIALS.format(slot))
        if mat is None:
            log(f"WARNING: no material for slot '{slot}'")
        elif sm.get_editor_property("material_interface") != mat:
            mesh.set_material(i, mat)
    if "_Core_" in name:                # the core carries the whole building's collision (UCX_ hulls from build_core.py)
        log(f"{name}: {sub.get_convex_collision_count(mesh)} collision hulls")
    elif sub.get_simple_collision_count(mesh) + sub.get_convex_collision_count(mesh) > 0:
        sub.remove_collisions(mesh)     # decorative parts have no collision of their own
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(DEST, recursive=False):
        p = str(a.package_name)         # placeholder materials the import made for the FBX slots
        if p not in before and p != path and isinstance(a.get_asset(), unreal.MaterialInterface):
            if not [r for r in eal.find_package_referencers_for_asset(p, load_assets_to_confirm=True) if r != p]:
                eal.delete_asset(p)
    eal.save_asset(path, only_if_is_dirty=False)
    bb = mesh.get_bounding_box()
    used = [m.get_editor_property("material_interface").get_name() for m in mesh.get_editor_property("static_materials")]
    log(f"{name}: nanite {mesh.get_editor_property('nanite_settings').get_editor_property('enabled')}, "
        f"recompute normals {sub.get_lod_build_settings(mesh, 0).get_editor_property('recompute_normals')}, "
        f"bounds ({bb.min.x:.0f}, {bb.min.y:.0f}, {bb.min.z:.0f}) to ({bb.max.x:.0f}, {bb.max.y:.0f}, {bb.max.z:.0f}), materials {used}")
log("DONE")
