"""Measuring pictures of one Meshy house, for the per-house routine (step 2).

Run headless with Blender:
  blender.exe -b --factory-startup --python measure_meshy.py -- <Folder> <out folder> [--scale S] [--ppm 100]
      [--slice name:axis:value:u0:u1:v0:v1]...

It imports C:/Users/Lenovo/Downloads/MeshyHouses/<Folder>/model.glb, moves it into house space (section 3 of the
ledger: front-left corner of the footprint at the origin, front face on y = 0, the house towards +y, z = 0 at the
bottom) and scales it by S (default: 1, i.e. Meshy units). Then it writes into <out folder>:
  m_front.png, m_back.png, m_left.png, m_right.png   unlit base colour, orthographic, at `ppm` pixels per metre,
                                                     with a grid line every 1 m (and a stronger one every 5 m)
  m_info.json                                        bounds before and after, the scale and the picture origins
  <name>.png                                         sections through the mesh (template_slices.py format), if asked
Picture coordinates: column = (u - u0) * ppm, row = (v1 - v) * ppm, with u0 / v1 in m_info.json for each view.
The GLB is a downloaded file and is only read as data.
"""
import json
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

ROOT = r"C:\Users\Lenovo\Downloads\MeshyHouses"
argv = sys.argv[sys.argv.index("--") + 1:]
folder, out = argv[0], argv[1]
scale, ppm, slices = 1.0, 100.0, []
i = 2
while i < len(argv):
    if argv[i] == "--scale":
        scale = float(argv[i + 1]); i += 2
    elif argv[i] == "--ppm":
        ppm = float(argv[i + 1]); i += 2
    elif argv[i] == "--slice":
        slices.append(argv[i + 1]); i += 2
    else:
        raise SystemExit(f"unknown argument {argv[i]}")
os.makedirs(out, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, folder, "model.glb"))
sc = bpy.context.scene
meshes = [o for o in sc.objects if o.type == 'MESH']
bpy.context.view_layer.update()


def bounds():
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for o in meshes:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo, hi = Vector(map(min, lo, w)), Vector(map(max, hi, w))
    return lo, hi


lo0, hi0 = bounds()
# parent everything to one empty that moves the model into house space and scales it
root = bpy.data.objects.new("root", None)
sc.collection.objects.link(root)
for o in meshes:
    if o.parent is None:
        o.parent = root
root.scale = (scale,) * 3
root.location = (-lo0.x * scale, -lo0.y * scale, -lo0.z * scale)
bpy.context.view_layer.update()
lo, hi = bounds()
info = {"folder": folder, "scale": scale, "meshy_bounds": [list(lo0), list(hi0)], "house_bounds": [list(lo), list(hi)],
        "size_m": list(hi - lo), "ppm": ppm, "views": {}}
print(f"MEASURE {folder}: meshy bounds {tuple(round(v, 4) for v in lo0)} .. {tuple(round(v, 4) for v in hi0)}, "
      f"at scale {scale}: size {tuple(round(v, 3) for v in hi - lo)}")

# --- orthographic base-colour views ----------------------------------------------------------------
sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading
sh.light, sh.color_type = 'FLAT', 'TEXTURE'
sh.show_shadows = sh.show_cavity = sh.show_object_outline = False
sc.display_settings.display_device = "sRGB"
sc.view_settings.view_transform = "Standard"
sc.world = bpy.data.worlds.new("w")
sc.world.color = (0.18, 0.18, 0.2)
sh.background_type = 'WORLD'
cd = bpy.data.cameras.new("cam")
cd.type = 'ORTHO'
cam = bpy.data.objects.new("cam", cd)
sc.collection.objects.link(cam)
sc.camera = cam
ctr, ext = (lo + hi) / 2, hi - lo
pad = 0.5
# view: (direction the camera looks, picture u axis in house space, which coordinate u reads, sign)
VIEWS = {"front": (Vector((0, 1, 0)), 0, 1), "back": (Vector((0, -1, 0)), 0, -1),
         "left": (Vector((1, 0, 0)), 1, -1), "right": (Vector((-1, 0, 0)), 1, 1)}
for name, (look, axis, sgn) in VIEWS.items():
    span_u = ext[axis] + 2 * pad
    span_v = ext.z + 2 * pad
    W, H = int(round(span_u * ppm)), int(round(span_v * ppm))
    sc.render.resolution_x, sc.render.resolution_y = W, H
    cd.ortho_scale = max(span_u, span_v)
    cd.sensor_fit = 'HORIZONTAL' if span_u >= span_v else 'VERTICAL'
    cam.location = ctr - look * (ext.length * 3)
    cam.rotation_euler = look.to_track_quat('-Z', 'Y').to_euler()
    cd.clip_end = ext.length * 8
    path = os.path.join(out, f"m_{name}.png")
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    # picture u runs along +axis for sgn 1, along -axis for sgn -1
    u_left = (ctr[axis] - sgn * span_u / 2)
    info["views"][name] = {"file": path, "w": W, "h": H, "u_axis": "xyz"[axis], "u_sign": sgn,
                           "u_at_col0": round(u_left, 4), "z_at_row0": round(ctr.z + span_v / 2, 4)}
    # grid overlay: every metre of house space
    img = bpy.data.images.load(path)
    px = np.empty(W * H * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(H, W, 4)                 # rows from the bottom
    for g in range(int(math.floor(lo[axis])) - 1, int(math.ceil(hi[axis])) + 2):
        c = int(round((g - u_left) * sgn * ppm))
        if 0 <= c < W:
            px[:, c, :3] = (1.0, 0.25, 0.25) if g % 5 == 0 else (0.9, 0.6, 0.2)
    zb = ctr.z - span_v / 2
    for g in range(int(math.floor(lo.z)) - 1, int(math.ceil(hi.z)) + 2):
        r = int(round((g - zb) * ppm))
        if 0 <= r < H:
            px[r, :, :3] = (0.3, 0.5, 1.0) if g % 5 == 0 else (0.3, 0.8, 0.9)
    img.pixels.foreach_set(px.ravel())
    img.filepath_raw = os.path.join(out, f"m_{name}_grid.png")
    img.file_format = 'PNG'
    img.save()
    bpy.data.images.remove(img)

# --- sections ---------------------------------------------------------------------------------------
if slices:
    import OpenImageIO as oiio
    co_all, tri_all = [], []
    n = 0
    for ob in meshes:
        me = ob.data
        me.calc_loop_triangles()
        co = np.empty(len(me.vertices) * 3, dtype=np.float32)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3).astype(np.float64)
        mw = np.array(ob.matrix_world)
        co_all.append(co @ mw[:3, :3].T + mw[:3, 3])
        t = np.empty(len(me.loop_triangles) * 3, dtype=np.int32)
        me.loop_triangles.foreach_get("vertices", t)
        tri_all.append(t.reshape(-1, 3) + n)
        n += len(co)
    co, tri = np.concatenate(co_all), np.concatenate(tri_all)
    P = co[tri]
    AX = {"x": (0, 1, 2), "y": (1, 0, 2), "z": (2, 0, 1)}
    for spec in slices:
        sname, axis, value, u0, u1, v0, v1 = spec.split(":")
        value, u0, u1, v0, v1 = map(float, (value, u0, u1, v0, v1))
        a, iu, iv = AX[axis]
        d = P[:, :, a] - value
        cross = (d.min(axis=1) < 0) & (d.max(axis=1) > 0)
        Q, D = P[cross], d[cross]
        pts = []
        for i0, j0 in ((0, 1), (1, 2), (2, 0)):
            m = (D[:, i0] * D[:, j0]) < 0
            t = D[:, i0] / (D[:, i0] - D[:, j0] + 1e-30)
            pt = Q[:, i0] + (Q[:, j0] - Q[:, i0]) * t[:, None]
            pts.append(np.where(m[:, None], pt, np.nan))
        pts = np.stack(pts, axis=1)
        order = np.argsort(np.isnan(pts[:, :, 0]), axis=1, kind="stable")
        pts = np.take_along_axis(pts, order[:, :, None], axis=1)[:, :2]
        A, Bp = pts[:, 0], pts[:, 1]
        W, H = int(round((u1 - u0) * ppm)), int(round((v1 - v0) * ppm))
        img = np.full((H, W, 3), 0.10, dtype=np.float32)
        for g in range(int(np.ceil(u0 * 10)), int(np.floor(u1 * 10)) + 1):
            c = int(round((g / 10 - u0) * ppm))
            if 0 <= c < W:
                img[:, c] = (0.45, 0.2, 0.2) if g % 10 == 0 else (0.16, 0.16, 0.16)
        for g in range(int(np.ceil(v0 * 10)), int(np.floor(v1 * 10)) + 1):
            r = H - 1 - int(round((g / 10 - v0) * ppm))
            if 0 <= r < H:
                img[r, :] = (0.2, 0.3, 0.55) if g % 10 == 0 else (0.16, 0.16, 0.16)
        for s in np.linspace(0.0, 1.0, 16):
            p = A + (Bp - A) * s
            cu = np.round((p[:, iu] - u0) * ppm).astype(int)
            rv = H - 1 - np.round((p[:, iv] - v0) * ppm).astype(int)
            ok = (cu >= 0) & (cu < W) & (rv >= 0) & (rv < H)
            img[rv[ok], cu[ok]] = (1.0, 1.0, 1.0)
        buf = oiio.ImageBuf(oiio.ImageSpec(W, H, 3, oiio.UINT8))
        buf.set_pixels(oiio.ROI(0, W, 0, H), img)
        buf.write(os.path.join(out, f"{sname}.png"))
        print(f"SLICE {sname}: {axis} = {value}, {int(cross.sum())} triangles cut, {W} x {H} px (grid 0.1 m, strong 1 m)")

json.dump(info, open(os.path.join(out, "m_info.json"), "w"), indent=1)
print("MEASURE done ->", out)
