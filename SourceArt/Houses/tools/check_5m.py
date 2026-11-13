"""The 5 m rule (ledger section 3): how close a house footprint comes to the rebuilt cathedral.

Run headless with Blender:
  blender.exe -b --factory-startup --python check_5m.py -- <x0> <x1> <y0> <y1> <z0> <z1> [--blend <house .blend>]

x0..x1, y0..y1, z0..z1 are the house's walls as a box in world centimetres (an upright box from street level to
the roof). The cathedral parts are read from SourceArt/Cathedral/v3/Girona_*.blend (the same meshes as the
components of BP_GironaCathedral_V3) and put where the BP stands in Lvl_CathedralDistrict_Art:
  location (21180, 3430, 1300), yaw 90  ->  world x = 21180 + 100 * by, world y = 3430 + 100 * bx, z = 1300 + 100 * bz.
For every part it prints
  plan   the smallest distance in plan (top view) from the footprint to any cathedral vertex between z0 and z1
  3d     the smallest distance from the walls of the box (sampled every 0.25 m) to the part's surface
With --blend, the house's own mesh (a build_house.py .blend, in house space) is checked instead of the box: pass
the transform with --place <x> <y> <z> <yaw> (the actor transform, see ledger section 3).
Exits with the closest distance in the last line: "CHECK5M min <metres> (<part>)".
"""
import glob
import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
V3 = os.path.normpath(os.path.join(HERE, "..", "..", "Cathedral", "v3"))
argv = sys.argv[sys.argv.index("--") + 1:]
x0, x1, y0, y1, z0, z1 = (float(v) for v in argv[:6])
house_blend, place = None, None
i = 6
while i < len(argv):
    if argv[i] == "--blend":
        house_blend = argv[i + 1]; i += 2
    elif argv[i] == "--place":
        place = [float(v) for v in argv[i + 1:i + 5]]; i += 5
    else:
        raise SystemExit(f"unknown argument {argv[i]}")

bpy.ops.wm.read_factory_settings(use_empty=True)


def load_meshes(path):
    with bpy.data.libraries.load(path, link=False) as (src, dst):
        dst.objects = list(src.objects)
    return [o for o in dst.objects if o is not None and o.type == 'MESH']


def world_of_cathedral(co):
    return np.stack([21180.0 + 100.0 * co[:, 1], 3430.0 + 100.0 * co[:, 0], 1300.0 + 100.0 * co[:, 2]], axis=1)


# sample points: on the walls of the box, or on the house mesh
if house_blend:
    pts = []
    for ob in load_meshes(house_blend):
        me = ob.data
        co = np.empty(len(me.vertices) * 3, dtype=np.float32)
        me.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3).astype(np.float64)
        px, py, pz, yaw = place
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        ux, uy = co[:, 0] * 100.0, -co[:, 1] * 100.0          # FBX export: Unreal mesh y = -Blender y
        pts.append(np.stack([px + c * ux - s * uy, py + s * ux + c * uy, pz + co[:, 2] * 100.0], axis=1))
    samples = np.concatenate(pts)
    print(f"CHECK5M house mesh {house_blend}: {len(samples)} vertices")
else:
    st = 25.0
    xs, ys, zs = np.arange(x0, x1 + 1e-6, st), np.arange(y0, y1 + 1e-6, st), np.arange(z0, z1 + 1e-6, st)
    s = []
    for z in zs:
        s += [(x, y0, z) for x in xs] + [(x, y1, z) for x in xs] + [(x0, y, z) for y in ys] + [(x1, y, z) for y in ys]
    samples = np.array(s)

best = (1e9, None)
for path in sorted(glob.glob(os.path.join(V3, "Girona_*.blend"))):
    for ob in load_meshes(path):
        me = ob.data
        co = np.empty(len(me.vertices) * 3, dtype=np.float32)
        me.vertices.foreach_get("co", co)
        w = world_of_cathedral(co.reshape(-1, 3).astype(np.float64))
        sel = (w[:, 2] >= z0) & (w[:, 2] <= z1)
        if sel.any():
            q = w[sel]
            dx = np.maximum(np.maximum(x0 - q[:, 0], q[:, 0] - x1), 0.0)
            dy = np.maximum(np.maximum(y0 - q[:, 1], q[:, 1] - y1), 0.0)
            plan = float(np.sqrt(dx * dx + dy * dy).min()) / 100.0
        else:
            plan = float("inf")
        tree = BVHTree.FromPolygons(w.tolist(), [tuple(p.vertices) for p in me.polygons], all_triangles=False)
        d3, at = 1e9, None
        for p in samples:
            hit = tree.find_nearest(Vector(p), 3000.0)
            if hit[0] is not None and hit[3] < d3:
                d3, at = hit[3], (tuple(round(v) for v in p), tuple(round(v) for v in hit[0]))
        d3 /= 100.0
        if min(plan, d3) < 12.0:
            print(f"CHECK5M {ob.name:34s} plan {plan:6.2f} m   3d {d3:6.2f} m   closest {at}")
        if d3 < best[0]:
            best = (d3, ob.name)
        if plan < best[0]:
            best = (plan, ob.name + " (plan)")
print(f"CHECK5M min {best[0]:.2f} ({best[1]})")
