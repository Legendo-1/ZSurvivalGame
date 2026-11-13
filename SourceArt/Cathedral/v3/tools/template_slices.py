"""Exact cross-sections of the template as PNG drawings with a metre grid.

usage: blender -b shell2_cache.blend --python slices.py -- <out folder> <spec> [<spec> ...]
spec:  name:axis:value:u0:u1:v0:v1:ppm     axis x|y|z = the plane's normal; (u, v) are the two other axes in
       the order (y,z) for x, (x,z) for y, (x,y) for z; ppm = pixels per metre.
Grid: thin lines every metre, stronger every 5 m, strongest every 10 m (through 0). v grows upward in the picture.
"""
import sys
import bpy
import numpy as np
import OpenImageIO as oiio

argv = sys.argv[sys.argv.index("--") + 1:]
out, specs = argv[0], argv[1:]
ob = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
me = ob.data
me.calc_loop_triangles()
co = np.empty(len(me.vertices) * 3, dtype=np.float32)
me.vertices.foreach_get("co", co)
co = co.reshape(-1, 3).astype(np.float64)
mw = np.array(ob.matrix_world)
co = co @ mw[:3, :3].T + mw[:3, 3]
tri = np.empty(len(me.loop_triangles) * 3, dtype=np.int32)
me.loop_triangles.foreach_get("vertices", tri)
tri = tri.reshape(-1, 3)
P = co[tri]                                         # (T, 3, 3)
AX = {"x": (0, 1, 2), "y": (1, 0, 2), "z": (2, 0, 1)}

for spec in specs:
    name, axis, value, u0, u1, v0, v1, ppm = spec.split(":")
    value, u0, u1, v0, v1, ppm = float(value), float(u0), float(u1), float(v0), float(v1), float(ppm)
    a, iu, iv = AX[axis]
    d = P[:, :, a] - value                          # signed distance of each corner
    cross = (d.min(axis=1) < 0) & (d.max(axis=1) > 0)
    Q, D = P[cross], d[cross]
    pts = []
    for i, j in ((0, 1), (1, 2), (2, 0)):           # edges that cross the plane
        m = (D[:, i] * D[:, j]) < 0
        t = D[:, i] / (D[:, i] - D[:, j] + 1e-30)
        pt = Q[:, i] + (Q[:, j] - Q[:, i]) * t[:, None]
        pts.append(np.where(m[:, None], pt, np.nan))
    pts = np.stack(pts, axis=1)                     # (K, 3 edges, 3): two of the three are valid
    order = np.argsort(np.isnan(pts[:, :, 0]), axis=1, kind="stable")
    pts = np.take_along_axis(pts, order[:, :, None], axis=1)[:, :2]
    A, Bp = pts[:, 0], pts[:, 1]
    W, H = int(round((u1 - u0) * ppm)), int(round((v1 - v0) * ppm))
    img = np.full((H, W, 3), 0.10, dtype=np.float32)
    for g in range(int(np.ceil(u0)), int(np.floor(u1)) + 1):
        c = int(round((g - u0) * ppm))
        if 0 <= c < W:
            img[:, c] = (0.55, 0.15, 0.15) if g % 10 == 0 else (0.33, 0.20, 0.12) if g % 5 == 0 else (0.17, 0.17, 0.17)
    for g in range(int(np.ceil(v0)), int(np.floor(v1)) + 1):
        r = H - 1 - int(round((g - v0) * ppm))
        if 0 <= r < H:
            img[r, :] = (0.15, 0.25, 0.60) if g % 10 == 0 else (0.12, 0.22, 0.36) if g % 5 == 0 else (0.17, 0.17, 0.17)
    n = 0
    for s in np.linspace(0.0, 1.0, 12):
        p = A + (Bp - A) * s
        cu = np.round((p[:, iu] - u0) * ppm).astype(int)
        rv = H - 1 - np.round((p[:, iv] - v0) * ppm).astype(int)
        ok = (cu >= 0) & (cu < W) & (rv >= 0) & (rv < H)
        img[rv[ok], cu[ok]] = (1.0, 1.0, 1.0)
        n += int(ok.sum())
    buf = oiio.ImageBuf(oiio.ImageSpec(W, H, 3, oiio.UINT8))
    buf.set_pixels(oiio.ROI(0, W, 0, H), img)
    buf.write(f"{out}/{name}.png")
    print(f"SLICE {name}: plane {axis} = {value}, {int(cross.sum())} triangles cut, picture {W} x {H}, u {u0}..{u1} v {v0}..{v1}")
