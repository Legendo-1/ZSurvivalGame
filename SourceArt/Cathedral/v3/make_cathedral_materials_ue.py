"""Creates the materials of the rebuilt Girona cathedral.

/Game/CityArt/Materials/M_Cathedral_Stone is the master: tiling textures on the mesh's own UVs
(the build scripts lay them out at 2 m per repeat) and weathering from the vertex colours the
build scripts write:
  R  tone of the single stone block (0.5 = neutral)
  G  dirt in crevices (0 = clean)
  B  rain streaks (0 = none)
  A  large light/dark patches taken from the Meshy model (0.5 = neutral)
The instances MI_Cath_* set textures and colours per kind of surface.

Run in the open editor (Output Log, Python mode):
  exec(open(r"<project>/SourceArt/Cathedral/v3/make_cathedral_materials_ue.py").read())
Safe to re-run: the master graph is rebuilt and the instances keep their names.
"""
import unreal

FOLDER = "/Game/CityArt/Materials"
MASTER = f"{FOLDER}/M_Cathedral_Stone"
TEX = "/Game/CityArt/Textures/CC0/T_Gothic_{}_CC0_{}"

# name -> (texture set, scalar parameters, vector parameters)
INSTANCES = {
    "MI_Cath_Ashlar": ("plaster", {"AlbedoBrightness": 1.25, "TextureAmount": 0.75, "VariationAmount": 0.16, "NormalStrength": 0.9,
                                   "DirtAmount": 0.75, "RoughnessScale": 1.0},
                       {"AlbedoTint": (0.78, 0.86, 0.95), "FlatColor": (0.52, 0.49, 0.44), "DirtColor": (0.42, 0.38, 0.32)}),
    "MI_Cath_Limestone": ("plaster", {"AlbedoBrightness": 1.35, "TextureAmount": 0.55, "VariationAmount": 0.05, "NormalStrength": 0.6,
                                      "DirtAmount": 0.6, "RoughnessScale": 0.95},
                          {"AlbedoTint": (0.8, 0.87, 0.95), "FlatColor": (0.56, 0.53, 0.48), "DirtColor": (0.5, 0.46, 0.4)}),
    "MI_Cath_Marble": ("plaster", {"AlbedoBrightness": 1.3, "TextureAmount": 0.4, "VariationAmount": 0.04, "NormalStrength": 0.35,
                                   "DirtAmount": 0.7, "RoughnessScale": 0.7},
                       {"AlbedoTint": (0.85, 0.9, 0.96), "FlatColor": (0.6, 0.6, 0.6), "DirtColor": (0.42, 0.4, 0.37)}),
    "MI_Cath_Paving": ("slate", {"AlbedoBrightness": 1.1, "TextureAmount": 0.8, "VariationAmount": 0.14, "NormalStrength": 0.9,
                                 "DirtAmount": 0.6, "RoughnessScale": 0.9},
                       {"AlbedoTint": (0.9, 0.9, 0.9), "FlatColor": (0.22, 0.22, 0.22), "DirtColor": (0.5, 0.48, 0.45)}),
    "MI_Cath_Bronze": ("plaster", {"AlbedoBrightness": 0.5, "TextureAmount": 0.8, "VariationAmount": 0.1, "NormalStrength": 0.8,
                                   "DirtAmount": 0.9, "RoughnessScale": 0.75, "Metallic": 0.55, "Tiling": 3.0},
                       {"AlbedoTint": (0.26, 0.5, 0.3), "FlatColor": (0.5, 0.5, 0.5), "DirtColor": (0.35, 0.3, 0.22)}),
    "MI_Cath_BronzeRust": ("plaster", {"AlbedoBrightness": 0.42, "TextureAmount": 0.8, "VariationAmount": 0.25, "NormalStrength": 0.8,
                                       "DirtAmount": 0.8, "RoughnessScale": 0.85, "Metallic": 0.35, "Tiling": 3.0},
                           {"AlbedoTint": (0.6, 0.38, 0.16), "FlatColor": (0.5, 0.5, 0.5), "DirtColor": (0.4, 0.3, 0.2)}),
    "MI_Cath_Timber": ("timber", {"AlbedoBrightness": 0.8, "TextureAmount": 1.0, "VariationAmount": 0.1, "NormalStrength": 1.0,
                                  "DirtAmount": 0.6, "RoughnessScale": 1.0},
                       {"AlbedoTint": (0.75, 0.62, 0.5), "FlatColor": (0.2, 0.14, 0.1), "DirtColor": (0.4, 0.35, 0.3)}),
    # painted door boards (the grey door on the left side) and clay roof tiles
    "MI_Cath_PaintGrey": ("timber", {"AlbedoBrightness": 1.05, "TextureAmount": 0.45, "VariationAmount": 0.1, "NormalStrength": 0.9,
                                     "DirtAmount": 0.7, "RoughnessScale": 0.9},
                          {"AlbedoTint": (0.62, 0.68, 0.74), "FlatColor": (0.45, 0.47, 0.5), "DirtColor": (0.4, 0.38, 0.35)}),
    "MI_Cath_Tile": ("slate", {"AlbedoBrightness": 1.5, "TextureAmount": 0.55, "VariationAmount": 0.3, "NormalStrength": 0.8,
                               "DirtAmount": 0.75, "RoughnessScale": 0.95},
                     {"AlbedoTint": (0.95, 0.56, 0.36), "FlatColor": (0.4, 0.4, 0.4), "DirtColor": (0.45, 0.36, 0.3)}),
    "MI_Cath_Iron": ("slate", {"AlbedoBrightness": 0.35, "TextureAmount": 0.6, "VariationAmount": 0.05, "NormalStrength": 0.6,
                               "DirtAmount": 0.5, "RoughnessScale": 0.7, "Metallic": 0.85},
                     {"AlbedoTint": (0.5, 0.5, 0.52), "FlatColor": (0.1, 0.1, 0.1), "DirtColor": (0.5, 0.4, 0.3)}),
}
# Window glass: an instance of the city's glass material, dark with a faint warm glow from inside.
GLASS = ("MI_Cath_Glass", "/Game/CityArt/Materials/M_Gothic_Glass",
         {"EmissiveStrength": 0.03, "Roughness": 0.12, "Specular": 0.6},
         {"GlassColor": (0.02, 0.03, 0.05), "EmissiveColor": (1.0, 0.62, 0.3)})

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log(f"[CathMaterials] {msg}")


# --- master ---------------------------------------------------------------------------------------
if eal.does_asset_exist(MASTER):
    mat = eal.load_asset(MASTER)
    mel.delete_all_material_expressions(mat)
else:
    mat = tools.create_asset("M_Cathedral_Stone", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
_col = [0]


def node(cls, **props):
    _col[0] += 1
    e = mel.create_material_expression(mat, cls, -2200 + 130 * (_col[0] % 14), -500 + 170 * (_col[0] // 14))
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, out, b, inp):
    if not mel.connect_material_expressions(a, out, b, inp):
        raise RuntimeError(f"could not connect {a.get_name()}.{out} -> {b.get_name()}.{inp}")


def scalar(name, default, group="Surface"):
    return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default, group=group)


def vector(name, rgb, group="Surface"):
    return node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.0), group=group)


def mul(a, aout, b, bout):
    m = node(unreal.MaterialExpressionMultiply)
    link(a, aout, m, "A")
    link(b, bout, m, "B")
    return m


def lerp(a, aout, b, bout, alpha, alpha_out):
    m = node(unreal.MaterialExpressionLinearInterpolate)
    link(a, aout, m, "A")
    link(b, bout, m, "B")
    link(alpha, alpha_out, m, "Alpha")
    return m


def texture(name, sampler, default_tex, uv):
    t = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, sampler_type=sampler,
             texture=eal.load_asset(default_tex), group="Textures")
    link(uv, "", t, "UVs")
    return t


uv = mul(node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0), "", scalar("Tiling", 1.0), "")
base = texture("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, TEX.format("plaster", "Albedo"), uv)
nrm = texture("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, TEX.format("plaster", "Normal"), uv)
orm = texture("ORM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, TEX.format("plaster", "ORM"), uv)
vc = node(unreal.MaterialExpressionVertexColor)

# colour: texture pulled towards a flat colour, tinted, then tone and large patches from the vertex colours
col = lerp(vector("FlatColor", (0.5, 0.5, 0.5)), "", base, "RGB", scalar("TextureAmount", 1.0), "")
col = mul(col, "", vector("AlbedoTint", (1, 1, 1)), "")
col = mul(col, "", scalar("AlbedoBrightness", 1.0), "")
for channel, amount in (("R", scalar("VariationAmount", 0.12, "Weathering")), ("A", scalar("MacroAmount", 0.25, "Weathering"))):
    swing = node(unreal.MaterialExpressionConstantBiasScale, bias=-0.5, scale=2.0)      # 0..1 -> -1..1
    link(vc, channel, swing, "")
    factor = node(unreal.MaterialExpressionAdd, const_b=1.0)
    link(mul(swing, "", amount, ""), "", factor, "A")
    col = mul(col, "", factor, "")
# dirt in crevices and rain streaks darken towards their own colour
dirt = node(unreal.MaterialExpressionSaturate)
link(mul(vc, "G", scalar("DirtAmount", 0.7, "Weathering"), ""), "", dirt, "")
col = lerp(col, "", mul(col, "", vector("DirtColor", (0.4, 0.36, 0.3), "Weathering"), ""), "", dirt, "")
streak = node(unreal.MaterialExpressionSaturate)
link(mul(vc, "B", scalar("StreakAmount", 0.6, "Weathering"), ""), "", streak, "")
col = lerp(col, "", mul(col, "", vector("StreakColor", (0.55, 0.52, 0.47), "Weathering"), ""), "", streak, "")
mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

rough = node(unreal.MaterialExpressionSaturate)
rough_sum = node(unreal.MaterialExpressionAdd)
link(mul(orm, "G", scalar("RoughnessScale", 1.0), ""), "", rough_sum, "A")
link(mul(dirt, "", node(unreal.MaterialExpressionConstant, r=0.12), ""), "", rough_sum, "B")
link(rough_sum, "", rough, "")
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(scalar("Metallic", 0.0), "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
flat = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.0, 0.0, 1.0, 0.0))
mel.connect_material_property(lerp(flat, "", nrm, "RGB", scalar("NormalStrength", 1.0), ""), "", unreal.MaterialProperty.MP_NORMAL)

mel.recompile_material(mat)
eal.save_asset(MASTER, only_if_is_dirty=False)
log(f"master {MASTER}: {mel.get_num_material_expressions(mat)} nodes, scalars {[str(n) for n in mel.get_scalar_parameter_names(mat)]}")

# --- instances ------------------------------------------------------------------------------------
for name, (tex, scalars, vectors) in INSTANCES.items():
    path = f"{FOLDER}/{name}"
    if eal.does_asset_exist(path):
        mi = eal.load_asset(path)
    else:
        mi = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    for pname, suffix in (("BaseColor", "Albedo"), ("Normal", "Normal"), ("ORM", "ORM")):
        mel.set_material_instance_texture_parameter_value(mi, pname, eal.load_asset(TEX.format(tex, suffix)))
    for pname, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, pname, value)
    for pname, value in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, pname, unreal.LinearColor(*value, 1.0))
    mel.update_material_instance(mi)
    eal.save_asset(path, only_if_is_dirty=False)
    log(f"{name}: textures '{tex}', {scalars}")
name, parent, scalars, vectors = GLASS
path = f"{FOLDER}/{name}"
mi = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi, eal.load_asset(parent))
for pname, value in scalars.items():
    mel.set_material_instance_scalar_parameter_value(mi, pname, value)
for pname, value in vectors.items():
    mel.set_material_instance_vector_parameter_value(mi, pname, unreal.LinearColor(*value, 1.0))
mel.update_material_instance(mi)
eal.save_asset(path, only_if_is_dirty=False)
log(f"{name}: glass, {scalars}")
log("DONE")
