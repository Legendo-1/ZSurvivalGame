"""Imports one house (SourceArt/Houses/build/<Folder>/SM_House_<Folder>_{Ext,Int}.fbx) into /Game/Buildings/Houses/<Folder>/
and (re)builds BP_House_<Folder> from them.

Run through SourceArt/Houses/tools/ue_house.py (it sets HOUSE_NAME), after make_house_materials_ue.py.
Safe to re-run after a rebuild: the meshes are replaced in place, so placed Blueprints pick up the change.

What it fixes after the import (the scripted Interchange import ignores parts of the pipeline, see the ledger):
  - build settings: recompute normals and weighted normals off (the FBX carries every hard and soft edge), no
    lightmap UVs (the level is lit by Lumen); Nanite on;
  - materials: slot "<Surface>" -> MI_House_<Folder>_<Surface>; the placeholder materials the import made are deleted;
  - collision: the Ext mesh keeps its UCX_ hulls (walls, floors, stair ramps, guards; doors open), the Int mesh has none.
The Blueprint holds both meshes at its origin. Its origin is the house's front-left corner at street level (house
space); see the ledger, section 3, for the transform to the level.
"""
import os

import unreal

FOLDER = HOUSE_NAME  # noqa: F821 (set by ue_house.py)
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt", "Houses", "build", FOLDER)
DEST = f"/Game/Buildings/Houses/{FOLDER}"
MATS = f"{DEST}/Materials/MI_House_{FOLDER}_{{}}"
PIPELINE = "/Game/Buildings/Houses/Tools/IP_HouseImport"
PARTS = ("Ext", "Int")

eal = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
ar = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log(f"[HouseImport] {msg}")


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
    except Exception as exc:
        log(f"pipeline: could not set {prop}: {exc}")


def import_pipeline():
    if not eal.does_asset_exist(PIPELINE):
        eal.duplicate_asset("/Interchange/Pipelines/DefaultAssetsPipeline", PIPELINE)
    p = eal.load_asset(PIPELINE)
    try_set(p.mesh_pipeline, "generate_lightmap_u_vs", False)
    try_set(p.mesh_pipeline, "build_nanite", True)
    try_set(p.mesh_pipeline, "import_skeletal_meshes", False)
    try_set(p.material_pipeline, "import_materials", False)
    try_set(p.common_meshes_properties, "recompute_normals", False)
    try_set(p.common_meshes_properties, "compute_weighted_normals", False)
    try_set(p.common_meshes_properties, "recompute_tangents", True)
    eal.save_asset(PIPELINE, only_if_is_dirty=False)
    return p


def import_fbx(fbx, name):
    params = unreal.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", True)
    params.set_editor_property("destination_name", name)
    params.set_editor_property("override_pipelines", [unreal.SoftObjectPath(import_pipeline().get_path_name())])
    manager = unreal.InterchangeManager.get_interchange_manager_scripted()
    return manager.import_asset(DEST, unreal.InterchangeManager.create_source_data(fbx), params)


meshes = {}
for part in PARTS:
    name = f"SM_House_{FOLDER}_{part}"
    fbx = os.path.join(SRC, name + ".fbx")
    path = f"{DEST}/{name}"
    before = set(str(a.package_name) for a in ar.get_assets_by_path(DEST, recursive=False))
    import_fbx(fbx, name)
    mesh = eal.load_asset(path)
    if mesh is None:
        raise RuntimeError(f"{path} was not created from {fbx}")
    build = sub.get_lod_build_settings(mesh, 0)
    wanted = {"recompute_normals": False, "compute_weighted_normals": False, "generate_lightmap_u_vs": False}
    if any(build.get_editor_property(k) != v for k, v in wanted.items()):
        for k, v in wanted.items():
            build.set_editor_property(k, v)
        sub.set_lod_build_settings(mesh, 0, build)
    nanite = mesh.get_editor_property("nanite_settings")
    if not nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", True)
        sub.set_nanite_settings(mesh, nanite, apply_changes=True)
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        mat = eal.load_asset(MATS.format(slot))
        if mat is None:
            log(f"WARNING: {name}: no material for slot '{slot}'")
        elif sm.get_editor_property("material_interface") != mat:
            mesh.set_material(i, mat)
    if part == "Ext":
        log(f"{name}: {sub.get_convex_collision_count(mesh)} UCX collision hulls")
    elif sub.get_simple_collision_count(mesh) + sub.get_convex_collision_count(mesh) > 0:
        sub.remove_collisions(mesh)
    if part != "Ext":
        body = mesh.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    for a in ar.get_assets_by_path(DEST, recursive=False):
        p = str(a.package_name)
        if p not in before and p != path and isinstance(a.get_asset(), unreal.MaterialInterface):
            if not [r for r in eal.find_package_referencers_for_asset(p, load_assets_to_confirm=True) if r != p]:
                eal.delete_asset(p)
    eal.save_asset(path, only_if_is_dirty=False)
    bb = mesh.get_bounding_box()
    used = [m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None
            for m in mesh.get_editor_property("static_materials")]
    log(f"{name}: nanite {mesh.get_editor_property('nanite_settings').get_editor_property('enabled')}, "
        f"bounds ({bb.min.x:.0f}, {bb.min.y:.0f}, {bb.min.z:.0f}) to ({bb.max.x:.0f}, {bb.max.y:.0f}, {bb.max.z:.0f}), materials {used}")
    meshes[part] = mesh

# --- the Blueprint ------------------------------------------------------------------------------
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
bp_name = f"BP_House_{FOLDER}"
bp_path = f"{DEST}/{bp_name}"
if eal.does_asset_exist(bp_path):
    bp = eal.load_asset(bp_path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(bp_name, DEST, unreal.Blueprint, factory)
    log(f"created {bp_path}")
handles = sds.k2_gather_subobject_data_for_blueprint(bp)
existing = {}
for h in handles:
    obj = lib.get_object(lib.get_data(h))
    if isinstance(obj, unreal.StaticMeshComponent):
        existing[str(lib.get_variable_name(lib.get_data(h)))] = obj
for part, mesh in meshes.items():
    comp = existing.get(part)
    if comp is None:
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.StaticMeshComponent, blueprint_context=bp)
        handle, fail = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"could not add {part}: {fail}")
        sds.rename_subobject(handle, unreal.Text(part))
        comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
    if part != "Ext":
        comp.set_collision_profile_name("NoCollision")
        comp.set_editor_property("can_ever_affect_navigation", False)    # the NavMesh follows the Ext collision only
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_asset(bp_path, only_if_is_dirty=False)
log(f"{bp_name}: components {list(meshes)}")
log("DONE")
