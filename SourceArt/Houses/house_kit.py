"""Geometry kit for the Meshy house rebuilds (Blender 5.2, Python). Built on SourceArt/Cathedral/v3/gothic_kit.py.

House space (ledger section 3): metres, origin at the front-left corner of the walls at street level, +x along
the frontage to the right of a viewer facing the front door, the front face on y = 0 and the house towards +y,
z up with z = 0 the street in front of the door.

Walls are built in gothic_kit "wall coordinates" (x along the wall to the viewer's right, y INTO the wall, faces of
the outside pointing -y) and put on each side of the house with Builder.push(facade_frame(...)).

Rules this kit keeps, so the see-through audit stays at 0:
  - every outer wall is an outer face at y = 0, an inner face at y = wall thickness, and reveals joining the two
    round every opening; slabs, ceilings and stair soffits end exactly on wall faces;
  - glass is two-sided (one quad facing out, one facing in), so a window closes the wall from both sides;
  - faces that would sit on another face facing the same way are left out (no z-fighting).
Every function that makes collision returns convex hulls as lists of points (house space); build_house.py turns
them into UCX_ meshes.
"""
import math
import os
import random
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Cathedral", "v3")))
import gothic_kit as gk  # noqa: E402
from gothic_kit import Builder, translate, rot_z, _minus  # noqa: E402,F401

# FBX material slots; each becomes MI_House_<Folder>_<name> in Unreal (ledger section 3)
MATERIALS = ["Stone", "Brick", "Gauged", "Trim", "Door", "Metal", "Glass", "Brass", "Dial", "Clay", "Roof", "Plaster", "Timber",
             "Bronze",          # added by house #1 (clock plate)
             "Lamp"]            # added by house #2 (lit lantern glass); append new slots at the end only
COLORS = {"Stone": (0.62, 0.58, 0.50), "Brick": (0.40, 0.22, 0.16), "Gauged": (0.45, 0.25, 0.18), "Trim": (0.66, 0.63, 0.56),
          "Door": (0.36, 0.31, 0.25), "Metal": (0.08, 0.08, 0.08), "Glass": (0.12, 0.14, 0.18), "Brass": (0.62, 0.48, 0.25),
          "Dial": (0.75, 0.66, 0.48), "Clay": (0.62, 0.38, 0.26), "Roof": (0.30, 0.28, 0.26), "Plaster": (0.80, 0.78, 0.72),
          "Timber": (0.42, 0.30, 0.20), "Bronze": (0.25, 0.18, 0.13), "Lamp": (0.95, 0.90, 0.70)}
UV0 = (0.0, 0.0)            # one texture offset for all flat walls, so brick courses run on round the corners


def facade_frame(side, W, D):
    """(matrix, length) of the wall-coordinate frame of one side of a W x D house: 'front', 'right', 'back', 'left'.
    Wall x runs left to right as a viewer outside that side sees it."""
    p = {"front": ((0.0, 0.0), (W, 0.0)), "right": ((W, 0.0), (W, D)), "back": ((W, D), (0.0, D)), "left": ((0.0, D), (0.0, 0.0))}[side]
    return gk.wall_frame(*p)


def rect(B, a, b, c, d, mat, want, var=0.5, uv_off=UV0):
    """Quad a-b-c-d (corners in order) facing `want`, texture box-projected with a fixed offset."""
    B.grid(np.array([[a, d], [b, c]], dtype=np.float64), mat, want=want, var=var, uv_off=uv_off)


def flat_face(B, x0, x1, z0, z1, mat, holes=(), y=0.0, inward=False, var=0.5, uv_off=UV0, cell=None):
    """Plain wall face on the plane y (wall coordinates) with rectangular holes, facing out (-y) or, with inward,
    towards +y (the inside of an outer wall). cell: split each piece into a grid of about that size, so the vertex
    colours (crevice dirt, streaks) vary across a big wall instead of spreading from its corners."""
    rects = [(x0, x1, z0, z1)]
    for h in holes:
        rects = _minus(rects, h[:4])
    want = (0, 1, 0) if inward else (0, -1, 0)
    for a, b, c, d in rects:
        if b - a > 1e-6 and d - c > 1e-6:
            if cell:
                nu, nv = max(1, int(math.ceil((b - a) / cell))), max(1, int(math.ceil((d - c) / cell)))
                xs, zs = np.linspace(a, b, nu + 1), np.linspace(c, d, nv + 1)
                P = np.stack(np.meshgrid(xs, np.full(1, y), zs, indexing="ij"), axis=-1)[:, 0]
                B.grid(P, mat, want=want, var=var, uv_off=uv_off)
            else:
                rect(B, (a, y, c), (b, y, c), (b, y, d), (a, y, d), mat, want, var=var, uv_off=uv_off)


def reveal(B, hole, y0, y1, mat, sides="lrtb", var=0.5):
    """The faces of a rectangular opening between the planes y0 and y1, facing into the opening."""
    x0, x1, z0, z1 = hole[:4]
    if "l" in sides:
        rect(B, (x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1), mat, (1, 0, 0), var)
    if "r" in sides:
        rect(B, (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1), mat, (-1, 0, 0), var)
    if "t" in sides:
        rect(B, (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), mat, (0, 0, -1), var)
    if "b" in sides:
        rect(B, (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), mat, (0, 0, 1), var)


def block(B, x0, x1, y0, y1, z0, z1, mat, faces="xXyYzZ", var=0.5):
    """Axis-aligned box (wall or house coordinates) with the chosen faces; textures box-projected."""
    B.box(x0, x1, y0, y1, z0, z1, mat, var=var, faces=faces)


def rubble_face(B, x0, x1, z0, z1, mat, holes=(), course=0.39, length=(0.45, 1.0), chamfer=0.03, jitter=0.012, seed=1):
    """Coursed pale rubble or ashlar: gothic_kit.block_wall with rounded (deeply chamfered) blocks."""
    gk.block_wall(B, x0, x1, z0, z1, mat, holes=holes, course=course, length=length, chamfer=chamfer, jitter=jitter, seed=seed)
    yb = chamfer + 0.002              # the joint plate carried 8 cm past the top and bottom of the zone, behind whatever
    for za, zb in ((z1, z1 + 0.08), (z0 - 0.08, z0)):     # comes next: closes the slit the chamfer leaves at the edge
        flat_face(B, x0, x1, za, zb, mat, holes=holes, y=yb, var=0.2)


def wall_core(B, L, z0, z1, t, holes=(), mat="Plaster"):
    """Hidden plates in the middle of a hollow outer wall (both ways, 2 mm apart) and lids at its top and bottom, so
    a sight line that slips through a joint or exactly through a corner edge still ends on something."""
    flat_face(B, 0.0, L, z0, z1, mat, holes=holes, y=t / 2 - 0.001, var=0.2)
    flat_face(B, 0.0, L, z0, z1, mat, holes=holes, y=t / 2 + 0.001, inward=True, var=0.2)
    rect(B, (0.0, 0.0, z1 - 0.005), (L, 0.0, z1 - 0.005), (L, t, z1 - 0.005), (0.0, t, z1 - 0.005), mat, (0, 0, -1), 0.2)
    rect(B, (0.0, 0.0, z0 - 0.01), (L, 0.0, z0 - 0.01), (L, t, z0 - 0.01), (0.0, t, z0 - 0.01), mat, (0, 0, 1), 0.2)


# ---------------------------------------------------------------------------------------------
# Openings
# ---------------------------------------------------------------------------------------------
def sash_window(B, x0, x1, z0, z1, frame, glass, cols, rows, y=0.12, depth=0.08, fw=0.07, bar=0.024, meeting=True):
    """Painted timber window in the plane y..y+depth (wall coordinates): frame, two-sided glass in the middle of the
    frame, glazing bars making cols x rows panes, and (sash windows) a heavier meeting rail at mid-height."""
    yc = y + depth / 2
    block(B, x0, x0 + fw, y, y + depth, z0, z1, frame, faces="Xyz" + "Y")
    block(B, x1 - fw, x1, y, y + depth, z0, z1, frame, faces="xyz" + "Y")
    block(B, x0 + fw, x1 - fw, y, y + depth, z0, z0 + fw * 1.3, frame, faces="yYzZ")
    block(B, x0 + fw, x1 - fw, y, y + depth, z1 - fw, z1, frame, faces="yYz")
    gx0, gx1, gz0, gz1 = x0 + fw, x1 - fw, z0 + fw * 1.3, z1 - fw
    for yy, want in ((yc - 0.001, (0, -1, 0)), (yc + 0.001, (0, 1, 0))):    # two sides, 2 mm apart (a ray tracer
        rect(B, (gx0, yy, gz0), (gx1, yy, gz0), (gx1, yy, gz1), (gx0, yy, gz1), glass, want, var=0.5)   # sees only one of two coincident quads)
    for i in range(1, cols):
        x = gx0 + (gx1 - gx0) * i / cols
        block(B, x - bar / 2, x + bar / 2, yc - 0.018, yc + 0.018, gz0, gz1, frame, faces="xXyY")
    mid = rows // 2 if (meeting and rows % 2 == 0 and rows > 1) else None
    for j in range(1, rows):
        z = gz0 + (gz1 - gz0) * j / rows
        h = bar * (1.8 if j == mid else 0.5)
        dy = 0.03 if j == mid else 0.018
        block(B, gx0, gx1, yc - dy, yc + dy, z - h, z + h, frame, faces="yYzZ")


def sill(B, x0, x1, z_top, mat, h=0.17, proud=0.07, back=0.12, var=0.5):
    """Stone sill: its top is the bottom of the opening (up to y = back) and stands `proud` in front of the wall."""
    block(B, x0, x1, -proud, back, z_top - h, z_top, mat, faces="xXyzZ", var=var)


def lintel(B, x0, x1, z0, z1, mat, proud=0.035, var=0.5):
    """Plain stone lintel or head block standing a little proud of the wall."""
    block(B, x0, x1, -proud, 0.0, z0, z1, mat, faces="xXyzZ", var=var)


def gauged_arch(B, x0, x1, z0, z1, mat, n=9, splay=0.10, proud=0.018, joint=0.006, var=0.5):
    """Flat arch of gauged bricks over an opening: n wedge bricks whose joints fan out from a point below, the arch
    `splay` wider on each side at the top than at the bottom."""
    cx, half = (x0 + x1) / 2, (x1 - x0) / 2
    k = (half + splay) / half
    rng = random.Random(int(x0 * 1000) + int(z0 * 100))
    for i in range(n):
        a, b = x0 + (x1 - x0) * i / n, x0 + (x1 - x0) * (i + 1) / n
        ta, tb = cx + (a - cx) * k, cx + (b - cx) * k
        a, b, ta, tb = a + joint / 2, b - joint / 2, ta + joint / 2, tb - joint / 2
        front = [(a, -proud, z0), (b, -proud, z0), (tb, -proud, z1), (ta, -proud, z1)]
        back = [(a, 0.0, z0), (b, 0.0, z0), (tb, 0.0, z1), (ta, 0.0, z1)]
        tone = min(1.0, max(0.0, rng.gauss(0.5, 0.12)))
        B.fan(front, mat, (0, -1, 0), var=tone)
        for j in range(4):
            p, q = front[j], front[(j + 1) % 4]
            mid = np.mean(front, axis=0)
            e = (np.array(p) + np.array(q)) / 2 - mid
            rect(B, p, q, back[(j + 1) % 4], back[j], mat, (e[0], 0.0, e[2]), var=tone)


def panel_leaf(B, w, h, mat, t=0.06, cols=2, rows=3, frame=0.13, sunk=0.014, var=0.5):
    """Panelled door leaf in its own frame: x 0..w (hinge at x = 0), y 0..t (face 1 at y = 0), z 0..h. Panels on both
    faces are sunk by `sunk`; the top row is shorter, as on a six-panel door."""
    weights = [1.0] * rows
    weights[0] = 0.6 if rows > 2 else 1.0
    weights = weights[::-1]                      # weights from the bottom up: the shortest row is at the top
    inner_h = h - frame * (rows + 1)
    zs, z = [], frame
    for wt in weights:
        ph = inner_h * wt / sum(weights)
        zs.append((z, z + ph))
        z += ph + frame
    pw = (w - frame * (cols + 1)) / cols
    holes = [(frame + c * (pw + frame), frame + c * (pw + frame) + pw, za, zb) for c in range(cols) for za, zb in zs]
    for y, inward in ((0.0, False), (t, True)):
        flat_face(B, 0.0, w, 0.0, h, mat, holes=holes, y=y, inward=inward, var=var)
        yp = y + sunk if not inward else y - sunk
        for hx0, hx1, hz0, hz1 in holes:
            flat_face(B, hx0, hx1, hz0, hz1, mat, y=yp, inward=inward, var=var)
            reveal(B, (hx0, hx1, hz0, hz1), min(y, yp), max(y, yp), mat, var=var)
    block(B, 0.0, w, 0.0, t, 0.0, h, mat, faces="xXzZ", var=var)


# ---------------------------------------------------------------------------------------------
# Clock (the Clockmaker's facade clock: dark square plate, brass ring, cream dial, black numerals)
# ---------------------------------------------------------------------------------------------
def clock_dial(B, cx, cz, half, r, plate, ring_mat, dial_mat, marks, brass=None, hour=10.0, minute=9.0, plate_proud=0.04):
    """Square plate (half size `half`, standing plate_proud in front of the wall) with brass corner bosses, a brass
    ring round a dial of radius r, a darker brass centre, Roman numerals and minute ticks in `marks`, brass hands.
    Returns the plate as (x0, x1, z0, z1) and a collision hull."""
    brass = brass or ring_mat
    block(B, cx - half, cx + half, -plate_proud, 0.0, cz - half, cz + half, plate, faces="xXyzZ", var=0.45)
    B.push(translate(0.0, -plate_proud, 0.0))
    border = [(cx - half + 0.05, 0.0, cz - half + 0.05), (cx - half + 0.05, 0.0, cz + half - 0.05),
              (cx + half - 0.05, 0.0, cz + half - 0.05), (cx + half - 0.05, 0.0, cz - half + 0.05)]
    B.sweep(border, [[(0.035, 0.0), (0.035, 0.012)], gk.half_round(0.035, 0.0, 0.012, n=6)], plate, b=(0, -1, 0), side=-1, closed=True)
    for sx in (-1, 1):
        for sz in (-1, 1):
            B.push(translate(cx + sx * (half - 0.2), 0.0, cz + sz * (half - 0.2)) @ gk.FACE_OUT)
            gk.rosette(B, brass, 0.12, 0.04, petals=6, layers=2)
            B.pop()
    ang = np.radians(np.linspace(0.0, 360.0, 97)[:-1])
    B.fan([(cx + r * math.cos(a), -0.012, cz + r * math.sin(a)) for a in ang], dial_mat, (0, -1, 0), var=0.5)
    B.fan([(cx + r * 0.52 * math.cos(a), -0.016, cz + r * 0.52 * math.sin(a)) for a in ang], brass, (0, -1, 0), var=0.3)
    gk.ring(B, ring_mat, cx, cz, r, [[(0.14, 0.0), (0.14, 0.055), (0.11, 0.055)], gk.half_round(0.11, 0.03, 0.055),
                                     [(0.03, 0.055), (0.0, 0.055), (0.0, 0.012)]], segs=96)
    gk.ring(B, ring_mat, cx, cz, r * 0.52, [[(0.025, 0.016), (0.025, 0.03), (-0.01, 0.03), (-0.01, 0.016)]], segs=72)
    h = r * 0.30
    w, adv = h * 0.105, {"I": h * 0.30, "V": h * 0.60, "X": h * 0.62}
    for hr, text in gk.ROMAN.items():
        a = math.radians(90.0 - 30.0 * hr)
        out = np.array([math.cos(a), math.sin(a)])
        right = np.array([math.sin(a), -math.cos(a)])
        base = np.array([cx, cz]) + out * (r * 0.60)
        x = -sum(adv[ch] for ch in text) / 2
        for ch in text:
            o = base + right * (x + adv[ch] / 2)
            g = adv[ch] * 0.36
            strokes = ([((0.0, 0.0), (0.0, h))] if ch == "I" else [((-g, h), (0.0, 0.0)), ((g, h), (0.0, 0.0))] if ch == "V"
                       else [((-g, 0.0), (g, h)), ((-g, h), (g, 0.0))])
            for (u0, v0), (u1, v1) in strokes:
                gk.bar(B, marks, o + right * u0 + out * v0, o + right * u1 + out * v1, w, -0.012, -0.03)
            x += adv[ch]
        half_t = sum(adv[ch] for ch in text) / 2
        for v in (0.0, h):
            gk.bar(B, marks, base + out * v - right * half_t, base + out * v + right * half_t, w * 0.55, -0.012, -0.028)
    for k in range(60):
        a = math.radians(6.0 * k)
        e = np.array([math.cos(a), math.sin(a)])
        gk.bar(B, marks, np.array([cx, cz]) + e * r * 0.92, np.array([cx, cz]) + e * r * 0.96, 0.012 if k % 5 else 0.03, -0.012, -0.024)
    for value, length, width in (((hour % 12) + minute / 60.0, r * 0.5, 0.075), (minute / 5.0, r * 0.82, 0.05)):
        a = math.radians(90.0 - 30.0 * value)
        e = np.array([math.cos(a), math.sin(a)])
        gk.bar(B, brass, np.array([cx, cz]) - e * r * 0.14, np.array([cx, cz]) + e * length, width, -0.035, -0.055)
    B.push(translate(cx, -0.055, cz) @ gk.FACE_OUT)
    gk.rosette(B, brass, 0.09, 0.03, petals=8, layers=1)
    B.pop()
    B.pop()
    return (cx - half, cx + half, cz - half, cz + half)


# ---------------------------------------------------------------------------------------------
# Courses, cornice, roof
# ---------------------------------------------------------------------------------------------
def plan_loop(W, D, inset=0.0):
    """The outline of a W x D house, listed so that every side runs left to right for a viewer outside it."""
    return [(inset, inset), (W - inset, inset), (W - inset, D - inset), (inset, D - inset)]


def band_profile(h, proud):
    """String course: plain band with a weathered (sloping) top; u out of the wall, v up."""
    return [[(0.0, 0.0), (proud, 0.0), (proud, h * 0.72), (proud * 0.8, h * 0.86), (0.0, h)]]


def band_loop(B, W, D, z0, z1, proud, mat, sides=None):
    """A string course right round the house (or along the listed sides only, with flat ends)."""
    prof = band_profile(z1 - z0, proud)
    loop = plan_loop(W, D)
    if sides is None:
        B.sweep([(x, y, z0) for x, y in loop], prof, mat, closed=True)
        return
    for side in sides:
        m, L = facade_frame(side, W, D)
        B.push(m)
        B.sweep([(0.0, 0.0, z0), (L, 0.0, z0)], prof, mat, ghost=((-1.0, 0.0, z0), (L + 1.0, 0.0, z0)))
        B.pop()


def cornice_loop(B, W, D, z, mat, wall=0.4, dentils=True):
    """The crowning entablature of gothic_kit (architrave, frieze, dentils, corona; ENTABLATURE_H tall, CORNICE_OUT
    proud) right round the house, with the flat top of the parapet from its outer edge to the parapet's inner face.
    Returns the height of its top."""
    loop = plan_loop(W, D)
    B.sweep([(x, y, z) for x, y in loop], gk.entablature_profile(), mat, closed=True)
    zt = z + gk.ENTABLATURE_H
    o, i = plan_loop(W, D, -gk.CORNICE_OUT), plan_loop(W, D, wall)
    for k in range(4):
        a, b = o[k], o[(k + 1) % 4]
        c, d = i[(k + 1) % 4], i[k]
        B.fan([(a[0], a[1], zt), (b[0], b[1], zt), (c[0], c[1], zt), (d[0], d[1], zt)], mat, (0, 0, 1), var=0.5)
    if dentils:
        for side in ("front", "right", "back", "left"):
            m, L = facade_frame(side, W, D)
            B.push(m)
            n = max(1, int(L / 0.085))
            for k in range(n):
                xc = (k + 0.5) * L / n
                if 0.06 < xc < L - 0.06:
                    block(B, xc - 0.026, xc + 0.026, -0.105, -0.05, z + 0.605, z + 0.685, mat, faces="xXyz")
            B.pop()
    return zt


def chimney(B, x0, x1, y0, y1, z0, z1, body, cap, pot, pots=2, cap_h=0.26, pot_h=0.73, pot_r=0.13):
    """Chimney stack standing on z0 (house coordinates) with a projecting cap and clay pots in a row along the
    longer side. Returns its collision hull."""
    block(B, x0, x1, y0, y1, z0, z1, body, faces="xXyY")
    block(B, x0 - 0.06, x1 + 0.06, y0 - 0.06, y1 + 0.06, z1, z1 + cap_h, cap, faces="xXyYzZ")
    along_y = (y1 - y0) >= (x1 - x0)
    for k in range(pots):
        t = (k + 0.5) / pots
        cxp = (x0 + x1) / 2 if along_y else x0 + (x1 - x0) * t
        cyp = y0 + (y1 - y0) * t if along_y else (y0 + y1) / 2
        r, H = pot_r, pot_h
        prof = [(r * 1.12, 0.0), (r * 1.12, 0.06), (r, 0.1), (r * 0.92, H * 0.75), (r * 1.05, H * 0.86), (r * 1.08, H),
                (r * 0.8, H), (r * 0.8, H * 0.55)]
        B.lathe([prof], pot, segs=16, center=(cxp, cyp, z1 + cap_h), smooth=True)
    return [box_pts(x0, x1, y0, y1, z0, z1 + cap_h)]


# ---------------------------------------------------------------------------------------------
# Inside: slabs, the dog-leg stair against the back wall, guards
# ---------------------------------------------------------------------------------------------
def box_pts(x0, x1, y0, y1, z0, z1):
    return [(x, y, z) for x in (x0, x1) for y in (y0, y1) for z in (z0, z1)]


def slab_rects(B, rects, z_top, thick, top_mat, bottom_mat, top=True, bottom=True, var=0.5):
    """Floor slab over plan rectangles (x0, x1, y0, y1): boards on top at z_top, a plaster ceiling below."""
    for x0, x1, y0, y1 in rects:
        if top:
            rect(B, (x0, y0, z_top), (x1, y0, z_top), (x1, y1, z_top), (x0, y1, z_top), top_mat, (0, 0, 1), var)
        if bottom:
            zb = z_top - thick
            rect(B, (x0, y0, zb), (x1, y0, zb), (x1, y1, zb), (x0, y1, zb), bottom_mat, (0, 0, -1), var)


def flight(B, xs, xe, y0, y1, zs, ze, n, tread, side, thick=0.3, solid_to=None, side_faces="", var=0.5):
    """Straight flight along x in the row y0..y1, from the landing at (xs, zs) to the landing at (xe, ze): n risers,
    the first at xs and the last at xe. Underneath, a soffit parallel to the line (xs, zs)-(xe, ze), `thick` below
    it, so it meets the slabs of both landings; or, with solid_to, solid down to that floor level. side_faces:
    'y' / 'Y' make the side faces at y0 / y1 (the others stand against walls). Returns the walkable ramp as a
    collision hull."""
    d = 1.0 if xe > xs else -1.0
    r = (ze - zs) / n
    g = abs(xe - xs) / (n - 1)
    xi = [xs + d * g * i for i in range(n)]
    for i in range(n):
        za, zb = zs + r * i, zs + r * (i + 1)
        rect(B, (xi[i], y0, za), (xi[i], y1, za), (xi[i], y1, zb), (xi[i], y0, zb), tread, (-d, 0, 0), var)
        if i < n - 1:
            rect(B, (xi[i], y0, zb), (xi[i + 1], y0, zb), (xi[i + 1], y1, zb), (xi[i], y1, zb), tread, (0, 0, 1), var)

    def under(x):
        if solid_to is not None:
            return solid_to
        return zs - thick + (x - xs) / (xe - xs) * (ze - zs)
    if solid_to is None:
        rect(B, (xs, y0, under(xs)), (xe, y0, under(xe)), (xe, y1, under(xe)), (xs, y1, under(xs)), side, (0, 0, -1), var)
    for key, y, want in (("y", y0, (0, -1, 0)), ("Y", y1, (0, 1, 0))):
        if key not in side_faces:
            continue
        for i in range(n - 1):
            ztop = zs + r * (i + 1)
            a, b = xi[i], xi[i + 1]
            rect(B, (a, y, under(a)), (b, y, under(b)), (b, y, ztop), (a, y, ztop), side, want, var)
    if solid_to is not None:
        return [(x, y, z) for x in (xs, xe) for y in (y0, y1) for z in (solid_to, (zs if x == xs else ze))]
    return [(x, y, z) for x in (xs, xe) for y in (y0, y1) for z in ((zs if x == xs else ze), under(x))]


def prism_bar(B, x0, z0, x1, z1, y, half_w, h, mat, ends=True, var=0.5):
    """Bar along x from (x0, z0) to (x1, z1) (its bottom edge), h tall and 2 * half_w wide round y: handrails, strings."""
    c = np.array([(x0 + x1) / 2, y, (z0 + z1) / 2 + h / 2])
    p = [(x0, y - half_w, z0), (x1, y - half_w, z1), (x1, y + half_w, z1), (x0, y + half_w, z0)]
    q = [(x, yy, z + h) for x, yy, z in p]
    rect(B, q[0], q[1], q[2], q[3], mat, ("from", c), var)
    rect(B, p[0], p[1], p[2], p[3], mat, ("from", c), var)
    rect(B, p[0], p[1], q[1], q[0], mat, ("from", c), var)
    rect(B, p[3], p[2], q[2], q[3], mat, ("from", c), var)
    if ends:
        rect(B, p[0], p[3], q[3], q[0], mat, ("from", c), var)
        rect(B, p[1], p[2], q[2], q[1], mat, ("from", c), var)


def balustrade(B, x0, x1, y, z0a, z0b, h, rail, baluster, cap_at=None, pitch=0.12, var=0.5):
    """Guard along x at y: balusters and a handrail h above its foot, the foot rising from z0a (at x0) to z0b (at x1).
    A level guard (z0a == z0b) also gets a base string. cap_at: a slab bottom above a flight; balusters and rail
    stop where the rail would meet it. Returns the x where the rail ends."""
    slope = (z0b - z0a) / (x1 - x0)

    def foot(x):
        return z0a + (x - x0) * slope
    xe = x1
    if cap_at is not None and z0b + h > cap_at - 0.02:
        if z0a + h >= cap_at - 0.02:
            return x0
        xe = x0 + (cap_at - 0.02 - (z0a + h)) / slope
    n = max(1, int((xe - x0) / pitch))
    for k in range(n + 1):
        x = x0 + 0.04 + (xe - x0 - 0.08) * k / n
        block(B, x - 0.016, x + 0.016, y - 0.016, y + 0.016, foot(x), foot(x) + h - 0.06, baluster, faces="xXyY", var=var)
    prism_bar(B, x0, foot(x0) + h - 0.07, xe, foot(xe) + h - 0.07, y, 0.035, 0.07, rail, var=var)
    if abs(slope) < 1e-9:
        prism_bar(B, x0, z0a, xe, z0a, y, 0.03, 0.05, rail, var=var)
    return xe


def dogleg_stair(B, C, floors, risers, x_left, x_half, x_land, x_right, y_back, fw=1.2, spine=0.12, thick=0.3,
                 ceiling=None, guard=1.0, tread="Timber", side="Plaster", rail="Timber", baluster="Metal"):
    """Dog-leg stair against the back wall, the same on every storey. Flight A runs along the back wall from the floor
    landing (x_land..x_right, both rows) to the half landing (x_left..x_half); flight B comes back in the front row
    to the next floor landing. A spine wall stands between the rows from the ground to `ceiling`. The ground storey is
    solid masonry under its flights and half landing. Every storey gets guards along the front of the stair zone: on
    the half landing, along flight B, and on the next floor round the well; on the top floor also across the back
    row. Builds into B (stair, guards) and C (unused: kept for a separate mesh), returns (well rect, collision hulls).
    floors: floor levels F0..Fn (Fn is the top floor, no flight from it); risers: one count per storey (even)."""
    hulls = []
    y_sp1 = y_back - fw
    y_sp0 = y_sp1 - spine
    y_front = y_sp0 - fw
    ceiling = ceiling if ceiling is not None else floors[-1] + 3.0
    for k in range(len(floors) - 1):
        F0, F1, n = floors[k], floors[k + 1], risers[k]
        zh = F0 + (F1 - F0) / 2
        nf = n // 2
        solid = F0 if k == 0 else None
        slab_above = F1 - thick
        # flight A: back row, from the floor landing (x_land) to the half landing (x_half)
        hulls.append(flight(B, x_land, x_half, y_sp1, y_back, F0, zh, nf, tread, side, thick=thick, solid_to=solid))
        # half landing
        if k == 0:
            block(B, x_left, x_half, y_front, y_back, F0, zh, side, faces="y")
            rect(B, (x_left, y_front, zh), (x_half, y_front, zh), (x_half, y_back, zh), (x_left, y_back, zh), tread, (0, 0, 1))
            hulls.append(box_pts(x_left, x_half, y_front, y_back, F0 - 0.05, zh))
        else:
            slab_rects(B, [(x_left, x_half, y_front, y_back)], zh, thick, tread, side)
            rect(B, (x_left, y_front, zh - thick), (x_half, y_front, zh - thick), (x_half, y_front, zh), (x_left, y_front, zh), side, (0, -1, 0))
            hulls.append(box_pts(x_left, x_half, y_front, y_back, zh - thick, zh))
        # flight B: front row, from the half landing back to the next floor landing
        hulls.append(flight(B, x_half, x_land, y_front, y_sp0, zh, F1, nf, tread, side, thick=thick, solid_to=solid, side_faces="y"))
        if k == 0:                                     # the solid under flight B ends in a face towards the ground-floor landing
            rect(B, (x_land, y_front, F0), (x_land, y_sp0, F0), (x_land, y_sp0, F1 - (F1 - zh) / nf), (x_land, y_front, F1 - (F1 - zh) / nf), side, (1, 0, 0))
        # guards: half landing, flight B (under the next slab), and round the well on the next floor
        yg = y_front + 0.03
        balustrade(B, x_left, x_half, yg, zh, zh, guard, rail, baluster, cap_at=slab_above)
        balustrade(B, x_half, x_land, yg, zh, F1, guard, rail, baluster, cap_at=slab_above)
        hulls.append(box_pts(x_left, x_half, y_front, y_front + 0.06, zh, slab_above))
        slope = (F1 - zh) / (x_land - x_half)
        xc = x_half + (slab_above - zh) / slope
        if xc > x_half + 0.05:
            hulls.append([(x_half, y, zh) for y in (y_front, y_front + 0.06)] + [(x_half, y, slab_above) for y in (y_front, y_front + 0.06)]
                         + [(xc, y, slab_above) for y in (y_front, y_front + 0.06)])
        top_of_storey = floors[k + 2] - thick if k + 2 < len(floors) else ceiling
        balustrade(B, x_left, x_land, y_front - 0.05, F1, F1, guard, rail, baluster)
        hulls.append(box_pts(x_left, x_land, y_front - 0.08, y_front, F1, top_of_storey))
        if k + 2 == len(floors):                       # top floor: across the back row too, where no flight goes on
            balustrade_y(B, y_sp1, y_back, x_land + 0.05, F1, guard, rail, baluster)
            hulls.append(box_pts(x_land, x_land + 0.08, y_sp1, y_back, F1, ceiling))
    # spine wall
    block(B, x_half, x_land, y_sp0, y_sp1, floors[0], ceiling, side, faces="xXyY")
    hulls.append(box_pts(x_half, x_land, y_sp0, y_sp1, floors[0], ceiling))
    return (x_left, x_land, y_front, y_back), hulls


def balustrade_y(B, y0, y1, x, z, h, rail, baluster, pitch=0.12):
    """A level guard along y at x (for the top floor's back row)."""
    B.push(translate(x, 0.0, 0.0) @ rot_z(90.0))
    balustrade(B, y0, y1, 0.0, z, z, h, rail, baluster, pitch=pitch)
    B.pop()


# ---------------------------------------------------------------------------------------------
# Added by house #2 (GuildmastersRowhouse): mouldings swept along a facade, architraves, an iron-framed studded
# door leaf, wall lanterns, consoles, gable copings, roof ribs and a diamond lattice
# ---------------------------------------------------------------------------------------------
def moulding_run(B, x0, x1, z, prof, mat, y=0.0, caps=True, hub_v=None, var=0.5):
    """A straight moulding (cornice, string course, hood, plinth cap) along wall x from x0 to x1 on the plane y
    (wall coordinates). prof = [(u, v), ...]: u out of the plane (towards -y), v up from z, listed from a point on
    the plane at the bottom, round the outside, to a point on the plane at the top (so the outside is on the right).
    With caps the flat ends are closed by fans around a hub on the plane at height hub_v (default: half the top);
    every profile point must be visible from the hub (true for profiles that only grow outwards going up)."""
    B.sweep([(x0, y, z), (x1, y, z)], [prof], mat, var=var)
    if caps:
        hv = hub_v if hub_v is not None else max(v for _u, v in prof) / 2
        for xe, nx in ((x0, -1.0), (x1, 1.0)):
            B.fan([(xe, y - u, z + v) for u, v in prof], mat, (nx, 0.0, 0.0), var=var, hub=(xe, y, z + hv))


def frame_run(B, x0, x1, z0, z1, prof, mat, y=0.0, var=0.5):
    """Architrave up both jambs and over the head of the opening x0..x1, z0..z1 (wall coordinates) on the plane y.
    prof = [(u, v), ...]: u away from the opening edge, v out of the wall, listed from the outer edge to the opening
    edge (gothic_kit.frame_profile format). The jambs end flat at z0: stand them on a sill, base or pedestal."""
    B.sweep([(x0, y, z0), (x0, y, z1), (x1, y, z1), (x1, y, z0)], [prof], mat, b=(0, -1, 0), side=-1, var=var)


def studded_leaf(B, w, h, frame_mat, panel_mat, rows, t=0.08, stile=(0.28, 0.28), sunk=0.03, stud=0.035, pitch=0.13,
                 ring=None, var=0.5):
    """Iron-framed timber door leaf: x 0..w, y 0..t (outside face at y = 0), z 0..h. One column of timber panels
    sunk `sunk` into the iron frame on both faces; rows = [(z0, z1), ...] of the panels; stile = (width at x = 0,
    width at x = w). Iron studs (cubes `stud` wide, half of them proud) along the middle of both stiles and of every
    rail on the outside; ring = (x, z, r): a ring knocker hanging from a boss, on the outside."""
    px0, px1 = stile[0], w - stile[1]
    holes = [(px0, px1, a, b) for a, b in rows]
    for y, inward in ((0.0, False), (t, True)):
        flat_face(B, 0.0, w, 0.0, h, frame_mat, holes=holes, y=y, inward=inward, var=var)
        yp = y + sunk if not inward else y - sunk
        for hx0, hx1, hz0, hz1 in holes:
            flat_face(B, hx0, hx1, hz0, hz1, panel_mat, y=yp, inward=inward, var=var)
            reveal(B, (hx0, hx1, hz0, hz1), min(y, yp), max(y, yp), frame_mat, var=var)
    block(B, 0.0, w, 0.0, t, 0.0, h, frame_mat, faces="xXzZ", var=var)
    s = stud / 2
    pts = []
    for xc in (px0 / 2, w - stile[1] / 2):
        n = max(1, int((h - 0.16) / pitch))
        pts += [(xc, 0.08 + (h - 0.16) * k / n) for k in range(n + 1)]
    edges = [0.0] + [v for ab in rows for v in ab] + [h]
    for k in range(0, len(edges), 2):
        zc = (edges[k] + edges[k + 1]) / 2
        n = max(1, int((px1 - px0 - 0.1) / pitch))
        pts += [(px0 + 0.05 + (px1 - px0 - 0.1) * j / n, zc) for j in range(n + 1)]
    for xc, zc in pts:
        block(B, xc - s, xc + s, -s, 0.0, zc - s, zc + s, frame_mat, faces="xXyzZ", var=var)
    if ring:
        rx, rz, r = ring
        block(B, rx - 0.045, rx + 0.045, -0.03, 0.0, rz + r - 0.03, rz + r + 0.06, frame_mat, faces="xXyzZ", var=var)
        a = np.radians(np.linspace(0.0, 360.0, 25)[:-1])
        B.tube([(rx + r * math.cos(q), -0.035, rz + r * math.sin(q)) for q in a], 0.016, frame_mat, sides=8, closed=True, var=var)


def wall_lantern(B, cx, y_wall, z_foot, out, iron, glass, var=0.5):
    """Iron wall lantern on the wall plane y_wall (wall coordinates, the wall facing -y): a back plate, an arm curving
    out and up, and a six-sided lantern standing on it, its foot at (cx, y_wall - out, z_foot): a tapered bottom, a
    flared glazed body in `glass` with iron corner posts, a wide cap rim, a pyramid cap and a ball finial."""
    yc = y_wall - out
    block(B, cx - 0.07, cx + 0.07, y_wall - 0.05, y_wall, z_foot - 0.66, z_foot - 0.06, iron, faces="xXyzZ", var=var)
    a = np.radians(np.linspace(0.0, 90.0, 10))
    rise = 0.40
    B.tube([(cx, y_wall - 0.05 - (out - 0.05) * math.sin(q), z_foot - 0.06 - rise + rise * (1 - math.cos(q))) for q in a],
           0.045, iron, sides=8, var=var)
    c = (cx, yc, z_foot)
    B.lathe([[(0.0, -0.08), (0.06, -0.06), (0.12, 0.06), (0.155, 0.18)]], iron, segs=6, center=c, smooth=False, var=var)
    B.lathe([[(0.155, 0.18), (0.24, 0.76)]], glass, segs=6, center=c, smooth=False, var=var)
    B.lathe([[(0.24, 0.76), (0.30, 0.78), (0.30, 0.85), (0.24, 0.87), (0.07, 1.03), (0.04, 1.03), (0.04, 1.09)]],
            iron, segs=6, center=c, smooth=False, var=var)
    ball = [(0.06 * math.sin(q), 1.15 - 0.06 * math.cos(q)) for q in np.radians(np.linspace(0.0, 180.0, 7))]
    B.lathe([ball[::-1]], iron, segs=8, center=c, smooth=True, var=var)
    for k in range(6):
        q = math.radians(60.0 * k)
        e = np.array([math.cos(q), math.sin(q), 0.0])
        B.tube([np.array(c) + e * 0.16 + (0, 0, 0.18), np.array(c) + e * 0.245 + (0, 0, 0.76)], 0.014, iron, sides=6, var=var)


def console(B, xc, y_wall, z_top, mat, out=0.28, height=0.78, width=0.3, var=0.5):
    """Scroll console under a hood or cornice (wall coordinates, the wall plane y_wall facing -y): from the side an S
    scroll, a big volute at the top standing `out` from the wall and a small one at the foot, `height` below z_top;
    from the front a solid upright block `width` wide."""
    block(B, xc - width / 2, xc + width / 2, y_wall - 0.09, y_wall, z_top - height, z_top, mat, faces="xXyzZ", var=var)
    B.push(translate(xc, y_wall, 0.0) @ rot_z(-90.0))
    r0, r1 = 0.12, 0.06
    gk.scroll(B, mat, (out - r0, z_top - r0 - 0.03), r0, (0.06 + r1, z_top - height + r1 + 0.02), r1, sx=1, y=0.0,
              width=width, thick=0.07, var=var)
    B.pop()


def gable_coping(B, x0, x1, outline, h, mat, inner_want, var=0.5):
    """Coping on a gable wall standing in x0..x1 (house coordinates) along the roof outline [(y, z), ...] (front to
    back): the strip from the outline up to outline + h. Makes its top (a vertical step of the outline gives an
    upright face), the face towards the roof (at x1 when inner_want = (1, 0, 0), else at x0) and both ends. Its
    outer face is the wall's own face, carried up to outline + h."""
    xin = x1 if inner_want[0] > 0 else x0
    for (ya, za), (yb, zb) in zip(outline, outline[1:]):
        up = (0.0, -(zb - za), yb - ya)
        rect(B, (x0, ya, za + h), (x1, ya, za + h), (x1, yb, zb + h), (x0, yb, zb + h), mat, up, var)
        if abs(yb - ya) > 1e-6:
            rect(B, (xin, ya, za), (xin, yb, zb), (xin, yb, zb + h), (xin, ya, za + h), mat, inner_want, var)
        elif zb > za:                   # an upright step up: close the end of the lower run below the step's own face
            rect(B, (x0, ya, za), (x1, ya, za), (x1, ya, za + h), (x0, ya, za + h), mat, (0, -1, 0), var)
    (y0, z0), (y1, z1) = outline[0], outline[-1]
    rect(B, (x0, y0, z0), (x1, y0, z0), (x1, y0, z0 + h), (x0, y0, z0 + h), mat, (0, -1, 0), var)
    rect(B, (x0, y1, z1), (x1, y1, z1), (x1, y1, z1 + h), (x0, y1, z1 + h), mat, (0, 1, 0), var)


def slope_rib(B, xc, w, a, b, h, mat, var=0.5):
    """Rib (batten, glazing bar, lead roll) w wide and h tall lying on a roof slope from a = (y, z) to b = (y, z)
    at x = xc (house coordinates): its top, both sides and both ends."""
    a, b = np.array([0.0, *a]), np.array([0.0, *b])
    d = b - a
    n = np.array([0.0, -d[2], d[1]]) / np.linalg.norm(d)
    if n[2] < 0:
        n = -n
    lo = {"a0": a + (xc - w / 2, 0, 0), "a1": a + (xc + w / 2, 0, 0), "b0": b + (xc - w / 2, 0, 0), "b1": b + (xc + w / 2, 0, 0)}
    hi = {k: v + n * h for k, v in lo.items()}
    rect(B, hi["a0"], hi["a1"], hi["b1"], hi["b0"], mat, tuple(n), var)
    rect(B, lo["a0"], lo["b0"], hi["b0"], hi["a0"], mat, (-1, 0, 0), var)
    rect(B, lo["a1"], lo["b1"], hi["b1"], hi["a1"], mat, (1, 0, 0), var)
    rect(B, lo["a0"], lo["a1"], hi["a1"], hi["a0"], mat, tuple(-d), var)
    rect(B, lo["b0"], lo["b1"], hi["b1"], hi["b0"], mat, tuple(d), var)


def lattice_bars(B, x0, x1, z0, z1, pitch, width, y_back, y_front, mat, var=0.5):
    """Diamond lattice (two sets of diagonal bars) clipped to the rectangle x0..x1, z0..z1 (wall coordinates)."""
    for sgn in (1.0, -1.0):
        lo, hi = sorted((z0 - sgn * x0, z0 - sgn * x1))[0] - pitch, sorted((z1 - sgn * x0, z1 - sgn * x1))[1] + pitch
        for c in np.arange(lo, hi, pitch):            # the line z = sgn * x + c
            pts = []
            for x in (x0, x1):
                z = sgn * x + c
                if z0 - 1e-9 <= z <= z1 + 1e-9:
                    pts.append((x, z))
            for z in (z0, z1):
                x = (z - c) / sgn
                if x0 - 1e-9 <= x <= x1 + 1e-9:
                    pts.append((x, z))
            pts = sorted(set((round(p[0], 6), round(p[1], 6)) for p in pts))
            if len(pts) >= 2 and math.dist(pts[0], pts[-1]) > 0.02:
                gk.bar(B, mat, pts[0], pts[-1], width, y_back, y_front, var=var)
