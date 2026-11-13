"""Builds SM_Cathedral_Core_liner: the inner core of the rebuilt Girona cathedral and its collision.

  - The visible mesh is a set of plain closed bodies standing 0.6 to 0.8 m inside the stonework. Nobody
    should ever see it: it is there so that no gap anywhere can show daylight through the building.
  - The collision (UCX_ convex hulls in the same FBX) follows the true wall faces: walls, buttresses and
    towers block, the front stairs and the landing can be walked, both side-door recesses can be entered
    as far as the doors.

Coordinates are those of the Meshy model (front = -Y, bell tower = +X, metres), as in build_front.py; the
numbers repeat the layouts of build_front.py, build_bell_tower.py, build_side.py and build_apse.py.

Run (Blender 5.2, no UI):  blender -b --factory-startup --python build_core.py -- --out <folder>
Writes <out>/Girona_Core_liner.blend and <out>/SM_Cathedral_Core_liner.fbx.
"""
import argparse
import math
import os
import sys

import bmesh
import bpy
import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gothic_kit as gk  # noqa: E402
import build_common  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--part", default="liner")
args, _unknown = ap.parse_known_args(argv)
NAME = "SM_Cathedral_Core_liner"


def box(x0, x1, y0, y1, z0, z1):
    return [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]


def prism(poly, z0, z1):
    return [(x, y, z) for x, y in poly for z in (z0, z1)]


def mirror(pts):
    return [(-x, y, z) for x, y, z in pts]


def both(pts):
    return [pts, mirror(pts)]


def rect(x0, x1, y0, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def flip(poly):
    return [(-x, y) for x, y in poly]


def grow(poly, d):
    """The convex plan polygon with every side moved outwards by d."""
    pts = np.array(poly, dtype=np.float64)
    centre = pts.mean(axis=0)
    lines = []
    for i in range(len(pts)):
        a, b = pts[i], pts[(i + 1) % len(pts)]
        t = (b - a) / np.linalg.norm(b - a)
        n = np.array([t[1], -t[0]])
        if np.dot(n, a - centre) < 0:
            n = -n
        lines.append((n, float(np.dot(n, a)) + d))
    return [tuple(np.linalg.solve(np.array([lines[i - 1][0], lines[i][0]]), np.array([lines[i - 1][1], lines[i][1]]))) for i in range(len(pts))]


def ring(c, r, n, first=0.0):
    return [(c[0] + r * math.cos(math.radians(first + 360.0 / n * k)), c[1] + r * math.sin(math.radians(first + 360.0 / n * k))) for k in range(n)]


C_LOW = (11.184, -37.134)               # bell tower shaft
C_APSE = (0.0, 39.3)
BUTTS = (-22.9, -13.4, -1.1)
FLOOR = 4.625                           # landing in front of the main door
PLINTH_H, PLINTH_OUT = 2.75, 0.22       # the base course round the building (build_front.py, build_side.py, build_apse.py)

# ---------------------------------------------------------------------------------------------
# Collision: the true faces
# ---------------------------------------------------------------------------------------------
COLL = []
# front, towers, nave
COLL.append(box(-13.2, 13.2, -42.2, -38.0, 0.0, 32.95))
COLL.append(prism(ring(C_LOW, 4.866 / math.cos(math.radians(22.5)), 8, 22.5), 0.0, 33.0))
COLL.append(prism([(-16.05, -39.15), (-13.2, -42.0), (-10.85, -42.0), (-10.85, -35.6), (-16.05, -35.6)], 0.0, 32.95))
COLL.append(box(-10.85, 10.85, -38.0, 20.5, 0.0, 30.0))
COLL.append([(sx * 10.85, y, 30.0) for sx in (-1, 1) for y in (-41.6, 20.5)] + [(0.0, y, 33.1) for y in (-41.6, 20.5)])
for pc in (-4.62, -2.42, 2.42, 4.62):                    # pedestals and columns beside the main door
    COLL.append(box(pc - 0.52, pc + 0.52, -42.88, -42.2, FLOOR, 9.0))
# front stairs: a ramp for the flight, the landing, the stepped side walls, terraces, rails
COLL.append([(sx * 7.0, y, z) for sx in (-1, 1) for y, z in ((-54.5, 0.0), (-54.5, 0.08), (-47.1, FLOOR), (-46.9, FLOOR), (-46.9, 0.0))])
COLL.append(box(-11.2, 11.2, -47.0, -42.2, 0.0, FLOOR))
for k in range(5):
    COLL += both(box(7.0, 7.9, -54.5 + 1.5 * k, -53.0 + 1.5 * k, 0.0, 0.925 * (k + 1) + 0.65))
COLL += both(box(7.9, 10.2, -54.5, -51.0, 0.0, 2.5))
COLL += both(box(7.9, 10.2, -54.5, -54.24, 2.5, 3.45)) + both(box(9.94, 10.2, -54.5, -51.0, 2.5, 3.45)) + both(box(7.9, 10.2, -51.26, -51.0, 2.5, 3.45))
COLL += both(box(7.9, 11.2, -47.0, -46.74, FLOOR, 5.7)) + both(box(10.94, 11.2, -47.0, -42.0, FLOOR, 5.7))
COLL += both(box(6.83, 8.07, -55.8, -54.52, 0.0, 2.3))
# both long sides: chapels, buttresses, old towers
COLL += both(box(10.85, 17.0, -23.82, 26.4, 0.0, 10.85))
for yc in BUTTS:
    COLL += both(box(10.85, 17.7, yc - 0.92, yc + 0.92, 0.0, 21.6)) + both(box(10.85, 16.05, yc - 0.92, yc + 0.92, 21.6, 28.3))
COLL.append(box(10.85, 18.1, 7.1, 14.1, 0.0, 31.6))
COLL.append(box(-17.8, -10.85, 13.9, 20.65, 0.0, 31.6))
# right side: the Gothic doorway. Back of the porch, the two splayed jambs, three steps up to the door
COLL.append(box(10.85, 13.0, -35.118, -23.82, 0.0, 17.2))
COLL.append(prism([(16.05, -34.45), (13.25, -31.75), (13.0, -31.75), (13.0, -35.118), (16.05, -35.118)], 0.0, 17.2))
COLL.append(prism([(16.05, -25.55), (16.05, -23.82), (13.0, -23.82), (13.0, -28.25), (13.25, -28.25)], 0.0, 17.2))
COLL.append(box(13.0, 16.05, -34.45, -25.55, 11.0, 17.2))
COLL += [box(13.0, 14.3, -33.0, -27.0, 0.0, 0.45), box(14.3, 14.75, -33.4, -26.6, 0.0, 0.30), box(14.75, 15.2, -33.8, -26.2, 0.0, 0.15)]
# left side: the columned doorway. Back wall, the two piers, the lintel, four columns, the steps
COLL.append(box(-14.15, -10.85, -35.6, -23.82, 0.0, 17.2))
COLL += [box(-16.05, -14.15, -35.6, -33.61, 0.0, 17.2), box(-16.05, -14.15, -25.81, -23.82, 0.0, 17.2), box(-16.05, -14.15, -33.61, -25.81, 8.6, 17.2)]
for dy in (-3.25, -1.72, 1.72, 3.25):
    COLL.append(box(-14.77, -14.15, -29.71 + dy - 0.33, -29.71 + dy + 0.33, 0.45, 6.4))
COLL += [box(-15.15, -14.15, -33.61, -25.81, 0.0, 0.45), box(-15.45, -15.15, -33.61, -25.81, 0.0, 0.30), box(-15.75, -15.45, -33.61, -25.81, 0.0, 0.15)]
# rear: the block between nave and apse, the annexes, the apse with its buttresses
COLL.append(box(-10.85, 10.85, 20.5, 26.4, 0.0, 10.85))
COLL += both(box(6.0, 18.0, 24.0, 27.9, 0.0, 6.2)) + both(box(6.0, 16.0, 27.9, 31.1, 0.0, 4.6))
COLL.append(prism(ring(C_APSE, 12.9 / math.cos(math.radians(15.0)), 12), 0.0, 18.0))
COLL.append(prism(ring(C_APSE, 10.2 / math.cos(math.radians(15.0)), 12), 18.0, 30.6))
for j in range(12):
    if j != 9:
        a = math.radians(30.0 * j)
        r, t = np.array([math.cos(a), math.sin(a)]), np.array([-math.sin(a), math.cos(a)])
        v = np.array(C_APSE) + r * (12.9 / math.cos(math.radians(15.0)))
        foot = [tuple(v - t * 0.75 - r * 0.3), tuple(v - t * 0.75 + r * 1.2), tuple(v + t * 0.75 + r * 1.2), tuple(v + t * 0.75 - r * 0.3)]
        COLL.append(prism(foot, 0.0, 13.0))
        COLL.append(prism(grow(foot, PLINTH_OUT), 0.0, PLINTH_H))
# the base course: every wall that stands on the ground is 0.22 m thicker up to 2.75 m
PLINTHS = [ring(C_LOW, 4.866 / math.cos(math.radians(22.5)), 8, 22.5),
           [(-16.05, -39.15), (-13.2, -42.0), (-10.85, -42.0), (-10.85, -35.6), (-16.05, -35.6)],
           rect(10.85, 18.1, 7.1, 14.1), rect(-17.8, -10.85, 13.9, 20.65),
           [(16.05, -34.45), (13.25, -31.75), (13.0, -31.75), (13.0, -35.118), (16.05, -35.118)],
           [(16.05, -25.55), (16.05, -23.82), (13.0, -23.82), (13.0, -28.25), (13.25, -28.25)],
           ring(C_APSE, 12.9 / math.cos(math.radians(15.0)), 12)]
for side in (1, -1):
    for poly in [rect(10.85, 17.0, -23.82, 26.4), rect(6.0, 18.0, 24.0, 27.9), rect(6.0, 16.0, 27.9, 31.1)] + [rect(10.85, 17.7, yc - 0.92, yc + 0.92) for yc in BUTTS]:
        PLINTHS.append(poly if side > 0 else flip(poly))
for poly in PLINTHS:
    COLL.append(prism(grow(poly, PLINTH_OUT), 0.0, PLINTH_H))
# the piers of the left doorway carry it on their outer face only: the opening between them stays its full width
COLL += [box(-16.05 - PLINTH_OUT, -14.15, -35.6, -33.61, 0.0, PLINTH_H), box(-16.05 - PLINTH_OUT, -14.15, -25.81, -23.82, 0.0, PLINTH_H)]

# ---------------------------------------------------------------------------------------------
# The visible core: well inside every wall, niche, window recess and roof
# ---------------------------------------------------------------------------------------------
CORE = []
CORE.append([(sx * 10.1, y, z) for sx in (-1, 1) for y in (-41.45, 19.8) for z in (0.0, 29.3)] + [(0.0, y, 32.4) for y in (-41.45, 19.8)])
CORE.append(box(-12.9, 12.9, -41.3, -38.5, 0.0, 32.2))
CORE.append(prism(ring(C_LOW, 4.1 / math.cos(math.radians(22.5)), 8, 22.5), 0.0, 32.2))
CORE.append(prism([(-15.3, -39.2), (-13.0, -41.3), (-10.5, -41.3), (-10.5, -36.3), (-15.3, -36.3)], 0.0, 32.2))
CORE += both(box(10.1, 16.25, -24.2, 25.65, 0.0, 10.2))
for yc in BUTTS:
    CORE += both(box(10.1, 16.95, yc - 0.3, yc + 0.3, 0.0, 20.9))
CORE.append(box(10.1, 17.35, 7.85, 13.35, 0.0, 30.9))
CORE.append(box(-17.05, -10.1, 14.65, 19.9, 0.0, 30.9))
CORE.append(box(10.1, 12.25, -35.0, -23.9, 0.0, 16.5))
CORE.append(box(-13.4, -10.1, -35.5, -23.9, 0.0, 16.5))
CORE.append(box(-10.1, 10.1, 19.5, 25.65, 0.0, 10.2))
CORE += both(box(6.0, 17.25, 24.75, 27.15, 0.0, 5.5)) + both(box(6.0, 15.25, 27.15, 30.35, 0.0, 3.9))
CORE.append(box(-10.45, 10.45, -46.25, -41.3, 0.0, 4.2))
CORE.append([(sx * 6.25, y, z) for sx in (-1, 1) for y, z in ((-53.2, 0.0), (-46.25, 0.0), (-46.25, 4.0))])
CORE += both(box(8.65, 9.45, -53.75, -51.75, 0.0, 1.9))


def hull(points):
    """Convex hull of the points as (vertices, triangles), facing outwards."""
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.verts.index_update()
    v = np.array([vt.co[:] for vt in bm.verts], dtype=np.float64)
    f = np.array([[vt.index for vt in fc.verts] for fc in bm.faces], dtype=np.int64)
    bm.free()
    return v, f


bpy.ops.wm.read_factory_settings(use_empty=True)
B = gk.Builder(build_common.MATERIALS)
for pts in CORE:
    v, f = hull(pts)
    B.add(v, f, "Ashlar", var=0.15)
obj = B.to_object(NAME, build_common.COLORS)
objs = [obj]
for i, pts in enumerate(COLL):
    v, f = hull(pts)
    me = bpy.data.meshes.new(f"UCX_{NAME}_{i:02d}")
    me.from_pydata(v.tolist(), [], f.tolist())
    me.update()
    ob = bpy.data.objects.new(f"UCX_{NAME}_{i:02d}", me)
    bpy.context.scene.collection.objects.link(ob)
    objs.append(ob)
print(f"BUILT {NAME}: {B.triangles()} triangles in {len(CORE)} bodies, {len(COLL)} collision hulls")
os.makedirs(args.out, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(args.out, "Girona_Core_liner.blend"))
bpy.ops.object.select_all(action='DESELECT')
for ob in objs:
    ob.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(
    filepath=os.path.join(args.out, NAME + ".fbx"), use_selection=True, object_types={'MESH'},
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y',
    mesh_smooth_type='FACE', colors_type='SRGB', use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
print("EXPORTED")
