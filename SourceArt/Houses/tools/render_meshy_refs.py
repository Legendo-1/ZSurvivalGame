"""Renders reference pictures and a colour palette for each downloaded Meshy house.

Run headless with Blender:
  blender.exe -b --factory-startup --python render_meshy_refs.py -- [--only Folder1,Folder2] [--size 640]

For each entry of C:/Users/Lenovo/Downloads/MeshyHouses/_index.json it imports <Folder>/model.glb and writes,
into that same folder:
  ref_front.png, ref_right.png, ref_back.png, ref_left.png, ref_iso.png   unlit base colour, orthographic
  palette.json   k-means clusters of the base-colour texture: linear RGB, sRGB hex and share of texels
  palette.png    swatch strip (widest share first)
and finally MeshyHouses/_contact_sheet.png (iso views, 6 columns, in _index.json order, left to right).

Orientation: Meshy puts the input image's front on glTF +Z, which Blender imports as -Y, so "front" looks from
-Y toward +Y (the same convention as the cathedral template). Models are normalised to about 1.9 units at their
largest side; real scale is set per house from storey heights.
The GLBs are downloaded, untrusted files: they are only read as data (no scripts run from them).
"""
import bpy, json, math, os, sys
import numpy as np
from mathutils import Vector

ROOT = r"C:\Users\Lenovo\Downloads\MeshyHouses"
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ONLY = None
SIZE = 640
SHEET_ONLY = None  # --sheet front : only rebuild _contact_sheet_front.png from existing ref_front.png files
for i, a in enumerate(argv):
    if a == "--only":
        ONLY = set(argv[i + 1].split(","))
    if a == "--size":
        SIZE = int(argv[i + 1])
    if a == "--sheet":
        SHEET_ONLY = argv[i + 1]

VIEWS = {  # name: (camera direction from the model centre, up)
    "front": Vector((0, -1, 0)),
    "right": Vector((1, 0, 0)),
    "back": Vector((0, 1, 0)),
    "left": Vector((-1, 0, 0)),
    "iso": Vector((0.75, -1.0, 0.65)).normalized(),
}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light = "FLAT"
    sh.color_type = "TEXTURE"
    sh.show_shadows = False
    sh.show_cavity = False
    sh.show_object_outline = False
    sc.display_settings.display_device = "sRGB"
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new("w")
    sc.world.color = (0.18, 0.18, 0.2)
    sh.background_type = "WORLD"
    sc.render.resolution_x = sc.render.resolution_y = SIZE
    sc.render.image_settings.file_format = "PNG"
    return sc


def bbox(objs):
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w))
            hi = Vector(map(max, hi, w))
    return lo, hi


def render_views(sc, folder, lo, hi):
    ctr = (lo + hi) / 2
    ext = hi - lo
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    dist = ext.length * 3
    cam_data.clip_end = dist * 3
    for name, d in VIEWS.items():
        cam.location = ctr + d * dist
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        if name in ("front", "back"):
            span = max(ext.x, ext.z)
        elif name in ("right", "left"):
            span = max(ext.y, ext.z)
        else:
            span = ext.length * 0.9
        cam_data.ortho_scale = span * 1.08
        sc.render.filepath = os.path.join(folder, f"ref_{name}.png")
        bpy.ops.render.render(write_still=True)


def srgb_to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def palette(folder, k=8):
    png = os.path.join(folder, "model_textures", "base_color.png")
    img = bpy.data.images.load(png)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)[::8, ::8, :3].reshape(-1, 3)  # 512x512 sample, sRGB-encoded floats
    bpy.data.images.remove(img)
    keep = px.sum(1) > 0.02  # drop pure-black atlas padding
    px = px[keep]
    rng = np.random.default_rng(7)
    cen = px[rng.choice(len(px), k, replace=False)]
    for _ in range(25):
        lab = np.argmin(((px[:, None, :] - cen[None]) ** 2).sum(2), 1)
        for j in range(k):
            m = px[lab == j]
            if len(m):
                cen[j] = m.mean(0)
    share = np.bincount(lab, minlength=k) / len(lab)
    order = np.argsort(-share)
    out = []
    for j in order:
        s = cen[j]
        lin = srgb_to_linear(s)
        out.append({"share": round(float(share[j]), 3),
                    "srgb_hex": "#%02x%02x%02x" % tuple(int(round(v * 255)) for v in s),
                    "linear": [round(float(v), 3) for v in lin]})
    json.dump(out, open(os.path.join(folder, "palette.json"), "w"), indent=1)
    # swatch strip: width proportional to share, 1024 x 96
    W, H = 1024, 96
    strip = np.zeros((H, W, 4), dtype=np.float32)
    strip[..., 3] = 1
    x = 0
    for i, j in enumerate(order):
        x2 = W if i == len(order) - 1 else x + max(1, int(round(share[j] * W)))
        strip[:, x:x2, :3] = cen[j]
        x = x2
    si = bpy.data.images.new("strip", W, H)
    si.pixels.foreach_set(strip.ravel())
    si.filepath_raw = os.path.join(folder, "palette.png")
    si.file_format = "PNG"
    si.save()
    bpy.data.images.remove(si)
    return out


def contact_sheet(entries, cols=6, cell=320, view="iso"):
    rows = math.ceil(len(entries) / cols)
    sheet = np.full((rows * cell, cols * cell, 4), 0.1, dtype=np.float32)
    sheet[..., 3] = 1
    for i, e in enumerate(entries):
        p = os.path.join(ROOT, e["folder"], f"ref_{view}.png")
        if not os.path.exists(p):
            continue
        img = bpy.data.images.load(p)
        w, h = img.size
        a = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(a)
        a = a.reshape(h, w, 4)
        step = max(1, w // cell)
        a = a[::step, ::step][:cell, :cell]
        r, c = divmod(i, cols)
        y0 = (rows - 1 - r) * cell  # Blender pixel rows start at the bottom
        sheet[y0:y0 + a.shape[0], c * cell:c * cell + a.shape[1]] = a
        bpy.data.images.remove(img)
    si = bpy.data.images.new("sheet", cols * cell, rows * cell)
    si.pixels.foreach_set(sheet.ravel())
    si.filepath_raw = os.path.join(ROOT, "_contact_sheet.png" if view == "iso" else f"_contact_sheet_{view}.png")
    si.file_format = "PNG"
    si.save()


def main():
    entries = json.load(open(os.path.join(ROOT, "_index.json"), encoding="utf-8"))
    if SHEET_ONLY:
        reset()
        contact_sheet(entries, view=SHEET_ONLY)
        print("SHEET", SHEET_ONLY)
        return
    for e in entries:
        if ONLY and e["folder"] not in ONLY:
            continue
        folder = os.path.join(ROOT, e["folder"])
        sc = reset()
        bpy.ops.import_scene.gltf(filepath=os.path.join(folder, "model.glb"))
        meshes = [o for o in sc.objects if o.type == "MESH"]
        lo, hi = bbox(meshes)
        render_views(sc, folder, lo, hi)
        pal = palette(folder)
        ext = hi - lo
        print(f"DONE {e['n']:2d} {e['folder']}  extent(x,y,z)=({ext.x:.3f},{ext.y:.3f},{ext.z:.3f})  "
              f"top={pal[0]['srgb_hex']}:{pal[0]['share']}", flush=True)
    reset()
    contact_sheet(entries)
    print("SHEET", os.path.join(ROOT, "_contact_sheet.png"))


main()
