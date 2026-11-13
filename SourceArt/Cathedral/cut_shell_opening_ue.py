"""Cuts the opening for the side portal into SM_GironaCathedral_Shell.

The portal's arches recede 2.8 m, most of it behind the chapel wall face (see build_side_portal.py),
so the piece of the shell's wall inside the arch has to go. The edge of the opening is hidden behind
the porch masonry, so a plain box is enough: everything of the shell inside the box is trimmed away
and nothing is added.

Run in the open editor (Output Log, Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/cut_shell_opening_ue.py").read())

Safe to re-run: when nothing of the shell is left inside the box it changes nothing.
To undo, restore Content/Buildings/Cathedral/SM_GironaCathedral_Shell.uasset from git.
Needs the Geometry Script plugin (enabled with the Modeling Tools editor mode).
"""
import unreal

SHELL = "/Game/Buildings/Cathedral/SM_GironaCathedral_Shell"

# The opening in the coordinates of build_side_portal.py (Blender space of the shell, metres):
# 4 m either side of the arch centre XC, from below the ground to 8.9 m, through the chapel's outer
# wall only (its face is at Y 13.52 to 13.64).
XC = 32.7
HALF_WIDTH = 4.0
Z_BOTTOM, Z_TOP = -1.0, 8.9
Y_INNER, Y_OUTER = 12.0, 14.6

eal = unreal.EditorAssetLibrary
assets = unreal.GeometryScript_AssetUtils
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(msg):
    unreal.log(f"[ShellOpening] {msg}")


def describe(mesh):
    bb = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(mesh)
    return (f"{mesh.get_triangle_count()} triangles, bounds ({bb.min.x:.0f}, {bb.min.y:.0f}, {bb.min.z:.0f}) "
            f"to ({bb.max.x:.0f}, {bb.max.y:.0f}, {bb.max.z:.0f})")


shell = eal.load_asset(SHELL)

# The asset is in Unreal units: centimetres, with Y mirrored against Blender.
x0, x1 = (XC - HALF_WIDTH) * 100.0, (XC + HALF_WIDTH) * 100.0
y0, y1 = -Y_OUTER * 100.0, -Y_INNER * 100.0
z0, z1 = Z_BOTTOM * 100.0, Z_TOP * 100.0

# Read the full-detail source mesh (the render mesh of a Nanite asset is only its coarse fallback).
hi_res = assets.get_num_static_mesh_lods_of_type(shell, unreal.GeometryScriptLODType.HI_RES_SOURCE_MODEL) > 0
read_lod = unreal.GeometryScriptMeshReadLOD()
read_lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.HI_RES_SOURCE_MODEL if hi_res
                             else unreal.GeometryScriptLODType.SOURCE_MODEL)
read_lod.set_editor_property("lod_index", 0)
read_opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
read_opts.set_editor_property("apply_build_settings", False)
read_opts.set_editor_property("request_tangents", True)
mesh = unreal.DynamicMesh()
_, outcome = assets.copy_mesh_from_static_mesh(shell, mesh, read_opts, read_lod)
if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
    raise RuntimeError(f"could not read {SHELL}: {outcome}")
log(f"source ({'hi-res' if hi_res else 'LOD0'}): {describe(mesh)}")
before = mesh.get_triangle_count()

tool = unreal.DynamicMesh()
unreal.GeometryScript_Primitives.append_box(
    tool, unreal.GeometryScriptPrimitiveOptions(),
    unreal.Transform(location=unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)),
    dimension_x=x1 - x0, dimension_y=y1 - y0, dimension_z=z1 - z0,
    origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
bool_opts = unreal.GeometryScriptMeshBooleanOptions()
bool_opts.set_editor_property("fill_holes", False)
bool_opts.set_editor_property("simplify_output", False)
unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(
    mesh, unreal.Transform(), tool, unreal.Transform(),
    unreal.GeometryScriptBooleanOperation.TRIM_INSIDE, bool_opts)
after = mesh.get_triangle_count()
log(f"after trimming the box X {x0:.0f}..{x1:.0f}, Y {y0:.0f}..{y1:.0f}, Z {z0:.0f}..{z1:.0f}: {describe(mesh)}")

if after == before:
    log("nothing of the shell is inside the box: already cut, asset left as it is")
else:
    write_lod = unreal.GeometryScriptMeshWriteLOD()
    write_lod.set_editor_property("write_hi_res_source", hi_res)
    write_lod.set_editor_property("lod_index", 0)
    write_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
    for prop, value in (("enable_recompute_normals", False), ("enable_recompute_tangents", False),
                        ("enable_remove_degenerates", False), ("replace_materials", False),
                        ("apply_nanite_settings", False), ("emit_transaction", False)):
        write_opts.set_editor_property(prop, value)
    _, outcome = assets.copy_mesh_to_static_mesh(mesh, shell, write_opts, write_lod)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError(f"could not write {SHELL}: {outcome}")
    eal.save_asset(SHELL, only_if_is_dirty=False)
    log(f"wrote the shell with the opening ({before} -> {after} triangles) and saved it")

bb = shell.get_bounding_box()
log(f"asset: bounds ({bb.min.x:.0f}, {bb.min.y:.0f}, {bb.min.z:.0f}) to ({bb.max.x:.0f}, {bb.max.y:.0f}, {bb.max.z:.0f}), "
    f"nanite {shell.get_editor_property('nanite_settings').get_editor_property('enabled')}, "
    f"{sub.get_convex_collision_count(shell)} convex hull(s), "
    f"materials {[str(m.get_editor_property('material_slot_name')) for m in shell.get_editor_property('static_materials')]}")
log("DONE")
