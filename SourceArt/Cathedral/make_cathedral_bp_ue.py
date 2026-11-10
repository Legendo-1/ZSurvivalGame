"""Creates /Game/Buildings/Cathedral/BP_GironaCathedral: the Meshy shell plus the left portal in one actor.

Both meshes share one coordinate space (see build_left_portal.py), so both components sit at the
actor's origin with no offset. Drag the Blueprint into a level to place the whole cathedral.

Run in the open editor (Output Log, Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/make_cathedral_bp_ue.py").read())
"""
import unreal

FOLDER = "/Game/Buildings/Cathedral"
BP_NAME = "BP_GironaCathedral"
BP_PATH = f"{FOLDER}/{BP_NAME}"
PARTS = [("Shell", f"{FOLDER}/SM_GironaCathedral_Shell"), ("LeftPortal", f"{FOLDER}/SM_Girona_LeftPortal")]

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log(f"[CathedralBP] {msg}")


if eal.does_asset_exist(BP_PATH):
    bp = eal.load_asset(BP_PATH)
    log("blueprint already exists, updating its components")
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, FOLDER, unreal.Blueprint, factory)
    log(f"created {bp.get_path_name()}")

handles = sds.k2_gather_subobject_data_for_blueprint(bp)
existing = {}
for h in handles:
    obj = lib.get_object(lib.get_data(h))
    if isinstance(obj, unreal.StaticMeshComponent):
        existing[str(lib.get_variable_name(lib.get_data(h)))] = obj
root = handles[0]

for comp_name, mesh_path in PARTS:
    mesh = eal.load_asset(mesh_path)
    comp = existing.get(comp_name)
    if comp is None:
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent,
                                              blueprint_context=bp)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"could not add {comp_name}: {fail_reason}")
        sds.rename_subobject(handle, unreal.Text(comp_name))
        comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", mesh)
    comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    comp.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
    log(f"component {comp_name} -> {mesh_path}")

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
eal.save_asset(BP_PATH, only_if_is_dirty=False)

handles = sds.k2_gather_subobject_data_for_blueprint(bp)
for h in handles:
    d = lib.get_data(h)
    obj = lib.get_object(d)
    sm = obj.get_editor_property("static_mesh") if isinstance(obj, unreal.StaticMeshComponent) else None
    log(f"  {lib.get_variable_name(d)} ({obj.get_class().get_name() if obj else None}) mesh={sm.get_name() if sm else None}")
log("DONE")
