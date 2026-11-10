"""Imports SM_Girona_LeftPortal.fbx into /Game/Buildings/Cathedral and sets up its materials.

Run it in the open editor (Output Log, "Python" mode):
  exec(open(r"<project>/SourceArt/Cathedral/import_left_portal_ue.py").read())
or with the editor closed:
  UnrealEditor-Cmd.exe <ZSurvivalGame.uproject> -run=pythonscript -script="<this file>" -unattended -nosplash -stdout

The portal FBX is built in the Meshy shell's own coordinate space (see build_left_portal.py),
so it is imported at the same scale as SM_GironaCathedral_Shell. Placed with the same
transform as the shell, it stands against the shell's left wall. BP_GironaCathedral
(make_cathedral_bp_ue.py) holds both meshes.

Safe to re-run after rebuilding the FBX: it replaces the mesh and keeps the materials.
"""
import os

import unreal

try:
    HERE = os.path.dirname(os.path.abspath(__file__))
except NameError:  # pasted into the Python console with exec(), where __file__ isn't set
    HERE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
                        "SourceArt", "Cathedral")
FBX = os.path.join(HERE, "SM_Girona_LeftPortal.fbx")
SHELL = "/Game/Buildings/Cathedral/SM_GironaCathedral_Shell"
DEST = "/Game/Buildings/Cathedral"
NAME = "SM_Girona_LeftPortal"
MESH_PATH = f"{DEST}/{NAME}"
SHELL_HEIGHT_BLENDER_M = 55.0          # the Meshy shell's height when the same FBX is read in Blender

# Both portal materials are instances of the city's plaster material (M_Gothic_Surface, triplanar).
PLASTER_INSTANCE = "/Game/CityArt/Materials/MI_Gothic_Cream"
STONE = "/Game/CityArt/Materials/MI_Girona_PortalStone"
BRONZE = "/Game/CityArt/Materials/MI_Girona_BronzeDoor"
# Stone: tuned against the shell's baked texture in Lvl_CathedralDistrict_Art's lighting, so the
# porch reads as the same pale limestone as the wall, a touch lighter. MI_Gothic_Cream itself is
# tan and about half as bright as the shell, which is why the portal has its own instance.
STONE_PARAMS = {"AlbedoBrightness": 0.87, "AlbedoTint": (0.76, 0.92, 1.0)}
# Door: the plaster's mottling tinted green reads as patinated bronze. (MI_Gothic_Iron can't be
# the base: its albedo is the engine's default checker texture, which shows as soon as it isn't
# tinted near-black.)
BRONZE_PARAMS = {"AlbedoBrightness": 0.42, "AlbedoTint": (0.30, 0.52, 0.34), "TilingScale": 200.0}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def log(msg):
    unreal.log(f"[LeftPortal] {msg}")


def bounds(mesh):
    bb = mesh.get_bounding_box()
    return bb.min, bb.max


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
    except Exception as exc:  # property names differ between engine versions
        log(f"could not set {prop}: {exc}")


def material_instance(path, params):
    """Creates the instance if it is missing (a copy of the plaster instance) and applies params."""
    if not eal.does_asset_exist(path):
        eal.duplicate_asset(PLASTER_INSTANCE, path)
    mat = eal.load_asset(path)
    for pname, value in params.items():
        if isinstance(value, tuple):
            mel.set_material_instance_vector_parameter_value(mat, pname, unreal.LinearColor(*value, 1.0))
        else:
            mel.set_material_instance_scalar_parameter_value(mat, pname, value)
    mel.update_material_instance(mat)
    log(f"{path}: {params}")
    return mat


# 1. Scale: match whatever scale the shell was imported at.
shell = eal.load_asset(SHELL)
smin, smax = bounds(shell)
log(f"shell bounds min=({smin.x:.0f}, {smin.y:.0f}, {smin.z:.0f}) max=({smax.x:.0f}, {smax.y:.0f}, {smax.z:.0f})")
scale = (smax.z - smin.z) / (SHELL_HEIGHT_BLENDER_M * 100.0)
log(f"import scale for the portal = {scale:.4f}")

# 2. Import. UE 5.8 ignores most of these pipeline overrides when the import goes through
#    AssetImportTask (it used the project's default pipeline: Nanite on, placeholder materials
#    created), so step 3 enforces the settings that matter on the imported asset.
pipeline = unreal.InterchangeGenericAssetsPipeline()
try_set(pipeline, "import_offset_uniform_scale", scale)
try_set(pipeline.material_pipeline, "import_materials", False)
try_set(pipeline.mesh_pipeline, "build_nanite", False)
try_set(pipeline.mesh_pipeline, "import_collision_according_to_mesh_name", True)  # UCX_ hulls

task = unreal.AssetImportTask()
task.set_editor_property("filename", FBX)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("destination_name", NAME)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", False)
task.set_editor_property("options", pipeline)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
imported = [str(p) for p in task.get_editor_property("imported_object_paths")]
log(f"imported: {imported}")

mesh = eal.load_asset(MESH_PATH)
if mesh is None:
    raise RuntimeError(f"{MESH_PATH} was not created")
pmin, pmax = bounds(mesh)
log(f"portal bounds min=({pmin.x:.0f}, {pmin.y:.0f}, {pmin.z:.0f}) max=({pmax.x:.0f}, {pmax.y:.0f}, {pmax.z:.0f})")

# 3. No Nanite: at 37k triangles it isn't needed, and Nanite's reduced fallback mesh (about
#    3.8k triangles) wrecks the thin mouldings wherever the fallback is shown.
nanite = mesh.get_editor_property("nanite_settings")
if nanite.get_editor_property("enabled"):
    nanite.set_editor_property("enabled", False)
    sub.set_nanite_settings(mesh, nanite, apply_changes=True)
log(f"nanite: {mesh.get_editor_property('nanite_settings').get_editor_property('enabled')}, "
    f"triangles LOD0: {mesh.get_num_triangles(0)}")

# 4. Materials, assigned by slot name.
stone = material_instance(STONE, STONE_PARAMS)
bronze = material_instance(BRONZE, BRONZE_PARAMS)
for pname in ("BaseColor", "Normal", "ORM"):     # an older bronze instance was based on MI_Gothic_Iron
    mel.set_material_instance_texture_parameter_value(
        bronze, pname, mel.get_material_instance_texture_parameter_value(stone, pname))
mel.update_material_instance(bronze)
for i, sm in enumerate(mesh.get_editor_property("static_materials")):
    slot = str(sm.get_editor_property("material_slot_name"))
    mat = bronze if slot.startswith("Door") else stone
    mesh.set_material(i, mat)
    log(f"slot {i} '{slot}' -> {mat.get_path_name()}")

# 5. Delete the placeholder materials the import created for the FBX's Stone/Trim/Door slots.
for obj_path in imported:
    path = obj_path.split(".")[0]
    if path in (MESH_PATH, STONE, BRONZE) or not eal.does_asset_exist(path):
        continue
    if not isinstance(eal.load_asset(path), unreal.MaterialInterface):
        continue
    refs = [r for r in eal.find_package_referencers_for_asset(path, load_assets_to_confirm=True) if r != path]
    if refs:
        log(f"kept {path}, referenced by {refs}")
    else:
        eal.delete_asset(path)
        log(f"deleted placeholder material {path}")

# 6. Collision comes from the UCX_ hulls in the FBX.
convex = sub.get_convex_collision_count(mesh)
log(f"collision: {convex} convex hulls (UCX)")
if convex == 0:
    log("WARNING: no collision was imported, the player can walk through the portal")

for path in (MESH_PATH, STONE, BRONZE):
    eal.save_asset(path, only_if_is_dirty=False)
log("DONE")
