"""Depth profiles of a Meshy house in house space: rays along +y (from the front) or -y (from the back) at chosen
x columns, printing where they first hit the model. Gives wall-face, cornice, surround and hood depths as numbers.

Run headless with Blender (same placement as measure_meshy.py: front-left corner of the bounds at the origin):
  blender.exe -b --factory-startup --python raycast_meshy.py -- <Folder> --scale S --x 0.3,4.0 [--z0 0 --z1 18 --step 0.05]
      [--from back] [--along x --y 3.0]   (--along x: rays along +x at depth y, for side-wall faces)
The GLB is a downloaded file and is only read as data.
"""
import os
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = r"C:\Users\Lenovo\Downloads\MeshyHouses"
argv = sys.argv[sys.argv.index("--") + 1:]
folder = argv[0]
o = {argv[i]: argv[i + 1] for i in range(1, len(argv), 2)}
scale = float(o.get("--scale", 1))
xs = [float(v) for v in o.get("--x", "0.3").split(",")]
z0, z1, step = float(o.get("--z0", 0)), float(o.get("--z1", 18)), float(o.get("--step", 0.05))
back = o.get("--from") == "back"
along_x = o.get("--along") == "x"

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, folder, "model.glb"))
meshes = [ob for ob in bpy.context.scene.objects if ob.type == 'MESH']
bpy.context.view_layer.update()
lo = Vector((1e9,) * 3)
for ob in meshes:
    for c in ob.bound_box:
        w = ob.matrix_world @ Vector(c)
        lo = Vector(map(min, lo, w))
verts, polys = [], []
for ob in meshes:
    me = ob.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    base = len(verts)
    for v in me.vertices:
        verts.append(((ob.matrix_world @ v.co) - lo) * scale)
    for p in me.polygons:
        polys.append([base + i for i in p.vertices])
tree = BVHTree.FromPolygons(verts, polys)
for x in (xs if "--depthmap" not in o else []):
    print(f"RAYS {'along x at y' if along_x else 'x'} = {x}")
    z = z1
    while z >= z0 - 1e-9:
        if along_x:
            org, d = Vector((-5.0, x, z)), Vector((1, 0, 0))
        elif back:
            org, d = Vector((x, 50.0, z)), Vector((0, -1, 0))
        else:
            org, d = Vector((x, -5.0, z)), Vector((0, 1, 0))
        hit = tree.ray_cast(org, d, 100.0)
        val = None if hit[0] is None else hit[0][0] if along_x else hit[0][1]
        if hit[0] is None:
            print(f"  z {z:6.2f}  -")
        else:
            print(f"  z {z:6.2f}  {val:6.3f}")
        z -= step

# --depthmap <out.npy> [--res 0.01 --xmax 9]: the front (or --from back) depth of the whole model on a grid: array[row, col]
# = hit y (nan = no hit), row 0 at the top (z = z1), col 0 at x = 0; plus <out>.png, nearer = brighter
if "--depthmap" in o:
    import numpy as np
    res = float(o.get("--res", 0.01))
    x1 = float(o.get("--xmax", 9.0))
    nx, nz = int(round(x1 / res)) + 1, int(round((z1 - z0) / res)) + 1
    dm = np.full((nz, nx), np.nan, dtype=np.float32)
    d = Vector((0, -1, 0)) if back else Vector((0, 1, 0))
    # --along x (house #7): a side map instead: col = y (0..xmax), value = the hit x of a ray along +x from the left
    # (or along -x from the right with --from back)
    if along_x:
        d = Vector((-1, 0, 0)) if back else Vector((1, 0, 0))
    for r in range(nz):
        z = z1 - r * res
        for c in range(nx):
            if along_x:
                hit = tree.ray_cast(Vector((50.0 if back else -5.0, c * res, z)), d, 100.0)
                if hit[0] is not None:
                    dm[r, c] = hit[0][0]
                continue
            hit = tree.ray_cast(Vector((c * res, 50.0 if back else -5.0, z)), d, 100.0)
            if hit[0] is not None:
                dm[r, c] = hit[0][1]
    np.save(o["--depthmap"], dm)
    import OpenImageIO as oiio
    lo_, hi_ = np.nanpercentile(dm, 1), np.nanpercentile(dm, 99)
    img = np.where(np.isnan(dm), 0.0, 1.0 - (dm - lo_) / max(hi_ - lo_, 1e-6)).clip(0, 1)
    out = oiio.ImageBuf(oiio.ImageSpec(nx, nz, 1, oiio.UINT8))
    out.set_pixels(oiio.ROI(0, nx, 0, nz), (img[:, :, None] * 255).astype(np.uint8))
    out.write(o["--depthmap"].replace(".npy", ".png"))
    print("DEPTHMAP", dm.shape, lo_, hi_)
