"""Creates (or updates) the material instances of one house: /Game/Buildings/Houses/<Folder>/Materials/MI_House_<Folder>_<Surface>.

Run through SourceArt/Houses/tools/ue_house.py (it sets HOUSE_NAME). Reads SourceArt/Houses/specs/<Folder>.json,
section "materials": one entry per FBX slot (house_kit.MATERIALS), e.g.
  "Brick": {"tex": "brick", "target": [0.082, 0.041, 0.027], "gain": 1.25, "ta": 0.9, "normal": 1.0}
  tex       the CC0 set (/Game/CityArt/Textures/CC0/T_Gothic_<tex>_CC0_*): ashlar brick cobble plaster slate timber
  target    the surface's mean linear base colour measured on the Meshy model (tools/color_match.py)
  gain      a correction found by comparing SCS_BASE_COLOR captures with the Meshy picture (weathering darkens a
            surface; the gain makes up for it), one number or [r, g, b]. AlbedoTint = target * gain / (texture mean)
  ta, normal, rough, metal, dirt, streak, variation, macro, tiling: TextureAmount, NormalStrength, RoughnessScale,
            Metallic, DirtAmount, StreakAmount, VariationAmount, MacroAmount, Tiling of M_Cathedral_Stone
  "glass": true makes an instance of M_Gothic_Glass with GlassColor = target * gain.
FlatColor (what the texture is pulled towards when ta < 1) is the texture's own mean colour, so ta only changes the
contrast of the texture, never the average.
"""
import json
import os

import unreal

FOLDER = HOUSE_NAME  # noqa: F821 (set by ue_house.py)
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
spec = json.load(open(os.path.join(PROJECT, "SourceArt", "Houses", "specs", FOLDER + ".json"), encoding="utf-8"))
DEST = f"/Game/Buildings/Houses/{FOLDER}/Materials"
MASTER = "/Game/CityArt/Materials/M_Cathedral_Stone"
GLASS = "/Game/CityArt/Materials/M_Gothic_Glass"
TEX = "/Game/CityArt/Textures/CC0/T_Gothic_{}_CC0_{}"
# Mean linear colour of each CC0 albedo, as the material samples it (measured 2026-11-13). Careful when measuring
# again: Unreal exports these sRGB textures as 16-bit PNGs that hold LINEAR values (take the raw mean), but plaster
# as an 8-bit TGA holding sRGB values (decode it first). Checked against SCS_BASE_COLOR captures of house #1.
TEX_MEAN = {"brick": (0.5094, 0.3878, 0.3123), "plaster": (0.59, 0.4442, 0.2933), "cobble": (0.4216, 0.387, 0.3447),
            "timber": (0.3132, 0.2644, 0.2302), "slate": (0.4428, 0.4331, 0.4071), "ashlar": (0.5766, 0.4692, 0.372)}
PARAMS = {"ta": "TextureAmount", "normal": "NormalStrength", "rough": "RoughnessScale", "metal": "Metallic", "dirt": "DirtAmount",
          "streak": "StreakAmount", "variation": "VariationAmount", "macro": "MacroAmount", "tiling": "Tiling"}

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"[HouseMaterials] {msg}")


def instance(name, parent):
    path = f"{DEST}/{name}"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, eal.load_asset(parent))
    return path, mi


for surface, m in spec["materials"].items():
    name = f"MI_House_{FOLDER}_{surface}"
    gain = m.get("gain", 1.0)                         # one number, or one per channel
    gain = list(gain) if isinstance(gain, (list, tuple)) else [gain] * 3
    target = [c * g for c, g in zip(m["target"], gain)]
    if m.get("glass"):
        path, mi = instance(name, GLASS)
        mel.set_material_instance_vector_parameter_value(mi, "GlassColor", unreal.LinearColor(*target, 1.0))
        mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", unreal.LinearColor(1.0, 0.62, 0.3, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", m.get("emissive", 0.02))
        mel.set_material_instance_scalar_parameter_value(mi, "Roughness", m.get("rough", 0.12))
        mel.set_material_instance_scalar_parameter_value(mi, "Specular", m.get("specular", 0.6))
    else:
        path, mi = instance(name, MASTER)
        tex = m["tex"]
        mean = TEX_MEAN[tex]
        tint = [t / max(c, 1e-4) for t, c in zip(target, mean)]
        for pname, suffix in (("BaseColor", "Albedo"), ("Normal", "Normal"), ("ORM", "ORM")):
            mel.set_material_instance_texture_parameter_value(mi, pname, eal.load_asset(TEX.format(tex, suffix)))
        mel.set_material_instance_vector_parameter_value(mi, "AlbedoTint", unreal.LinearColor(*tint, 1.0))
        mel.set_material_instance_vector_parameter_value(mi, "FlatColor", unreal.LinearColor(*mean, 1.0))
        mel.set_material_instance_scalar_parameter_value(mi, "AlbedoBrightness", 1.0)
        for key, pname in PARAMS.items():
            if key in m:
                mel.set_material_instance_scalar_parameter_value(mi, pname, m[key])
        if "dirt_color" in m:
            mel.set_material_instance_vector_parameter_value(mi, "DirtColor", unreal.LinearColor(*m["dirt_color"], 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    log(f"{name}: target {[round(c, 3) for c in m['target']]} x gain {gain}")
log("DONE")
