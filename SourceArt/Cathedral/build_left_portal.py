"""Builds SM_Girona_LeftPortal: the Gothic side portal of the Girona cathedral
(reference: "Cathedral images/Left_door.jpg" and "Left_side_door.jpg").

The portal is built in the coordinate space of the Meshy cathedral shell
(SM_GironaCathedral_Shell), so it lines up when placed with the same transform
as the shell. It stands against the shell's left (-Y) wall, between
Charlemagne's tower and the first nave buttress.

Run (Blender 5.2, no UI):
  blender -b --factory-startup --python build_left_portal.py -- --out <folder>
      [--shell <blend with the Meshy shell> --renders <folder for preview renders>]

Writes <out>/Girona_LeftPortal.blend and <out>/SM_Girona_LeftPortal.fbx
(the mesh plus UCX_ collision hulls for Unreal).
"""
import argparse
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--shell")
ap.add_argument("--renders")
args = ap.parse_args(argv)

NAME = "SM_Girona_LeftPortal"

# ---------------------------------------------------------------------------
# Placement on the shell (Blender coordinates of the imported Meshy FBX).
# Local frame used below: x = along the wall (viewer's right), y = out of the
# wall towards the viewer, z = up. G() converts local to shell coordinates.
# ---------------------------------------------------------------------------
XC = 2.8     # portal centre along the wall; the left pier tucks 0.2 m into the tower (tower edge X -3.2)
Y0 = -14.0   # back of the porch; the chapel wall is at Y -14.2, so the porch is 0.2 m embedded


def G(x, y, z):
    return Vector((XC + x, Y0 - y, z))


# ---------------------------------------------------------------------------
# Dimensions in metres, from the photos (ratios to the door width) fitted to
# the shell's scale: the porch top meets the chapel parapet (~9.8 m).
# ---------------------------------------------------------------------------
W2 = 6.2            # half width of the porch
H = 10.2            # porch top (just above the chapel parapet at ~9.8 m)
D = 2.75            # porch front face (out from the wall)
YD = 1.7            # door / tympanum plane (just in front of the shell's buttress at 1.45)
P_LO, P_HI = 1.45, 1.15             # pier widths below / above the springing
PIER_FRONT, PIER_FRONT_HI = D + 0.4, D + 0.2
PIER_STEP_Z0, PIER_STEP_Z1 = 5.2, 5.6
PIER_TOP = H + 0.45
ZS = 4.15           # springing line (capital band)
ZB = 2.9            # colonnettes start (top of the lower niche tier)
Z_LOWER_TOP = 2.69
Z_PLINTH = 0.38
R0, RI = 4.15, 1.34     # outer / inner half span of the archivolts
A0, A1 = 9.6, 6.15      # outer / inner apex height (pointed, rise ~1.3x the half span)
N_RINGS = 12
ROLL = 0.11             # archivolt roll height
SPLAY_N = (0.327, 0.945)  # (inward, out) mix of the splayed archivolt surface normal
DOOR_W2, DOOR_H = 1.025, 3.04

STONE, TRIM, DOOR = 0, 1, 2
MAT_NAMES = ["Stone", "Trim", "Door"]
MAT_COLORS = [(0.70, 0.66, 0.58), (0.80, 0.77, 0.70), (0.16, 0.24, 0.19)]


# ---------------------------------------------------------------------------
# Geometry helpers. Every piece is built in its own bmesh with correct
# normals, then appended to the accumulator with its material index.
# ---------------------------------------------------------------------------
class Accumulator:
    def __init__(self):
        self.bm = bmesh.new()

    def add(self, src, mat, smooth=False):
        src.verts.index_update()
        vmap = [self.bm.verts.new(v.co) for v in src.verts]
        for f in src.faces:
            try:
                nf = self.bm.faces.new([vmap[v.index] for v in f.verts])
            except ValueError:
                continue
            nf.material_index = mat
            nf.smooth = smooth
        src.free()


ACC = Accumulator()


def solid_from_loops(loops, mat, smooth=False, closed_loop=True):
    """Skin consecutive loops of equal length; cap the first and last loop."""
    bm = bmesh.new()
    vs = [[bm.verts.new(p) for p in loop] for loop in loops]
    n = len(loops[0])
    rng = range(n) if closed_loop else range(n - 1)
    for a, b in zip(vs, vs[1:]):
        for i in rng:
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i]))
    bm.faces.new(vs[0])
    bm.faces.new(vs[-1])
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4], quad_method='BEAUTY', ngon_method='EAR_CLIP')
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ACC.add(bm, mat, smooth)


def hexa(bottom, top, mat):
    """Closed six-sided solid from 4 bottom and 4 top local points."""
    solid_from_loops([[G(*p) for p in bottom], [G(*p) for p in top]], mat)


def box(x0, x1, y0, y1, z0, z1, mat):
    q = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    hexa([(x, y, z0) for x, y in q], [(x, y, z1) for x, y in q], mat)


def extrude_front(poly_xz, y0, y1, mat):
    """Polygon in the wall plane (x, z), extruded from depth y0 to y1."""
    solid_from_loops([[G(x, y0, z) for x, z in poly_xz], [G(x, y1, z) for x, z in poly_xz]], mat)


def extrude_plane(poly_az, origin, u, n, d0, d1, mat):
    """Polygon in a vertical plane (a along u, z up), extruded along n from d0 to d1."""
    def P(a, z, d):
        p = origin + u * a + n * d
        return G(p.x, p.y, z)
    solid_from_loops([[P(a, z, d0) for a, z in poly_az], [P(a, z, d1) for a, z in poly_az]], mat)


def arch_band(outer, inner, origin, u, n, d0, d1, mat):
    """Arch-shaped band between two curves of equal point count, built from quads
    (no polygon fill, so nothing can bridge the inside of the arch)."""
    def P(a, z, d):
        p = origin + u * a + n * d
        return G(p.x, p.y, z)
    bm = bmesh.new()
    o0 = [bm.verts.new(P(a, z, d0)) for a, z in outer]
    o1 = [bm.verts.new(P(a, z, d1)) for a, z in outer]
    i0 = [bm.verts.new(P(a, z, d0)) for a, z in inner]
    i1 = [bm.verts.new(P(a, z, d1)) for a, z in inner]
    for k in range(len(outer) - 1):
        for a, b, c, d in ((o1[k], o1[k + 1], i1[k + 1], i1[k]),     # front
                           (o0[k], i0[k], i0[k + 1], o0[k + 1]),     # back
                           (o0[k], o0[k + 1], o1[k + 1], o1[k]),     # outer edge
                           (i0[k], i1[k], i1[k + 1], i0[k + 1])):    # inner edge
            bm.faces.new((a, b, c, d))
    for k in (0, len(outer) - 1):
        bm.faces.new((o0[k], o1[k], i1[k], i0[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ACC.add(bm, mat)


FRONT_U, FRONT_N = Vector((1, 0, 0)), Vector((0, 1, 0))


def pyramid_local(cx, cy, z0, half, height, mat, u=Vector((1, 0, 0)), n=Vector((0, 1, 0))):
    bm = bmesh.new()
    c = Vector((cx, cy, 0))
    base = []
    for su, sn in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        p = c + u * (su * half) + n * (sn * half)
        base.append(bm.verts.new(G(p.x, p.y, z0)))
    tip = bm.verts.new(G(cx, cy, z0 + height))
    bm.faces.new(base)
    for i in range(4):
        bm.faces.new((base[i], base[(i + 1) % 4], tip))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ACC.add(bm, mat)


def tube(path, radius, mat, sides=8, closed=False):
    """Round moulding swept along a list of local points."""
    pts = [G(*p) for p in path]
    loops = []
    count = len(pts)
    prev_normal = None
    for i, p in enumerate(pts):
        if closed:
            t = (pts[(i + 1) % count] - pts[i - 1]).normalized()
        else:
            t = (pts[min(i + 1, count - 1)] - pts[max(i - 1, 0)]).normalized()
        if prev_normal is None:
            ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            nrm = t.cross(ref).normalized()
        else:
            nrm = (prev_normal - t * prev_normal.dot(t)).normalized()
        prev_normal = nrm
        bi = t.cross(nrm)
        loops.append([p + (nrm * math.cos(a) + bi * math.sin(a)) * radius
                      for a in (2 * math.pi * k / sides for k in range(sides))])
    bm = bmesh.new()
    vs = [[bm.verts.new(q) for q in loop] for loop in loops]
    pairs = list(zip(vs, vs[1:])) + ([(vs[-1], vs[0])] if closed else [])
    for a, b in pairs:
        for k in range(sides):
            bm.faces.new((a[k], a[(k + 1) % sides], b[(k + 1) % sides], b[k]))
    if not closed:
        bm.faces.new(vs[0])
        bm.faces.new(vs[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ACC.add(bm, mat, smooth=True)


def icosphere(center_local, radius, mat):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=radius, matrix=Matrix.Translation(G(*center_local)))
    ACC.add(bm, mat, smooth=True)


def pointed_arch(h, rise, z0, steps=10):
    """Points of a two-centred pointed arch, left springing -> apex -> right springing, centred on a=0."""
    c = (rise * rise - h * h) / (2 * h)
    rho = h + c
    th_a = math.acos(-c / rho)
    left = [(c + rho * math.cos(th), z0 + rho * math.sin(th))
            for th in (math.pi + (th_a - math.pi) * i / steps for i in range(steps + 1))]
    left[-1] = (0.0, left[-1][1])
    right = [(-a, z) for a, z in reversed(left[:-1])]
    return left + right


# ---------------------------------------------------------------------------
# Archivolts: one smooth surface with N rolls, from the outer arch at the
# front face down to the inner arch at the door plane.
# ---------------------------------------------------------------------------
def ring_params(s):
    r = R0 + (RI - R0) * s
    apex = A0 + (A1 - A0) * s
    y = D + (YD - D) * s
    rise = apex - ZS
    c = (rise * rise - r * r) / (2 * r)
    return r, y, c, r + c


def ring_path(s, zb=ZB, nv=8, na=48):
    """Points (x, z) and in-plane inward normals of one archivolt ring, left foot -> right foot."""
    r, y, c, rho = ring_params(s)
    th_a = math.acos(-c / rho)
    pts, nrm = [], []
    for i in range(nv + 1):
        pts.append((-r, zb + (ZS - zb) * i / nv))
        nrm.append((1.0, 0.0))
    for i in range(1, na + 1):
        th = math.pi + (th_a - math.pi) * i / na
        pts.append((c + rho * math.cos(th), ZS + rho * math.sin(th)))
        nrm.append((-math.cos(th), -math.sin(th)))
    pts[-1] = (0.0, pts[-1][1])
    nrm[-1] = (0.0, -1.0)
    for i in range(na - 1, -1, -1):
        th = math.pi + (th_a - math.pi) * i / na
        pts.append((-(c + rho * math.cos(th)), ZS + rho * math.sin(th)))
        nrm.append((math.cos(th), -math.sin(th)))
    for i in range(nv - 1, -1, -1):
        pts.append((r, zb + (ZS - zb) * i / nv))
        nrm.append((-1.0, 0.0))
    return pts, nrm, y


def roll_bump(s):
    q = (s * N_RINGS) % 1.0
    return ROLL * math.sqrt(max(0.0, 1.0 - (2 * q - 1) ** 2))


def roll_point(s, idx):
    pts, nrm, y = ring_path(s)
    (x, z), (nx, nz) = pts[idx], nrm[idx]
    b = roll_bump(s)
    return (x + b * SPLAY_N[0] * nx, y + b * SPLAY_N[1], z + b * SPLAY_N[0] * nz)


def build_archivolts():
    rows = N_RINGS * 10 + 1
    grid = []
    for j in range(rows):
        s = j / (rows - 1)
        pts, _, _ = ring_path(s)
        grid.append([G(*roll_point(s, i)) for i in range(len(pts))])
    bm = bmesh.new()
    vs = [[bm.verts.new(p) for p in row] for row in grid]
    for a, b in zip(vs, vs[1:]):
        for i in range(len(a) - 1):
            bm.faces.new((a[i], a[i + 1], b[i + 1], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # Face the viewer: most faces must point out of the wall (-Y in shell space).
    bm.normal_update()
    if sum(1 for f in bm.faces if f.normal.y < 0) < len(bm.faces) / 2:
        bmesh.ops.reverse_faces(bm, faces=bm.faces)
    ACC.add(bm, TRIM, smooth=True)

    # Capitals at the springing and bases at the foot of every roll, both sides.
    nv = 8
    for k in range(N_RINGS):
        s = (k + 0.5) / N_RINGS
        pts, _, _ = ring_path(s)
        for idx in (nv, len(pts) - 1 - nv):           # springing point, left and right
            cx, cy, _ = roll_point(s, idx)
            h0, h1 = 0.075, 0.16
            hexa([(cx - h0, cy - h0, ZS - 0.36), (cx + h0, cy - h0, ZS - 0.36), (cx + h0, cy + h0, ZS - 0.36), (cx - h0, cy + h0, ZS - 0.36)],
                 [(cx - h1, cy - h1, ZS - 0.08), (cx + h1, cy - h1, ZS - 0.08), (cx + h1, cy + h1, ZS - 0.08), (cx - h1, cy + h1, ZS - 0.08)], TRIM)
            box(cx - 0.18, cx + 0.18, cy - 0.18, cy + 0.18, ZS - 0.08, ZS, TRIM)          # abacus
            box(cx - 0.09, cx + 0.09, cy - 0.09, cy + 0.09, ZS - 0.42, ZS - 0.36, TRIM)    # necking
            for ox, oy in ((-1, 1), (1, 1)):                                               # leaf tips
                pyramid_local(cx + ox * 0.12, cy + oy * 0.12, ZS - 0.26, 0.05, 0.16, TRIM)
        for idx in (0, len(pts) - 1):                  # bases at the foot
            cx, cy, _ = roll_point(s, idx)
            box(cx - 0.11, cx + 0.11, cy - 0.11, cy + 0.11, ZB, ZB + 0.08, TRIM)

    # Hood moulding just outside the outer arch.
    pts, nrm, _ = ring_path(0.0)
    hood = [(x - 0.1 * nx, D + 0.04, z - 0.1 * nz) for (x, z), (nx, nz) in list(zip(pts, nrm))[nv:len(pts) - nv]]
    tube(hood, 0.075, TRIM)


# ---------------------------------------------------------------------------
# Niche arcades: mullion strips, pointed heads and pinnacles standing proud of
# a vertical plane.
# ---------------------------------------------------------------------------
def panel_set(origin, u, n, length, z0, z_spring, rise, count, mat=TRIM, depth=0.06, wm=0.07, sill=True):
    margin = 0.06
    wn = (length - 2 * margin - (count + 1) * wm) / count
    centres = [margin + wm / 2 + i * (wn + wm) for i in range(count + 1)]
    for a in centres:
        extrude_plane([(a - wm / 2, z0), (a + wm / 2, z0), (a + wm / 2, z_spring), (a - wm / 2, z_spring)],
                      origin, u, n, 0.0, depth, mat)
        p = origin + u * a + n * (depth / 2)
        pyramid_local(p.x, p.y, z_spring + rise * 0.55, wm * 0.75, rise * 0.75 + 0.18, mat, u=u, n=n)
    for a0, a1 in zip(centres, centres[1:]):
        ac, hs = (a0 + a1) / 2, (a1 - a0) / 2
        outer = [(ac + a, z) for a, z in pointed_arch(hs + wm / 2, rise + 0.05, z_spring)]
        inner = [(ac + a, z) for a, z in pointed_arch(hs - wm / 2, rise - 0.02, z_spring)]
        arch_band(outer, inner, origin, u, n, 0.0, depth, mat)
        # trefoil cusps: two small points hanging inside the head
        for side in (-1, 1):
            cx = ac + side * (hs - wm / 2) * 0.45
            cz = z_spring + rise * 0.55
            extrude_plane([(cx - 0.035, cz + 0.06), (cx + 0.035, cz + 0.06), (cx, cz - 0.05)],
                          origin, u, n, 0.0, depth * 0.8, mat)
    if sill:
        extrude_plane([(0, z0 - 0.08), (length, z0 - 0.08), (length, z0), (0, z0)], origin, u, n, 0.0, depth + 0.02, mat)


# ---------------------------------------------------------------------------
# The porch
# ---------------------------------------------------------------------------
def build_porch():
    # Front wall (spandrels) with the arch opening, from the wall out to the front face.
    pts, _, _ = ring_path(0.0, zb=0.0)
    poly = [(-W2 + 0.05, 0.0), (-W2 + 0.05, H - 0.05), (W2 - 0.05, H - 0.05), (W2 - 0.05, 0.0)] + list(reversed(pts))
    extrude_front(poly, -0.2, D, STONE)

    # Door wall / tympanum at the back of the recess, with the door opening.
    # Built as three simple pieces (tympanum above the door, two strips beside it)
    # so no fill can bridge the door opening.
    pts, _, _ = ring_path(1.0, zb=DOOR_H)
    extrude_front(list(pts), YD - 0.2, YD, STONE)
    box(-RI, -DOOR_W2, YD - 0.2, YD, 0.0, DOOR_H, STONE)
    box(DOOR_W2, RI, YD - 0.2, YD, 0.0, DOOR_H, STONE)

    # Top: cornice and capping slab; base course on the front face.
    box(-W2, W2, D, D + 0.16, H - 0.34, H - 0.14, TRIM)
    box(-W2, W2, -0.2, D + 0.24, H - 0.14, H, STONE)
    for sx in (-1, 1):
        x_in, x_out = sx * R0, sx * (W2 - 0.05)
        box(min(x_in, x_out), max(x_in, x_out), D - 0.05, D + 0.14, 0.0, Z_PLINTH, STONE)

    # Splayed jambs: lower niche tier, its plinth and the band under the colonnettes.
    for sx in (-1, 1):
        a = Vector((sx * RI, YD, 0))
        b = Vector((sx * R0, D, 0))
        u = (b - a).normalized()
        n = Vector((-u.y, u.x, 0)) if (-u.y * 0 + u.x) > 0 else Vector((u.y, -u.x, 0))
        if n.y < 0:
            n = -n
        length = (b - a).length
        ext = u * 0.03

        def q(off):
            p0, p1 = a - ext + n * off, b + ext + n * off
            return p0, p1
        f0, f1 = q(0.0)
        k0, k1 = q(-0.6)
        hexa([(f0.x, f0.y, Z_PLINTH), (f1.x, f1.y, Z_PLINTH), (k1.x, k1.y, Z_PLINTH), (k0.x, k0.y, Z_PLINTH)],
             [(f0.x, f0.y, Z_LOWER_TOP), (f1.x, f1.y, Z_LOWER_TOP), (k1.x, k1.y, Z_LOWER_TOP), (k0.x, k0.y, Z_LOWER_TOP)], STONE)
        for off, z0, z1 in ((0.22, 0.0, 0.2), (0.11, 0.2, Z_PLINTH)):
            s0, s1 = q(off)
            hexa([(s0.x, s0.y, z0), (s1.x, s1.y, z0), (k1.x, k1.y, z0), (k0.x, k0.y, z0)],
                 [(s0.x, s0.y, z1), (s1.x, s1.y, z1), (k1.x, k1.y, z1), (k0.x, k0.y, z1)], STONE)
        s0, s1 = q(0.13)
        hexa([(s0.x, s0.y, Z_LOWER_TOP), (s1.x, s1.y, Z_LOWER_TOP), (k1.x, k1.y, Z_LOWER_TOP), (k0.x, k0.y, Z_LOWER_TOP)],
             [(s0.x, s0.y, ZB), (s1.x, s1.y, ZB), (k1.x, k1.y, ZB), (k0.x, k0.y, ZB)], TRIM)
        # niches, laid out from the inner end so both sides read the same way
        start = a if sx > 0 else b
        uu = u if sx > 0 else -u
        panel_set(start, uu, n, length, Z_PLINTH + 0.08, 1.9, 0.52, 5)

    # Piers.
    for sx in (-1, 1):
        xo = sx * W2
        xi_lo, xi_hi = sx * (W2 - P_LO), sx * (W2 - P_HI)
        lo, hi = min(xo, xi_lo), max(xo, xi_lo)
        box(lo - 0.08 * (sx < 0), hi + 0.08 * (sx > 0), -0.2, PIER_FRONT + 0.08, 0.0, 0.42, STONE)  # base
        box(lo, hi, -0.2, PIER_FRONT, 0.42, PIER_STEP_Z0, STONE)
        lo2, hi2 = min(xo, xi_hi), max(xo, xi_hi)
        hexa([(lo, -0.2, PIER_STEP_Z0), (hi, -0.2, PIER_STEP_Z0), (hi, PIER_FRONT, PIER_STEP_Z0), (lo, PIER_FRONT, PIER_STEP_Z0)],
             [(lo2, -0.2, PIER_STEP_Z1), (hi2, -0.2, PIER_STEP_Z1), (hi2, PIER_FRONT_HI, PIER_STEP_Z1), (lo2, PIER_FRONT_HI, PIER_STEP_Z1)], STONE)
        box(lo2, hi2, -0.2, PIER_FRONT_HI, PIER_STEP_Z1, PIER_TOP, STONE)
        box(lo2 - 0.06, hi2 + 0.06, -0.2, PIER_FRONT_HI + 0.06, PIER_TOP, PIER_TOP + 0.12, TRIM)        # cap
        box(lo2 - 0.04, hi2 + 0.04, -0.2, PIER_FRONT_HI + 0.04, H - 0.34, H - 0.14, TRIM)               # cornice wraps the pier
        pyramid_local((lo2 + hi2) / 2, (PIER_FRONT_HI - 0.2) / 2, PIER_TOP + 0.12, 0.42, 0.75, STONE)
        # Front of the lower pier: two tiers of tracery panels, then a gablet and pinnacle.
        origin = Vector((lo, PIER_FRONT, 0))
        panel_set(origin, Vector((1, 0, 0)), Vector((0, 1, 0)), hi - lo, 0.58, 1.95, 0.36, 2)
        panel_set(origin, Vector((1, 0, 0)), Vector((0, 1, 0)), hi - lo, 2.42, 3.8, 0.36, 2)
        mid = (lo + hi) / 2
        extrude_front([(lo + 0.08, 4.32), (hi - 0.08, 4.32), (mid, 5.02)], PIER_FRONT, PIER_FRONT + 0.1, TRIM)
        box(lo + 0.04, hi - 0.04, PIER_FRONT, PIER_FRONT + 0.12, 4.22, 4.32, TRIM)
        pyramid_local(mid, PIER_FRONT + 0.05, 4.9, 0.09, 0.65, TRIM)
        for side in (-1, 1):
            pyramid_local(mid + side * (hi - lo) * 0.42, PIER_FRONT + 0.05, 4.32, 0.06, 0.45, TRIM)

    # Corbel heads on the spandrels above the arch shoulders.
    for sx in (-1, 1):
        cx = sx * 3.6
        box(cx - 0.17, cx + 0.17, D, D + 0.28, 8.5, 8.78, TRIM)
        icosphere((cx, D + 0.2, 8.38), 0.16, TRIM)

    # Door surround: moulded frame and the carved lintel frieze.
    fw = 0.17
    for sx in (-1, 1):
        x0, x1 = sorted((sx * DOOR_W2, sx * (DOOR_W2 + fw)))
        box(x0, x1, YD, YD + 0.08, 0.0, DOOR_H + fw, TRIM)
        xi0, xi1 = sorted((sx * DOOR_W2, sx * (DOOR_W2 + 0.05)))
        box(xi0, xi1, YD, YD + 0.12, 0.0, DOOR_H + 0.05, TRIM)
    box(-DOOR_W2 - fw, DOOR_W2 + fw, YD, YD + 0.08, DOOR_H, DOOR_H + fw, TRIM)
    box(-DOOR_W2 - 0.05, DOOR_W2 + 0.05, YD, YD + 0.12, DOOR_H, DOOR_H + 0.05, TRIM)
    box(-RI + 0.02, RI - 0.02, YD, YD + 0.07, 3.35, 3.8, TRIM)
    box(-RI + 0.02, RI - 0.02, YD, YD + 0.1, 3.76, 3.84, TRIM)
    x = -RI + 0.17
    while x < RI - 0.12:
        extrude_front([(x - 0.085, 3.575), (x, 3.66), (x + 0.085, 3.575), (x, 3.49)], YD + 0.07, YD + 0.12, TRIM)
        x += 0.25


def build_doors():
    yb, yf = YD - 0.14, YD - 0.08       # leaf slab
    yr = YD - 0.05                      # raised frame front
    for sx in (-1, 1):
        x_in, x_out = sx * 0.006, sx * DOOR_W2
        lo, hi = sorted((x_in, x_out))
        box(lo, hi, yb, yf, 0.0, DOOR_H, DOOR)
        st_out, st_in = 0.12, 0.10
        so0, so1 = sorted((x_out, x_out - sx * st_out))
        si0, si1 = sorted((x_in, x_in + sx * st_in))
        box(so0, so1, yf, yr, 0.0, DOOR_H, DOOR)
        box(si0, si1, yf, yr, 0.0, DOOR_H, DOOR)
        for z0, z1 in ((0.0, 0.18), (1.42, 1.58), (DOOR_H - 0.14, DOOR_H)):
            box(lo, hi, yf, yr, z0, z1, DOOR)
        # pointed-arch head of the upper panel
        pc = (so0 + so1 + si0 + si1) / 4
        hs = (abs(x_out - x_in) - st_out - st_in) / 2 - 0.03
        outer = [(pc + a, z) for a, z in pointed_arch(hs, 0.5, 2.3)]
        inner = [(pc + a, z) for a, z in pointed_arch(hs - 0.07, 0.44, 2.3)]
        arch_band(outer, inner, Vector((0, 0, 0)), FRONT_U, FRONT_N, yf, yr + 0.006, DOOR)
        # studs along stiles and rails
        for sx_line in ((so0 + so1) / 2, (si0 + si1) / 2):
            z = 0.25
            while z < DOOR_H - 0.15:
                box(sx_line - 0.018, sx_line + 0.018, yr, yr + 0.022, z - 0.018, z + 0.018, DOOR)
                z += 0.16
        for zr in (0.09, 1.5, DOOR_H - 0.07):
            xx = min(so1, si1) + 0.08 if sx < 0 else min(so1, si1) + 0.08
            xa, xb = (min(so0, si0), max(so1, si1))
            xx = xa + 0.16
            while xx < xb - 0.14:
                box(xx - 0.018, xx + 0.018, yr, yr + 0.022, zr - 0.018, zr + 0.018, DOOR)
                xx += 0.16
    # ring handle and boss on the right leaf
    box(0.28, 0.42, yr, yr + 0.02, 1.43, 1.57, DOOR)
    ring = [(0.35 + 0.1 * math.sin(t), yr + 0.035, 1.42 - 0.1 * math.cos(t))
            for t in (2 * math.pi * k / 24 for k in range(24))]
    tube(ring, 0.016, DOOR, sides=6, closed=True)


# ---------------------------------------------------------------------------
# Build, UVs, materials, collision, save and export
# ---------------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
build_porch()
build_archivolts()
build_doors()

bm = ACC.bm
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
bm.normal_update()
uv = bm.loops.layers.uv.new("UVMap")
TILE = 2.5
for f in bm.faces:
    nx, ny, nz = (abs(c) for c in f.normal)
    for loop in f.loops:
        co = loop.vert.co
        if nz >= nx and nz >= ny:
            loop[uv].uv = (co.x / TILE, co.y / TILE)
        elif nx >= ny:
            loop[uv].uv = (co.y / TILE, co.z / TILE)
        else:
            loop[uv].uv = (co.x / TILE, co.z / TILE)
mesh = bpy.data.meshes.new(NAME)
bm.to_mesh(mesh)
bm.free()
for name, col in zip(MAT_NAMES, MAT_COLORS):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*col, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.85 if name != "Door" else 0.45
    bsdf.inputs["Metallic"].default_value = 0.0 if name != "Door" else 0.7
    mesh.materials.append(mat)
portal = bpy.data.objects.new(NAME, mesh)
bpy.context.scene.collection.objects.link(portal)


def hull_object(name, pts_local):
    hb = bmesh.new()
    for p in pts_local:
        hb.verts.new(G(*p))
    bmesh.ops.convex_hull(hb, input=hb.verts)
    m = bpy.data.meshes.new(name)
    hb.to_mesh(m)
    hb.free()
    o = bpy.data.objects.new(name, m)
    bpy.context.scene.collection.objects.link(o)
    o.display_type = 'WIRE'
    return o


def prism(plan, z0, z1):
    return [(x, y, z) for x, y in plan for z in (z0, z1)]


collision = [
    hull_object(f"UCX_{NAME}_00", prism([(-W2, -0.2), (W2, -0.2), (W2, YD), (-W2, YD)], 0.0, H)),
    hull_object(f"UCX_{NAME}_01", prism([(-W2, -0.2), (-W2, PIER_FRONT), (-R0, D), (-RI, YD), (-RI, -0.2)], 0.0, PIER_TOP)),
    hull_object(f"UCX_{NAME}_02", prism([(W2, -0.2), (W2, PIER_FRONT), (R0, D), (RI, YD), (RI, -0.2)], 0.0, PIER_TOP)),
    hull_object(f"UCX_{NAME}_03", prism([(-R0, -0.2), (R0, -0.2), (R0, D + 0.24), (-R0, D + 0.24)], A0 - 0.6, H)),
]

tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
lo = [min(v.co[i] for v in mesh.vertices) for i in range(3)]
hi = [max(v.co[i] for v in mesh.vertices) for i in range(3)]
print(f"PORTAL tris={tris} bounds lo={[round(v, 3) for v in lo]} hi={[round(v, 3) for v in hi]}")

os.makedirs(args.out, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(args.out, "Girona_LeftPortal.blend"))
bpy.ops.object.select_all(action='DESELECT')
for o in [portal] + collision:
    o.select_set(True)
bpy.context.view_layer.objects.active = portal
bpy.ops.export_scene.fbx(
    filepath=os.path.join(args.out, NAME + ".fbx"), use_selection=True, object_types={'MESH'},
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y',
    mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False)
print("EXPORTED")

# ---------------------------------------------------------------------------
# Optional preview renders on the shell, from the photos' camera positions.
# ---------------------------------------------------------------------------
if args.shell and args.renders:
    for o in collision:
        o.hide_render = True
    with bpy.data.libraries.load(args.shell, link=False) as (src, dst):
        dst.objects = list(src.objects)
    for o in dst.objects:
        if o is not None and o.type == 'MESH':
            bpy.context.scene.collection.objects.link(o)
    scene = bpy.context.scene
    for engine in ('BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    try:
        scene.eevee.taa_render_samples = 32
    except AttributeError:
        pass
    world = bpy.data.worlds.new("Sky")
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.55, 0.63, 0.78, 1.0)
    bg.inputs["Strength"].default_value = 0.9
    scene.world = world
    scene.view_settings.exposure = -1.3
    gm = bpy.data.meshes.new("Ground")
    gm.from_pydata([G(-40, -5, 0), G(40, -5, 0), G(40, 60, 0), G(-40, 60, 0)], [], [(0, 1, 2, 3)])
    gmat = bpy.data.materials.new("GroundMat")
    gmat.use_nodes = True
    gmat.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.35, 0.34, 0.32, 1.0)
    gm.materials.append(gmat)
    scene.collection.objects.link(bpy.data.objects.new("Ground", gm))
    sun_data = bpy.data.lights.new("Sun", 'SUN')
    sun_data.energy = 4.0
    sun_data.angle = math.radians(2)
    sun = bpy.data.objects.new("Sun", sun_data)
    scene.collection.objects.link(sun)
    light_dir = G(-8, -10, -14) - G(0, 0, 0)       # from front-right, high
    sun.rotation_euler = light_dir.to_track_quat('-Z', 'Y').to_euler()

    def shot(name, loc, target, lens, rx, ry):
        cd = bpy.data.cameras.new(name)
        cd.lens = lens
        cd.clip_end = 500
        co = bpy.data.objects.new(name, cd)
        scene.collection.objects.link(co)
        co.location = G(*loc)
        co.rotation_euler = (G(*target) - G(*loc)).to_track_quat('-Z', 'Y').to_euler()
        scene.camera = co
        scene.render.resolution_x, scene.render.resolution_y = rx, ry
        scene.render.filepath = os.path.join(args.renders, name + ".png")
        bpy.ops.render.render(write_still=True)

    shot("cmp_left_door", (0.0, 17.0, 1.6), (0.0, 2.5, 5.0), 30, 900, 1200)
    shot("cmp_left_side_door", (2.0, 30.0, 1.7), (0.5, 0.0, 9.5), 22, 900, 1640)
    shot("chk_oblique_left", (-13.0, 13.0, 3.0), (0.0, 1.5, 5.0), 28, 1400, 1000)
    shot("chk_high_right", (13.0, 12.0, 15.0), (0.0, 1.0, 6.0), 28, 1400, 1000)
    shot("chk_door_close", (1.2, 8.0, 1.7), (0.0, 1.7, 2.8), 30, 1400, 1000)
    print("RENDERED")
