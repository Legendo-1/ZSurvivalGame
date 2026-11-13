"""Builds /Game/Buildings/Cathedral/Version_0_3/BP_GironaCathedral_V3: the rebuilt cathedral as one actor.

It holds
  - SM_Cathedral_Template: a COPY of the Meshy model from Version_0_2 (the original is never
    changed). Every zone that has been rebuilt is carved out of this copy as a closed recess, so
    old and new stonework never overlap and nothing can be seen through where they meet. When the
    whole outside is rebuilt the copy is dropped.
  - every SM_Cathedral_* part in Version_0_3.
All components sit at the actor's origin: the parts are built in the Meshy model's coordinates.

Run in the open editor (Output Log, Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/v3/make_cathedral_v3_bp_ue.py").read())
Safe to re-run: the template is only rebuilt (from the untouched original) when CUTS has changed.
"""
import math

import unreal

SOURCE = "/Game/Buildings/Cathedral/Version_0_2/Meshy_AI_girona_cathedral_righ_1111012459_image-to-3d-texture"
FOLDER = "/Game/Buildings/Cathedral/Version_0_3"
TEMPLATE = f"{FOLDER}/SM_Cathedral_Template"
BP_NAME = "BP_GironaCathedral_V3"
CUT_MATERIAL = "/Game/CityArt/Materials/MI_Cath_Ashlar"    # shown on the faces of a recess

# Rebuilt zones, as upright prisms in the Blender coordinates of the build scripts (metres):
# name: (plan polygon [(x, y), ...], z0, z1). Front = -Y, bell tower = +X.
# A prism is subtracted from the solid model, so it must reach behind the deepest new part (niches go
# 0.44 m into the wall); its faces inside the model become the back and sides of the recess.
# Every polygon must be convex. The prisms of one zone overlap, so no skin is left between them.
_R = 0.7071


def _box(x0, x1, y0, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


_FB = -41.5                                             # back of the recess behind the front
_APSE = [(round(16.57 * math.cos(math.radians(a)), 3), round(39.3 + 16.57 * math.sin(math.radians(a)), 3)) for a in range(-60, 241, 30)]   # apothem 16.0: the template's buttress feet reach 15.98
_CH = [(12.5, -41.992), (15.696, -38.796), (17.11, -40.21)]   # 0.5 m behind the right 45 degree corner, then out past it
CUTS = {
    # the whole main front with its stairs: everything in front of the line 0.5 to 0.7 m behind the new wall faces
    "front_mid": ([(-12.99, -60.0), (12.99, -60.0), (12.99, _FB), (-12.99, _FB)], -1.0, 32.95),
    "front_right": ([(12.5, -60.0), (17.11, -60.0), _CH[2], _CH[1], _CH[0]], -1.0, 32.95),
    "front_left": ([(-x, y) for x, y in ((12.5, -60.0), (17.11, -60.0), _CH[2], _CH[1], _CH[0])][::-1], -1.0, 32.95),
    # the old top edge above the new parapet, left of the bell tower
    "front_top_mid": ([(-12.99, -44.0), (6.0, -44.0), (6.0, _FB), (-12.99, _FB)], 32.9, 33.7),
    "front_top_left": ([(-x, y) for x, y in ((12.5, -44.0), (17.11, -44.0), _CH[2], _CH[1], _CH[0])][::-1], 32.9, 33.7),
    # the bell tower: its right face as far as the porch of the side door; above the porch (z > 17.2) the rest of
    # that face, the back-right corner and the back; and the whole bell storey above the parapet
    "tower_right_low": ([(15.55, -40.0), (17.4, -40.0), (17.4, -36.6), (15.55, -36.6)], -1.0, 17.2),
    "tower_right_high": ([(15.55, -40.0), (17.4, -40.0), (17.4, -34.9), (15.55, -34.9)], 17.2, 32.95),
    "tower_corner_high": ([(15.944, -35.719), (16.863, -34.800), (13.517, -31.456), (12.598, -32.375)], 17.2, 32.95),
    "tower_back_high": ([(11.0, -32.77), (13.4, -32.77), (13.4, -31.2), (11.0, -31.2)], 17.2, 32.95),
    "tower_top": ([(5.3, -42.9), (17.4, -42.9), (17.4, -31.4), (5.3, -31.4)], 32.95, 56.0),
    # right side, from the bell tower to the old tower (y 7.2). The buttresses are 0.92 m to each side of
    # y -22.9, -13.4 and -1.1; the template's stay inside the new ones, only their fronts and tops are taken.
    "sideR_portal": (_box(12.6, 19.5, -36.7, -23.82), -1.0, 17.2),            # the Gothic doorway and its porch
    "sideR_chapels": (_box(16.5, 19.5, -23.82, 7.2), -1.0, 11.3),             # chapel wall, base and buttress feet
    "sideR_nave": (_box(10.35, 11.0, -23.82, 7.2), 11.0, 29.95),              # face of the high wall
    "sideR_nave_porch": (_box(10.35, 11.0, -32.3, -23.82), 17.2, 29.95),      # the same above the porch
    "sideR_butt1_front": (_box(15.5, 19.5, -23.82, -21.98), 11.3, 29.2),
    "sideR_butt2_front": (_box(15.5, 19.5, -14.32, -12.48), 11.3, 29.2),
    "sideR_butt3_front": (_box(15.5, 19.5, -2.02, -0.18), 11.3, 29.2),
    "sideR_butt1_top": (_box(10.35, 15.6, -23.82, -21.98), 27.0, 29.2),
    "sideR_butt2_top": (_box(10.35, 15.6, -14.32, -12.48), 27.0, 29.2),
    "sideR_butt3_top": (_box(10.35, 15.6, -2.02, -0.18), 27.0, 29.2),
    # right side beyond the last buttress: the old Romanesque tower, the chapel bay and the high wall behind it
    "sideR_oldtower": (_box(10.4, 19.5, 7.1, 14.1), -1.0, 32.6),
    "sideR_chapel4": (_box(16.5, 19.5, 14.1, 24.0), -1.0, 11.3),
    "sideR_nave5": (_box(10.35, 11.0, 14.1, 20.5), 11.0, 29.95),
    # left side: the block behind the front's corner (side face, back above the porch, top), the columned
    # doorway in its porch, then chapels, high wall, buttresses and the old tower as on the right
    "sideL_corner": (_box(-17.4, -15.55, -40.0, -35.6), -1.0, 32.95),
    "sideL_corner_back": (_box(-17.4, -11.0, -36.1, -35.0), 17.2, 32.95),
    "sideL_corner_top": (_box(-17.4, -10.35, -41.5, -35.0), 32.9, 34.5),
    "sideL_doorway": (_box(-19.5, -13.3, -35.6, -23.82), -1.0, 17.2),
    "sideL_chapels": (_box(-19.5, -16.5, -23.82, 13.9), -1.0, 11.3),
    "sideL_nave": (_box(-11.0, -10.35, -23.82, 13.9), 11.0, 29.95),
    "sideL_nave_porch": (_box(-11.0, -10.35, -35.6, -23.82), 17.2, 29.95),
    "sideL_butt1_front": (_box(-19.5, -15.5, -23.82, -21.98), 11.3, 29.2),
    "sideL_butt2_front": (_box(-19.5, -15.5, -14.32, -12.48), 11.3, 29.2),
    "sideL_butt3_front": (_box(-19.5, -15.5, -2.02, -0.18), 11.3, 29.2),
    "sideL_butt1_top": (_box(-15.6, -10.35, -23.82, -21.98), 27.0, 29.2),
    "sideL_butt2_top": (_box(-15.6, -10.35, -14.32, -12.48), 27.0, 29.2),
    "sideL_butt3_top": (_box(-15.6, -10.35, -2.02, -0.18), 27.0, 29.2),
    "sideL_oldtower": (_box(-19.5, -10.4, 13.9, 20.65), -1.0, 32.6),
    "sideL_chapel4": (_box(-19.5, -16.5, 20.65, 24.0), -1.0, 11.3),
    # the rear: the whole two-tier apse with its roof (a twelve-sided prism round (0, 39.3), cut straight where it
    # meets the nave), then on each side the low annexes, the end of the chapel wall and the chapels' east wall
    "apse": (_APSE, -1.0, 38.5),
    "apse_annexR": (_box(10.5, 19.5, 26.4, 34.5), -1.0, 9.0),
    "apse_chapelendR": (_box(16.5, 19.5, 23.9, 26.9), -1.0, 11.3),
    "apse_eastwallR": (_box(5.5, 17.5, 25.9, 27.2), 4.0, 11.3),
    "apse_annexL": (_box(-19.5, -10.5, 26.4, 34.5), -1.0, 9.0),
    "apse_chapelendL": (_box(-19.5, -16.5, 23.9, 26.9), -1.0, 11.3),
    "apse_eastwallL": (_box(-17.5, -5.5, 25.9, 27.2), 4.0, 11.3),
}
SIGNATURE = "prisms-1 " + repr(sorted(CUTS.items()))

# The whole outside is rebuilt, so the template copy is no longer part of the building. Set
# CATHEDRAL_TEMPLATE = True before running to carve it again and put it back (for comparing or measuring).
USE_TEMPLATE = bool(globals().get("CATHEDRAL_TEMPLATE", False))

eal = unreal.EditorAssetLibrary
assets = unreal.GeometryScript_AssetUtils
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log(f"[CathedralV3] {msg}")


def read_lod(asset):
    hi_res = assets.get_num_static_mesh_lods_of_type(asset, unreal.GeometryScriptLODType.HI_RES_SOURCE_MODEL) > 0
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.HI_RES_SOURCE_MODEL if hi_res else unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property("lod_index", 0)
    return lod, hi_res


# --- 1. the template copy, with the rebuilt zones carved out ---------------------------------------
if not eal.does_asset_exist(TEMPLATE):
    eal.duplicate_asset(SOURCE, TEMPLATE)
    log(f"copied the Meshy model to {TEMPLATE}")
template = eal.load_asset(TEMPLATE)
if not USE_TEMPLATE:
    log("template: not used (CATHEDRAL_TEMPLATE is not set)")
elif eal.get_metadata_tag(template, "cathedral_cuts") == SIGNATURE:
    log("template: up to date with CUTS")
else:
    source = eal.load_asset(SOURCE)
    lod, _ = read_lod(source)
    read_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    read_opts.set_editor_property("apply_build_settings", False)
    read_opts.set_editor_property("request_tangents", True)
    mesh = unreal.DynamicMesh()
    _, outcome = assets.copy_mesh_from_static_mesh(source, mesh, read_opts, lod)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"could not read {SOURCE}: {outcome}")
    before = mesh.get_triangle_count()
    _area, volume_before = unreal.GeometryScript_MeshQueries.get_mesh_volume_area(mesh)
    tools_volume = 0.0
    bool_opts = unreal.GeometryScriptMeshBooleanOptions()
    bool_opts.set_editor_property("fill_holes", True)
    bool_opts.set_editor_property("simplify_output", False)
    prim = unreal.GeometryScriptPrimitiveOptions()
    prim.set_editor_property("material_id", 1)                    # the recess faces get the second material slot
    for name, (polygon, z0, z1) in CUTS.items():
        # the asset is in Unreal units: centimetres, Y mirrored against Blender
        plan = [unreal.Vector2D(x * 100.0, -y * 100.0) for x, y in polygon]
        for attempt in range(2):                                  # the prism must not come out inside-out
            tool = unreal.DynamicMesh()
            unreal.GeometryScript_Primitives.append_simple_extrude_polygon(
                tool, prim, unreal.Transform(location=unreal.Vector(0.0, 0.0, z0 * 100.0)), plan, height=(z1 - z0) * 100.0,
                height_steps=0, capped=True, origin=unreal.GeometryScriptPrimitiveOriginMode.BASE)
            _area, volume = unreal.GeometryScript_MeshQueries.get_mesh_volume_area(tool)
            if volume > 0.0:
                break
            plan = plan[::-1]
        if volume <= 0.0:
            raise RuntimeError(f"cut '{name}': could not build its prism (volume {volume})")
        tools_volume += volume
        n0 = mesh.get_triangle_count()
        unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(
            mesh, unreal.Transform(), tool, unreal.Transform(), unreal.GeometryScriptBooleanOperation.SUBTRACT, bool_opts)
        log(f"carved '{name}': {n0} -> {mesh.get_triangle_count()} triangles")
    # A subtraction can only take away, and never more than the prisms hold. Anything else means a boolean went wrong.
    _area, volume_after = unreal.GeometryScript_MeshQueries.get_mesh_volume_area(mesh)
    removed = volume_before - volume_after
    log(f"volume {volume_before / 1e6:.0f} -> {volume_after / 1e6:.0f} m3, removed {removed / 1e6:.0f} m3 of the {tools_volume / 1e6:.0f} m3 inside the prisms")
    if mesh.get_triangle_count() < 1000 or volume_after <= 0.0 or removed < 0.0 or removed > tools_volume * 1.02:
        raise RuntimeError("the carved template lost more than the prisms can hold, or gained volume; not written")
    write_lod = unreal.GeometryScriptMeshWriteLOD()
    write_lod.set_editor_property("write_hi_res_source", read_lod(template)[1])
    write_lod.set_editor_property("lod_index", 0)
    write_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    for prop, value in (("enable_recompute_normals", False), ("enable_recompute_tangents", False), ("enable_remove_degenerates", False),
                        ("apply_nanite_settings", False), ("emit_transaction", False), ("replace_materials", True),
                        ("new_materials", [source.get_material(0), eal.load_asset(CUT_MATERIAL)]),
                        ("new_material_slot_names", ["Material_001", "Recess"])):
        write_opts.set_editor_property(prop, value)
    _, outcome = assets.copy_mesh_to_static_mesh(mesh, template, write_opts, write_lod)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"could not write {TEMPLATE}: {outcome}")
    eal.set_metadata_tag(template, "cathedral_cuts", SIGNATURE)
    eal.save_asset(TEMPLATE, only_if_is_dirty=False)
    log(f"template rebuilt from the original: {before} -> {mesh.get_triangle_count()} triangles, "
        f"materials {[str(m.get_editor_property('material_slot_name')) for m in template.get_editor_property('static_materials')]}")

# --- 2. the Blueprint ---------------------------------------------------------------------------
ar = unreal.AssetRegistryHelpers.get_asset_registry()
parts = sorted(str(a.asset_name) for a in ar.get_assets_by_path(FOLDER, recursive=False)
               if str(a.asset_class_path.asset_name) == "StaticMesh" and str(a.asset_name).startswith("SM_Cathedral_")
               and (USE_TEMPLATE or str(a.asset_name) != "SM_Cathedral_Template"))
bp_path = f"{FOLDER}/{BP_NAME}"
if eal.does_asset_exist(bp_path):
    bp = eal.load_asset(bp_path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, FOLDER, unreal.Blueprint, factory)
    log(f"created {bp_path}")
handles = sds.k2_gather_subobject_data_for_blueprint(bp)
existing = {}
for h in handles:
    obj = lib.get_object(lib.get_data(h))
    if isinstance(obj, unreal.StaticMeshComponent):
        existing[str(lib.get_variable_name(lib.get_data(h)))] = obj
for h in handles:                       # components whose part is gone (or the template, when it is not used)
    data = lib.get_data(h)
    if isinstance(lib.get_object(data), unreal.StaticMeshComponent) and "SM_Cathedral_" + str(lib.get_variable_name(data)) not in parts:
        log(f"removing component {lib.get_variable_name(data)}")
        sds.delete_subobject(handles[0], h, bp)
        existing.pop(str(lib.get_variable_name(data)), None)
for asset_name in parts:
    comp_name = asset_name.replace("SM_Cathedral_", "")
    comp = existing.get(comp_name)
    if comp is None:
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.StaticMeshComponent, blueprint_context=bp)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"could not add {comp_name}: {fail_reason}")
        sds.rename_subobject(handle, unreal.Text(comp_name))
        comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", eal.load_asset(f"{FOLDER}/{asset_name}"))
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_asset(bp_path, only_if_is_dirty=False)
log(f"{BP_NAME}: components {[p.replace('SM_Cathedral_', '') for p in parts]}")
log("DONE")
