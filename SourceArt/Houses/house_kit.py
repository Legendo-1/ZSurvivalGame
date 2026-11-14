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


def panel_leaf(B, w, h, mat, t=0.06, cols=2, rows=3, frame=0.13, sunk=0.014, var=0.5, weights=None):
    """Panelled door leaf in its own frame: x 0..w (hinge at x = 0), y 0..t (face 1 at y = 0), z 0..h. Panels on both
    faces are sunk by `sunk`; the top row is shorter, as on a six-panel door. weights: the rows' relative heights
    from the bottom up, overriding that (added by house #5)."""
    if weights is None:
        weights = [1.0] * rows
        weights[0] = 0.6 if rows > 2 else 1.0
        weights = weights[::-1]                  # weights from the bottom up: the shortest row is at the top
    else:
        weights = list(weights)
        rows = len(weights)
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
def clock_dial(B, cx, cz, half, r, plate, ring_mat, dial_mat, marks, brass=None, hour=10.0, minute=9.0, plate_proud=0.04, rosette=None, centre=None):
    """Square plate (half size `half`, standing plate_proud in front of the wall) with brass corner bosses, a brass
    ring round a dial of radius r, a darker brass centre, Roman numerals and minute ticks in `marks`, brass hands.
    rosette: material of the corner bosses; centre: material of the centre disc and its ring (both default: brass;
    added by house #3).
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
            gk.rosette(B, rosette or brass, 0.12, 0.04, petals=6, layers=2)
            B.pop()
    ang = np.radians(np.linspace(0.0, 360.0, 97)[:-1])
    B.fan([(cx + r * math.cos(a), -0.012, cz + r * math.sin(a)) for a in ang], dial_mat, (0, -1, 0), var=0.5)
    B.fan([(cx + r * 0.52 * math.cos(a), -0.016, cz + r * 0.52 * math.sin(a)) for a in ang], centre or brass, (0, -1, 0), var=0.3)
    gk.ring(B, ring_mat, cx, cz, r, [[(0.14, 0.0), (0.14, 0.055), (0.11, 0.055)], gk.half_round(0.11, 0.03, 0.055),
                                     [(0.03, 0.055), (0.0, 0.055), (0.0, 0.012)]], segs=96)
    gk.ring(B, centre or ring_mat, cx, cz, r * 0.52, [[(0.025, 0.016), (0.025, 0.03), (-0.01, 0.03), (-0.01, 0.016)]], segs=72)
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


# ---------------------------------------------------------------------------------------------
# Added by house #3 (SilversmithsOrnateAtelier): cast-iron balcony lace, roof railings, cast columns and newels,
# spandrel arches, and a square clock cupola (blank dials, tiled pyramid, arched dormer, iron finial)
# ---------------------------------------------------------------------------------------------
MIRROR_Y = np.diag([1.0, -1.0, 1.0, 1.0])


def both_faces(B, y, fn):
    """Runs fn() (flat iron drawn in front of the plane y, facing -y, e.g. gk.bar pieces) twice: as is, and mirrored
    about the plane y, so pierced iron has a back as well as a front."""
    fn()
    B.push(translate(0.0, 2.0 * y, 0.0) @ MIRROR_Y)
    fn()
    B.pop()


def lace_panel(B, x0, x1, z0, z1, y, mat, pitch=0.10, bar_w=0.02, th=0.012, frame=0.035, var=0.5):
    """Cast-iron balcony panel in the plane y (wall coordinates): bottom and top rails, end stiles, a mid rail and a
    dense diamond lattice between them, iron on both faces (2 * th thick)."""
    for a, b, c, d in ((x0, x1, z0, z0 + frame), (x0, x1, z1 - frame, z1), (x0, x0 + frame, z0, z1), (x1 - frame, x1, z0, z1)):
        block(B, a, b, y - th - 0.004, y + th + 0.004, c, d, mat, var=var)
    zm = (z0 + z1) / 2
    block(B, x0 + frame, x1 - frame, y - th, y + th, zm - 0.012, zm + 0.012, mat, faces="yYzZ", var=var)
    both_faces(B, y, lambda: lattice_bars(B, x0 + frame, x1 - frame, z0 + frame, z1 - frame, pitch, bar_w, y, y - th, mat, var=var))


def iron_railing(B, x0, x1, y, z0, h, mat, pitch=0.39, spear=0.12, ends=False, var=0.5):
    """Roof railing along wall x in the plane y: bottom, top and two mid rails, square bars every ~pitch rising
    through the top rail to spear heads, and an oval ring between every two bars. ends: bars at x0 and x1 too."""
    for za, zb in ((z0 + 0.06, z0 + 0.11), (z0 + h - 0.07, z0 + h - 0.02), (z0 + 0.20, z0 + 0.23), (z0 + h - 0.28, z0 + h - 0.25)):
        block(B, x0, x1, y - 0.025, y + 0.025, za, zb, mat, faces="yYzZ", var=var)
    n = max(1, int(round((x1 - x0) / pitch)))
    xs = [x0 + (x1 - x0) * k / n for k in range(n + 1)]
    for k, x in enumerate(xs):
        if not ends and k in (0, n):
            continue
        block(B, x - 0.013, x + 0.013, y - 0.013, y + 0.013, z0 + 0.06, z0 + h, mat, faces="xXyY", var=var)
        B.lathe([[(0.032, 0.0), (0.032, 0.025), (0.001, spear)]], mat, segs=4, a0=45.0, a1=405.0, center=(x, y, z0 + h), smooth=False, var=var)
    zc, rz = z0 + (0.23 + h - 0.28) / 2, (h - 0.28 - 0.23) / 2 - 0.012
    a = np.radians(np.linspace(0.0, 360.0, 17)[:-1])
    for xa, xb in zip(xs, xs[1:]):
        rx = (xb - xa) / 2 - 0.025
        B.tube([((xa + xb) / 2 + rx * math.cos(q), y, zc + rz * math.sin(q)) for q in a], 0.012, mat, sides=6, closed=True, var=var)


def cast_column(B, cx, cy, z0, z1, mat, r=0.055, var=0.5):
    """Slender cast-iron column (house or wall coordinates) from z0 to z1: moulded base, round shaft with a collar
    near the top, and a flared capital under a round abacus."""
    H = z1 - z0
    prof = [(0.001, 0.0), (0.10, 0.0), (0.10, 0.05), (0.075, 0.09), (0.072, 0.20), (0.085, 0.24), (0.062, 0.28), (r, 0.34),
            (r, H - 0.42), (0.072, H - 0.39), (0.072, H - 0.34), (r * 0.95, H - 0.32), (0.10, H - 0.08), (0.12, H - 0.06),
            (0.12, H), (0.001, H)]
    B.lathe([prof], mat, segs=12, center=(cx, cy, z0), smooth=True, var=var)


def newel(B, cx, cy, z0, z1, mat, w=0.10, ball_r=0.065, var=0.5):
    """Square iron newel post from z0 with a cap and a ball finial; the ball's top is at z1."""
    zc = z1 - 2 * ball_r
    block(B, cx - w / 2, cx + w / 2, cy - w / 2, cy + w / 2, z0, zc - 0.04, mat, faces="xXyYz", var=var)
    block(B, cx - w / 2 - 0.02, cx + w / 2 + 0.02, cy - w / 2 - 0.02, cy + w / 2 + 0.02, zc - 0.04, zc, mat, var=var)
    B.push(translate(cx, cy, zc))
    gk.ball(B, mat, ball_r, var=var)
    B.pop()


def spandrel(B, xa, xb, z_spring, z_crown, z_top, y, mat, pitch=0.09, var=0.5):
    """Cast-iron arched spandrel in the plane y (wall coordinates) between columns at xa and xb: a segmental arch
    rod from the springing up to the crown, a beam under z_top, and the space between filled with uprights and rings."""
    half, rise = (xb - xa) / 2, z_crown - z_spring
    R = (half * half + rise * rise) / (2 * rise)
    xm, zc0 = (xa + xb) / 2, z_crown - R
    a0 = math.atan2(z_spring - zc0, xa - xm)
    a1 = math.atan2(z_spring - zc0, xb - xm)
    B.tube([(xm + R * math.cos(q), y, zc0 + R * math.sin(q)) for q in np.linspace(a0, a1, 25)], 0.03, mat, sides=6, var=var, flat=0.6)
    block(B, xa - 0.04, xb + 0.04, y - 0.03, y + 0.03, z_top - 0.07, z_top, mat, var=var)

    def arch_z(x):
        return zc0 + math.sqrt(max(R * R - (x - xm) ** 2, 0.0))
    n = max(1, int((xb - xa) / pitch))
    for k in range(1, n):
        x = xa + (xb - xa) * k / n
        za = arch_z(x)
        if z_top - 0.07 - za > 0.03:
            block(B, x - 0.008, x + 0.008, y - 0.008, y + 0.008, za, z_top - 0.07, mat, faces="xXyY", var=var)
    a = np.radians(np.linspace(0.0, 360.0, 13)[:-1])
    x = xa + 0.16
    while x < xb - 0.16:
        free = z_top - 0.07 - arch_z(x)
        if free > 0.16:
            r = min(free / 2 - 0.02, 0.13)
            zc = arch_z(x) + free / 2
            B.tube([(x + r * math.cos(q), y, zc + r * math.sin(q)) for q in a], 0.011, mat, sides=5, closed=True, var=var)
            x += 2 * r + 0.06
        else:
            x += 0.08


def blank_dial(B, cx, cz, half, r, plate, ring_mat, disc_mat, plate_proud=0.04, var=0.5):
    """The clock_dial plate and ring round a plain disc, with no numerals or hands (the cupola's other faces)."""
    block(B, cx - half, cx + half, -plate_proud, 0.0, cz - half, cz + half, plate, faces="xXyzZ", var=0.45)
    B.push(translate(0.0, -plate_proud, 0.0))
    ang = np.radians(np.linspace(0.0, 360.0, 97)[:-1])
    B.fan([(cx + r * math.cos(q), -0.012, cz + r * math.sin(q)) for q in ang], disc_mat, (0, -1, 0), var=var)
    gk.ring(B, ring_mat, cx, cz, r, [[(0.14, 0.0), (0.14, 0.055), (0.11, 0.055)], gk.half_round(0.11, 0.03, 0.055),
                                     [(0.03, 0.055), (0.0, 0.055), (0.0, 0.012)]], segs=96)
    B.pop()


def pyramid_roof(B, cx, cy, half, z0, apex, mat, pitch=0.2, step=0.02, seed=5):
    """Square pyramid roof laid in courses of tiles (gothic_kit.tile_facet), eaves `half` from (cx, cy) at z0."""
    c = [(cx - half, cy - half, z0), (cx + half, cy - half, z0), (cx + half, cy + half, z0), (cx - half, cy + half, z0)]
    top = (cx, cy, apex)
    for k in range(4):
        gk.tile_facet(B, mat, c[k], c[(k + 1) % 4], top, top, pitch=pitch, step=step, seed=seed + k)


def arched_block(B, cx, w, z0, z_spring, y0, y1, mat, n=12, var=0.5):
    """Block w wide from y0 (its front, facing -y) back to y1 with a round top: upright sides up to z_spring and a
    half circle over them (wall coordinates). A dormer front or a gablet."""
    r = w / 2
    arc = [(cx + r * math.cos(q), z_spring + r * math.sin(q)) for q in np.radians(np.linspace(0.0, 180.0, n + 1))]
    out = [(cx - r, z0), (cx + r, z0)] + arc + [(cx - r, z0)]
    pts = out[:-1]
    B.fan([(x, y0, z) for x, z in pts], mat, (0, -1, 0), var=var)
    B.fan([(x, y1, z) for x, z in pts], mat, (0, 1, 0), var=var)
    mid = np.array([cx, (z0 + z_spring + r) / 2])
    for (xa, za), (xb, zb) in zip(out[1:], out[2:]):
        e = (np.array([xa + xb, za + zb]) / 2) - mid
        rect(B, (xa, y0, za), (xb, y0, zb), (xb, y1, zb), (xa, y1, za), mat, (e[0], 0.0, e[1]), var)


def iron_finial(B, cx, cy, z0, top, mat, var=0.5):
    """Wrought-iron spire finial from z0 up to top: turned bulbs and collars on a tapering rod, with two tiers of four
    curling leaves (a fleur-de-lis cresting)."""
    s = (top - z0) / 3.06
    prof = [(0.13, 0.0), (0.13, 0.1), (0.06, 0.2), (0.05, 0.6), (0.1, 0.7), (0.1, 0.8), (0.04, 0.9), (0.035, 1.6), (0.09, 1.7),
            (0.07, 1.85), (0.03, 1.9), (0.025, 2.6), (0.06, 2.65), (0.001, 3.06)]
    B.lathe([[(r, z * s) for r, z in prof]], mat, segs=10, center=(cx, cy, z0), smooth=True, var=var)
    for zl, k in ((0.95, 1.0), (1.95, 0.7)):
        for q in range(4):
            d = np.array([math.cos(math.pi / 2 * q), math.sin(math.pi / 2 * q), 0.0])
            base = np.array([cx, cy, z0 + zl * s])
            pts = [base + d * 0.04, base + d * 0.20 * k + (0, 0, 0.15 * k), base + d * 0.30 * k + (0, 0, 0.38 * k),
                   base + d * 0.24 * k + (0, 0, 0.58 * k), base + d * 0.13 * k + (0, 0, 0.55 * k), base + d * 0.12 * k + (0, 0, 0.45 * k)]
            B.tube(pts, [0.03 * k, 0.03 * k, 0.028 * k, 0.024 * k, 0.02 * k, 0.015 * k], mat, sides=6, var=var)


# ---------------------------------------------------------------------------------------------
# House #4 (mansard tenement): faces under a sloping outline, verge bands, a glazed door leaf, quoins
# ---------------------------------------------------------------------------------------------
def outline_at(outline, x):
    """z of a top outline [(x, z), ...] (x ascending) at x, linear between its points."""
    xs, zs = zip(*outline)
    return float(np.interp(x, xs, zs))


def outline_face(B, outline, z0, mat, holes=(), y=0.0, inward=False, var=0.5, uv_off=UV0):
    """Upright face on the plane y (wall coordinates) from z0 up to a top outline [(x, z), ...] (x ascending), e.g. a
    gable under a roof, with rectangular holes. Cut into upright strips at every outline point and hole edge, so
    every piece is a quad with a flat foot and a straight top."""
    xs = sorted({p[0] for p in outline} | {h[0] for h in holes} | {h[1] for h in holes})
    xs = [x for x in xs if outline[0][0] - 1e-9 <= x <= outline[-1][0] + 1e-9]
    want = (0, 1, 0) if inward else (0, -1, 0)
    for xa, xb in zip(xs, xs[1:]):
        if xb - xa < 1e-6:
            continue
        ta, tb = outline_at(outline, xa), outline_at(outline, xb)
        # every hole over this strip, bottom to top (house #8: a strip under several stacked holes used to keep only
        # the last one; with one hole or none the spans are the same as before)
        cut = sorted((hz0, hz1) for hx0, hx1, hz0, hz1 in (h[:4] for h in holes) if hx0 <= xa + 1e-9 and xb <= hx1 + 1e-9)
        spans, zc = [], z0
        for hz0, hz1 in cut:
            spans.append((zc, hz0))
            zc = max(zc, hz1)
        spans.append((zc, None))
        for za, zb in spans:
            if zb is None:
                if min(ta, tb) - za > 1e-6:
                    rect(B, (xa, y, za), (xb, y, za), (xb, y, tb), (xa, y, ta), mat, want, var=var, uv_off=uv_off)
            elif zb - za > 1e-6:
                rect(B, (xa, y, za), (xb, y, za), (xb, y, zb), (xa, y, zb), mat, want, var=var, uv_off=uv_off)


def verge_band(B, outline, h, proud, mat, var=0.5):
    """Flat band `h` deep and `proud` out of the wall plane y = 0 (wall coordinates), hanging under a top outline
    [(x, z), ...]: its face, its sloping underside and its two ends (a gable's verge moulding under the roof)."""
    for (xa, za), (xb, zb) in zip(outline, outline[1:]):
        rect(B, (xa, -proud, za - h), (xb, -proud, zb - h), (xb, -proud, zb), (xa, -proud, za), mat, (0, -1, 0), var)
        rect(B, (xa, 0.0, za - h), (xb, 0.0, zb - h), (xb, -proud, zb - h), (xa, -proud, za - h), mat, (-(zb - za), -0.2, xb - xa), var)
    for (x, z), nx in ((outline[0], -1.0), (outline[-1], 1.0)):
        rect(B, (x, 0.0, z - h), (x, -proud, z - h), (x, -proud, z), (x, 0.0, z), mat, (nx, 0, 0), var)


def glazed_leaf(B, w, h, mat, glass, bar_mat, t=0.06, frame=0.12, light=(1.2, 2.1), grid=(6, 7), panel_z=(0.16, 0.98), sunk=0.014,
                var=0.5):
    """Shop door leaf: x 0..w (hinge at x = 0), y 0..t (outside face at y = 0), z 0..h. A glazed light (z light[0]..light[1],
    two-sided glass in the middle of the leaf with a grid of thin bars on both faces) over two sunk panels side by side
    (z panel_z) on both faces."""
    pw = (w - 3 * frame) / 2
    panels = [(frame, frame + pw, *panel_z), (2 * frame + pw, w - frame, *panel_z)]
    lt = (frame, w - frame, *light)
    for y, inward in ((0.0, False), (t, True)):
        flat_face(B, 0.0, w, 0.0, h, mat, holes=panels + [lt], y=y, inward=inward, var=var)
        yp = y + sunk if not inward else y - sunk
        for hx0, hx1, hz0, hz1 in panels:
            flat_face(B, hx0, hx1, hz0, hz1, mat, y=yp, inward=inward, var=var)
            reveal(B, (hx0, hx1, hz0, hz1), min(y, yp), max(y, yp), mat, var=var)
    reveal(B, lt, 0.0, t, mat, var=var)
    block(B, 0.0, w, 0.0, t, 0.0, h, mat, faces="xXzZ", var=var)
    yc = t / 2
    gx0, gx1, gz0, gz1 = lt
    for yy, want in ((yc - 0.001, (0, -1, 0)), (yc + 0.001, (0, 1, 0))):
        rect(B, (gx0, yy, gz0), (gx1, yy, gz0), (gx1, yy, gz1), (gx0, yy, gz1), glass, want, var=0.5)
    cols, rows = grid
    for i in range(1, cols):
        x = gx0 + (gx1 - gx0) * i / cols
        block(B, x - 0.008, x + 0.008, yc - 0.012, yc + 0.012, gz0, gz1, bar_mat, faces="xXyY", var=var)
    for j in range(1, rows):
        z = gz0 + (gz1 - gz0) * j / rows
        block(B, gx0, gx1, yc - 0.012, yc + 0.012, z - 0.008, z + 0.008, bar_mat, faces="yYzZ", var=var)


def quoin_corner(B, z0, z1, course, long, short, proud, mat, start_long=True, var=0.5):
    """Quoins up the corner at the origin of a facade frame (wall x = 0, wall plane y = 0, the neighbouring side's face
    on x = 0 going into +y): courses alternating long and short on this face, the other way round on the corner face."""
    n = max(1, round((z1 - z0) / course))
    hc = (z1 - z0) / n
    for i in range(n):
        za, zb = z0 + i * hc + 0.006, z0 + (i + 1) * hc - 0.006
        a, b = (long, short) if (i % 2 == 0) == start_long else (short, long)
        block(B, -proud, a, -proud, 0.0, za, zb, mat, faces="xXyzZ", var=var)
        block(B, -proud, 0.0, 0.0, b, za, zb, mat, faces="xYzZ", var=var)


# ---------------------------------------------------------------------------------------------
# House #5 (BookbindersMansardAtelier): round arches, a sunburst fanlight, basement vent grilles, standing-seam metal
# ---------------------------------------------------------------------------------------------
def arc_pts(xc, zc, r, a0=0.0, a1=180.0, n=24):
    """Points [(x, z), ...] on a circle arc about (xc, zc) from angle a0 to a1 (degrees, 0 = +x, 90 = up)."""
    return [(xc + r * math.cos(q), zc + r * math.sin(q)) for q in np.radians(np.linspace(a0, a1, n + 1))]


def arch_intrados(B, xc, zs, r, y0, y1, mat, n=24, var=0.5):
    """Soffit of a round arch (wall coordinates): the half cylinder of radius r about (xc, zs) from the plane y0 back to
    y1, facing the arch's axis."""
    pts = arc_pts(xc, zs, r, n=n)
    for (xa, za), (xb, zb) in zip(pts, pts[1:]):
        mx, mz = (xa + xb) / 2 - xc, (za + zb) / 2 - zs
        rect(B, (xa, y0, za), (xb, y0, zb), (xb, y1, zb), (xa, y1, za), mat, (-mx, 0.0, -mz), var)


def arch_ring(B, xc, zs, r0, r1, y, mat, n=24, var=0.5):
    """Half-round arch ring (an archivolt) standing on the wall plane y = 0 (wall coordinates): its front face on the
    plane y (y < 0, facing -y) between radii r0 and r1, and its outer face (extrados) from y back to the wall."""
    a, b = arc_pts(xc, zs, r0, n=n), arc_pts(xc, zs, r1, n=n)
    for k in range(n):
        rect(B, (a[k][0], y, a[k][1]), (b[k][0], y, b[k][1]), (b[k + 1][0], y, b[k + 1][1]), (a[k + 1][0], y, a[k + 1][1]), mat, (0, -1, 0), var)
        mx, mz = (b[k][0] + b[k + 1][0]) / 2 - xc, (b[k][1] + b[k + 1][1]) / 2 - zs
        rect(B, (b[k][0], y, b[k][1]), (b[k + 1][0], y, b[k + 1][1]), (b[k + 1][0], 0.0, b[k + 1][1]), (b[k][0], 0.0, b[k][1]), mat, (mx, 0.0, mz), var)


def arch_spandrels(B, xc, zs, r, x0, x1, z_top, y, mat, n=24, var=0.5):
    """Flat faces on the plane y (facing -y) filling the rectangle x0..x1, zs..z_top outside the half circle of radius
    r about (xc, zs) (x0 <= xc - r, x1 >= xc + r): one fan per side from its top outer corner. n must be even."""
    q = arc_pts(xc, zs, r, n=n)
    h = n // 2
    for hub, poly in (((x1, z_top), [(x1, zs)] + q[:h + 1] + [(xc, z_top)]),
                      ((x0, z_top), [(xc, z_top)] + q[h:] + [(x0, zs)])):
        pts = [poly[0]]
        for p in poly[1:]:
            if math.dist(p, pts[-1]) > 1e-5:
                pts.append(p)
        B.fan([(x, y, z) for x, z in pts], mat, (0, -1, 0), var=var, hub=(hub[0], y, hub[1]))


def fanlight(B, xc, zs, r, y, glass, iron, spokes=10, var=0.5):
    """Half-round fanlight in the plane y (wall coordinates, seen from -y): two-sided glass, and in front of it an iron
    sunburst: a half-round hub, radial bars, a half ring through them and a small ring between every two bars."""
    pts = arc_pts(xc, zs, r, n=32)
    for yy, want in ((y - 0.001, (0, -1, 0)), (y + 0.001, (0, 1, 0))):
        B.fan([(x, yy, z) for x, z in pts], glass, want, var=var)
    yb, yf = y - 0.003, y - 0.022
    hub = arc_pts(xc, zs, r * 0.17, n=12)
    B.fan([(x, yf, z) for x, z in hub], iron, (0, -1, 0), var=var)
    for k in range(1, spokes):
        a = math.radians(180.0 * k / spokes)
        e = np.array([math.cos(a), math.sin(a)])
        gk.bar(B, iron, np.array([xc, zs]) + e * r * 0.15, np.array([xc, zs]) + e * r, 0.022, yb, yf, var=var)
    ring = arc_pts(xc, zs, r * 0.55, n=24)
    B.tube([(x, (yb + yf) / 2, z) for x, z in ring], 0.011, iron, sides=6, var=var)
    a = np.radians(np.linspace(0.0, 360.0, 13)[:-1])
    rr = r * 0.11
    for k in range(spokes):
        q = math.radians(180.0 * (k + 0.5) / spokes)
        cx, cz = xc + r * 0.78 * math.cos(q), zs + r * 0.78 * math.sin(q)
        B.tube([(cx + rr * math.cos(t), (yb + yf) / 2, cz + rr * math.sin(t)) for t in a], 0.009, iron, sides=5, closed=True, var=var)
    gk.bar(B, iron, (xc - r, zs + 0.011), (xc + r, zs + 0.011), 0.022, yb, yf, var=var)


def vent_grille(B, x0, x1, z0, z1, y, depth, mat, iron, back, pitch=0.075, var=0.5):
    """Basement vent in a face on the plane y (wall coordinates): reveals in `mat` back to y + depth, a dark plate
    there in `back`, and an iron grille of upright bars in a flat frame 2 cm behind the face."""
    reveal(B, (x0, x1, z0, z1), y, y + depth, mat, var=var)
    rect(B, (x0, y + depth, z0), (x1, y + depth, z0), (x1, y + depth, z1), (x0, y + depth, z1), back, (0, -1, 0), var)
    yg = y + 0.02
    f = 0.035
    for a, b, c, d in ((x0, x1, z0, z0 + f), (x0, x1, z1 - f, z1), (x0, x0 + f, z0 + f, z1 - f), (x1 - f, x1, z0 + f, z1 - f)):
        block(B, a, b, yg, yg + 0.025, c, d, iron, faces="yzZxX", var=var)
    n = max(1, int(round((x1 - x0 - 2 * f) / pitch)))
    for k in range(1, n):
        x = x0 + f + (x1 - x0 - 2 * f) * k / n
        block(B, x - 0.011, x + 0.011, yg, yg + 0.02, z0 + f, z1 - f, iron, faces="xXy", var=var)
    zm = (z0 + z1) / 2
    block(B, x0 + f, x1 - f, yg + 0.005, yg + 0.02, zm - 0.012, zm + 0.012, iron, faces="yzZ", var=var)


def seam_facet(B, mat, a, b, c, d, pitch=0.78, w=0.04, h=0.035, ribs=True, var=0.5):
    """Standing-seam metal sheet on the quad a-b-c-d (house coordinates: a-b the lower edge along x, d-c the upper
    edge, a and d at the low x), facing up, with seam ribs (slope_rib) running up the slope about every `pitch`."""
    a, b, c, d = (np.asarray(p, dtype=np.float64) for p in (a, b, c, d))
    n = np.cross(b - a, d - a)
    if n[2] < 0:
        n = -n
    rect(B, tuple(a), tuple(b), tuple(c), tuple(d), mat, tuple(n), var)
    if not ribs:
        return
    k = max(1, int(round((b[0] - a[0]) / pitch)))
    for i in range(1, k):
        xc = a[0] + (b[0] - a[0]) * i / k
        slope_rib(B, xc, w, (a[1], a[2]), (d[1], d[2]), h, mat, var)


# ---------------------------------------------------------------------------------------------
# House #6 (GrandApothecaryHerbalistShop): half-timbering, open plank shutters, a barred window, a hanging shop sign
# ---------------------------------------------------------------------------------------------
def timber_members(B, wl, z0, top, var=0.5):
    """Timber framing proud of a wall's infill plane y = 0 (wall coordinates), from the wall spec wl:
      "plates": [[x0, x1, z0, z1, proud], ...]   sole plates, bressummers, top plates (the most proud)
      "posts":  [[x0, x1], ...]                  full height from z0 up to the top (a height, or a top outline
                                                 [(x, z), ...]: the post stops under its lowest point), 7.5 cm proud
      "rails":  [[x0, x1, z0, z1], ...]          rails and short studs, 6 cm proud
      "braces": [[xa, za, xb, zb], ...]          diagonal braces 16 cm wide, 6.5 cm proud
    Members stand at different depths so that crossing faces never lie on one plane."""
    def zt(x0, x1):
        return top if not isinstance(top, (list, tuple)) else min(outline_at(top, x0), outline_at(top, x1))
    for x0, x1, za, zb, p in wl.get("plates", []):
        block(B, x0, x1, -p, 0.0, za, zb, "Timber", faces="xXyzZ", var=var)
    for x0, x1 in wl.get("posts", []):
        block(B, x0, x1, -0.075, 0.0, z0, zt(x0, x1), "Timber", faces="xXyzZ", var=var)
    for x0, x1, za, zb in wl.get("rails", []):
        block(B, x0, x1, -0.06, 0.0, za, zb, "Timber", faces="xXyzZ", var=var)
    for xa, za, xb, zb in wl.get("braces", []):
        gk.bar(B, "Timber", (xa, za), (xb, zb), 0.16, 0.0, -0.065, var=var)


def plank_shutter(B, x0, x1, z0, z1, y, mat, iron, hinge="l", t=0.045, var=0.5):
    """Open plank shutter lying against a wall (wall coordinates): a board x0..x1, z0..z1 with its back on the plane y
    (the face towards -y), two ledgers across it near the top and the bottom, shallow plank joints, and an iron strap
    hinge on each ledger running from the hinge side ("l" = x0, "r" = x1, the side next to the window)."""
    block(B, x0, x1, y - t, y, z0, z1, mat, faces="xXyzZ", var=var)
    yf = y - t
    w, h = x1 - x0, z1 - z0
    for k in (1, 2):                                          # plank joints
        x = x0 + w * k / 3
        block(B, x - 0.006, x + 0.006, yf - 0.003, yf, z0 + 0.02, z1 - 0.02, mat, faces="xXy", var=var)
    for zc in (z0 + 0.11 * h, z1 - 0.11 * h):
        block(B, x0 + 0.03, x1 - 0.03, yf - 0.022, yf, zc - 0.06, zc + 0.06, mat, faces="xXyzZ", var=var)
        a, b = (x0 - 0.01, x0 + 0.62 * w) if hinge == "l" else (x1 - 0.62 * w, x1 + 0.01)
        block(B, a, b, yf - 0.03, yf - 0.022, zc - 0.022, zc + 0.022, iron, faces="xXyzZ", var=var)


def barred_window(B, x0, x1, z0, z1, depth, stone, frame, glass, iron, cols=4, rows=4, fw=0.09, var=0.5):
    """Small barred window in a wall face on the plane y = 0 (wall coordinates): a flat frame `fw` wide standing
    6 cm proud round the hole, reveals back to `depth`, two-sided dark glass there and a grid of square iron bars
    3 cm in front of it."""
    for a, b, c, d in ((x0 - fw, x1 + fw, z0 - fw, z0), (x0 - fw, x1 + fw, z1, z1 + fw), (x0 - fw, x0, z0, z1), (x1, x1 + fw, z0, z1)):
        block(B, a, b, -0.06, 0.0, c, d, frame, faces="xXyzZ", var=var)
    reveal(B, (x0, x1, z0, z1), 0.0, depth, stone, var=var)
    for yy, want in ((depth - 0.001, (0, -1, 0)), (depth + 0.001, (0, 1, 0))):
        rect(B, (x0, yy, z0), (x1, yy, z0), (x1, yy, z1), (x0, yy, z1), glass, want, var=var)
    yb = depth - 0.03
    for i in range(1, cols):
        x = x0 + (x1 - x0) * i / cols
        block(B, x - 0.014, x + 0.014, yb - 0.028, yb, z0, z1, iron, faces="xXy", var=var)
    for j in range(1, rows):
        z = z0 + (z1 - z0) * j / rows
        block(B, x0, x1, yb - 0.04, yb - 0.012, z - 0.014, z + 0.014, iron, faces="yzZ", var=var)


def hanging_sign(B, x, y_wall, z_arm, reach, board, mat, var=0.5):
    """Wrought-iron shop sign hanging over the street (house coordinates, the wall facing -y at y = y_wall): a wall
    plate, an arm `reach` long at z_arm with a ball at its end, a curved stay from the plate up the wall to the arm,
    and a board hung from the arm on two rods: board = (y_near, y_far, z_top, z_side, z_point), a shield shape whose
    sides run down to z_side and whose foot curves to a point at z_point, with a scroll curl at each lower corner."""
    block(B, x - 0.05, x + 0.05, y_wall - 0.03, y_wall, z_arm - 0.25, z_arm + 1.1, mat, faces="xXyzZ", var=var)
    ye = y_wall - reach
    B.tube([(x, y_wall - 0.03, z_arm), (x, ye, z_arm)], 0.035, mat, sides=8, var=var)
    B.tube([(x, ye - 0.06 + 0.06 * math.cos(a), z_arm + 0.06 * math.sin(a)) for a in np.linspace(0, 2 * math.pi, 9)], 0.04, mat, sides=6, closed=True, var=var)
    stay = [(x, y_wall - 0.03 - (reach * 0.55) * (1 - math.cos(a)), z_arm + 0.95 * math.sin(math.pi / 2 - a) ** 1.3)
            for a in np.linspace(0.0, math.pi / 2, 9)]
    B.tube(stay, 0.022, mat, sides=6, var=var)
    yn, yfar, zt, zs, zp = board
    for yy in (yn + 0.12, yfar - 0.12):
        B.tube([(x, yy, z_arm - 0.03), (x, yy, zt)], 0.012, mat, sides=6, var=var)
    ym = (yn + yfar) / 2
    hw = abs(yfar - yn) / 2
    pts = [(yfar, zt), (yn, zt)] + [(ym + hw * math.cos(a) * (1 if yn > yfar else -1), zs - (zs - zp) * math.sin(a)) for a in np.linspace(0.0, math.pi, 13)]
    th = 0.03
    for xx, nx in ((x - th, -1.0), (x + th, 1.0)):
        B.fan([(xx, yy, zz) for yy, zz in pts], mat, (nx, 0, 0), var=var)
    for (ya, za), (yb, zb) in zip(pts, pts[1:] + pts[:1]):
        rect(B, (x - th, ya, za), (x + th, ya, za), (x + th, yb, zb), (x - th, yb, zb), mat, ("from", (x, ym, (zt + zp) / 2)), var)
    for yy in (yn, yfar):
        cz = zs - 0.05
        B.tube([(x, yy + 0.07 * math.cos(a), cz + 0.07 * math.sin(a)) for a in np.linspace(0, 2 * math.pi, 11)], 0.012, mat, sides=5, closed=True, var=var)


# ---------------------------------------------------------------------------------------------
# House #7 (TextilePrintersDryingLoft): fixed louvres, a ledged plank door leaf, a gable hoist
# ---------------------------------------------------------------------------------------------
AXIS_X = np.array([[0, 0, 1, 0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1.0]])   # local z -> x, x -> y, y -> z: a lathe turned about x


def louvres(B, x0, x1, z0, z1, mat, back_mat=None, depth=0.09, pitch=0.25, slat=0.025, var=0.5):
    """Fixed louvre boards filling a hole x0..x1, z0..z1 in a wall face on the plane y = 0 (wall coordinates): reveals
    back to `depth`, a board there that closes the wall, and one sloping slat per `pitch`, from its front edge (1 cm
    in front of the face, at the bottom of the pitch) up to the back board at the top of the pitch."""
    back_mat = back_mat or mat
    reveal(B, (x0, x1, z0, z1), 0.0, depth, mat, var=var)
    rect(B, (x0, depth, z0), (x1, depth, z0), (x1, depth, z1), (x0, depth, z1), back_mat, (0, -1, 0), var)
    n = max(1, int(round((z1 - z0) / pitch)))
    p = (z1 - z0) / n
    yf, yb = -0.01, depth - 0.002
    for k in range(n):
        zf = z0 + k * p + slat                       # top of the front edge
        zb = min(z0 + (k + 1) * p, z1)               # where the slat's top meets the back board
        dz, dy = zb - zf, yb - yf
        rect(B, (x0, yf, zf), (x1, yf, zf), (x1, yb, zb), (x0, yb, zb), mat, (0.0, -dz, dy), var)
        rect(B, (x0, yf, zf - slat), (x1, yf, zf - slat), (x1, yb, zb - slat), (x0, yb, zb - slat), mat, (0.0, dz, -dy), var)
        rect(B, (x0, yf, zf - slat), (x1, yf, zf - slat), (x1, yf, zf), (x0, yf, zf), mat, (0, -1, 0), var)


def plank_leaf(B, w, h, mat, iron, t=0.06, planks=5, straps=(0.2, 0.8), strap_h=0.07, hinge="l", ring=0.45, var=0.5):
    """Ledged plank door leaf in leaf space (x 0..w, y 0..t, z 0..h, its face on y = 0 towards -y): the board,
    shallow joints between `planks` boards, an iron strap hinge right across it at each height in `straps` (fractions
    of h) with rivets, running from the hinge side ("l" = x 0, "r" = x w), and (ring > 0) a ring pull on a plate at
    that fraction of h near the latch side."""
    block(B, 0.0, w, 0.0, t, 0.0, h, mat, var=var)
    for k in range(1, planks):
        x = w * k / planks
        block(B, x - 0.006, x + 0.006, -0.003, 0.0, 0.01, h - 0.01, mat, faces="xXy", var=var)
    a, b = (0.005, w - 0.03) if hinge == "l" else (0.03, w - 0.005)
    for f in straps:
        zc = h * f
        block(B, a, b, -0.014, 0.0, zc - strap_h / 2, zc + strap_h / 2, iron, faces="xXyzZ", var=var)
        n = max(2, int((b - a) / 0.16))
        for k in range(n + 1):
            x = a + 0.03 + (b - a - 0.06) * k / n
            block(B, x - 0.011, x + 0.011, -0.022, -0.014, zc - 0.011, zc + 0.011, iron, faces="xXyzZ", var=var)
    if ring > 0:
        xr = w - 0.12 if hinge == "l" else 0.12
        zr = h * ring
        block(B, xr - 0.045, xr + 0.045, -0.012, 0.0, zr - 0.02, zr + 0.11, iron, faces="xXyzZ", var=var)
        B.tube([(xr + 0.065 * math.sin(q), -0.03, zr - 0.065 * math.cos(q)) for q in np.linspace(0, 2 * math.pi, 13)], 0.011, iron,
               sides=6, closed=True, var=var)


def hoist_crane(B, xc, y_wall, y_tip, z_beam, beam_h, foot, wheel_r, block_c, block_r, hook_z, iron, rope, var=0.5):
    """A gable hoist (house coordinates; the gable faces -y on the plane y = y_wall): a square iron beam (top at z_beam,
    beam_h deep, 12 cm wide) out of the gable towards -y, two cheek plates carrying a pulley wheel of radius wheel_r on
    an axle (along x) at y = y_tip; a diagonal strut from foot = (y, z) on the gable up under the beam's end; two ropes
    down from the wheel to a lower block (block_c = (y, z), radius block_r) with a strap and a hook ring under it
    (hook_z = the ring's centre); and a rope from the wheel's top back along the beam into the gable."""
    hw = 0.06
    zb = z_beam - beam_h
    zc = z_beam - beam_h / 2                                      # the axle
    ye = y_tip + wheel_r + 0.04                                   # the beam's end, just short of the wheel
    block(B, xc - hw, xc + hw, ye, y_wall, zb, z_beam, iron, faces="xXyzZ", var=var)
    for sx in (-1.0, 1.0):
        x = xc + sx * (hw + 0.013)
        block(B, x - 0.012, x + 0.012, y_tip - 0.07, ye + 0.20, zc - 0.07, zc + 0.07, iron, var=var)
    B.tube([(xc - 0.10, y_tip, zc), (xc + 0.10, y_tip, zc)], 0.03, iron, sides=8, var=var)

    def wheel(yc, zw, r, w):
        B.push(translate(xc, yc, zw) @ AXIS_X)
        B.lathe([[(0.02, -w), (r, -w)], [(r, -w), (r, w)], [(r, w), (0.02, w)]], iron, segs=28, smooth=False, var=var)
        B.lathe([[(r * 0.55, -w - 0.012), (r * 0.82, -w - 0.012)], [(r * 0.82, w + 0.012), (r * 0.55, w + 0.012)]], iron, segs=28,
                smooth=False, var=var)                            # a raised ring on each face
        B.pop()
    wheel(y_tip, zc, wheel_r, 0.045)
    by, bz = block_c
    wheel(by, bz, block_r, 0.04)
    B.tube([(xc - 0.07, by, bz), (xc + 0.07, by, bz)], 0.025, iron, sides=8, var=var)
    block(B, xc - 0.015, xc + 0.015, by - 0.03, by + 0.03, hook_z + 0.09, bz, iron, var=var)
    B.tube([(xc, by + 0.10 * math.sin(q), hook_z - 0.10 * math.cos(q)) for q in np.linspace(0, 2 * math.pi, 15)], 0.024, iron,
           sides=6, closed=True, var=var)
    for s in (-1.0, 1.0):
        B.tube([(xc, y_tip + s * wheel_r, zc), (xc, by + s * block_r, bz)], 0.016, rope, sides=6, var=var)
    B.tube([(xc, y_tip, zc + wheel_r), (xc, y_wall + 0.05, z_beam + 0.06)], 0.016, rope, sides=6, var=var)
    fy, fz = foot
    B.tube([(xc, fy, fz), (xc, ye + 0.30, zb + 0.03)], 0.055, iron, sides=6, var=var)
    block(B, xc - 0.09, xc + 0.09, fy - 0.025, fy, fz, fz + 0.16, iron, faces="xXyzZ", var=var)


# ---------------------------------------------------------------------------------------------
# House #8 (GlassblowersHighDraftFurnace): an octagonal brick chimney stack, iron bands, tie rods, anchor plates
# ---------------------------------------------------------------------------------------------
def oct_ring(cx, cy, r, z):
    """The 8 corners of a regular octagon with apothem r (centre to a flat) whose flats face +-x, +-y and the
    diagonals, anticlockwise from the lower end of the +x flat (-22.5 degrees). Flat k (corners k, k+1) faces 45k."""
    rc = r / math.cos(math.pi / 8)
    return [(cx + rc * math.cos(math.radians(-22.5 + 45 * k)), cy + rc * math.sin(math.radians(-22.5 + 45 * k)), z) for k in range(8)]


def rect_ring(cx, cy, hx, hy, c, z):
    """8 points on a rectangle (half sizes hx, hy, corners chamfered by c), matched one to one with oct_ring's corners:
    the octagon's +x flat goes to the rectangle's +x side, its diagonal flats to the chamfers."""
    a, b = hx - c, hy - c
    pts = [(hx, -b), (hx, b), (a, hy), (-a, hy), (-hx, b), (-hx, -b), (-a, -hy), (a, -hy)]
    return [(cx + px, cy + py, z) for px, py in pts]


def ring_skin(B, lo, hi, mat, var=0.5):
    """The 8 faces between two rings of 8 points (lo under hi), facing away from the axis through their centres."""
    c = np.mean(np.asarray(lo + hi, dtype=np.float64), axis=0)
    for k in range(8):
        j = (k + 1) % 8
        rect(B, lo[k], lo[j], hi[j], hi[k], mat, ("from", tuple(c)), var)


def octagon_profile(B, cx, cy, prof, mat, var=0.5):
    """An octagonal shaft turned from a profile [(z, apothem), ...] listed bottom to top: upright or sloping faces,
    and flat steps where z repeats (facing down where the profile steps out, up where it steps in)."""
    for (za, ra), (zb, rb) in zip(prof, prof[1:]):
        dz, dr = zb - za, rb - ra
        if abs(dz) < 1e-9 and abs(dr) < 1e-9:
            continue
        lo, hi = oct_ring(cx, cy, ra, za), oct_ring(cx, cy, rb, zb)
        for k in range(8):
            j = (k + 1) % 8
            th = math.radians(45 * k)
            rect(B, lo[k], lo[j], hi[j], hi[k], mat, (dz * math.cos(th), dz * math.sin(th), -dr), var)


def octagon_band(B, cx, cy, z0, z1, r0, r1, proud, mat, rivets=2, rivet=0.022, var=0.5):
    """An iron band round an octagonal shaft (apothem r0 at z0, r1 at z1), `proud` thick, with `rivets` round-headed
    rivets (square studs) on each flat."""
    octagon_profile(B, cx, cy, [(z0, r0), (z0, r0 + proud), (z1, r1 + proud), (z1, r1)], mat, var=var)
    zm, r = (z0 + z1) / 2, (r0 + r1) / 2 + proud
    half = r * math.tan(math.pi / 8)
    for k in range(8):
        B.push(translate(cx, cy, 0.0) @ rot_z(45.0 * k))
        for i in range(rivets):
            u = -half + 2 * half * (i + 0.5) / rivets
            block(B, r - 0.004, r + rivet * 0.7, u - rivet, u + rivet, zm - rivet, zm + rivet, mat, faces="xXyYzZ", var=var)
        B.pop()


def axis_frame(c, n):
    """A frame at point c whose local z points along the unit vector n (x and y any perpendicular pair)."""
    n = np.asarray(n, dtype=np.float64)
    n = n / np.linalg.norm(n)
    ref = np.array([0.0, 0.0, 1.0]) if abs(n[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    ex = np.cross(ref, n)
    ex /= np.linalg.norm(ex)
    ey = np.cross(n, ex)
    m = np.eye(4)
    m[:3, 0], m[:3, 1], m[:3, 2], m[:3, 3] = ex, ey, n, c
    return m


def anchor_plate(B, c, n, mat, r=0.27, t=0.035, boss_r=0.10, boss_h=0.07, nut=0.05, var=0.5):
    """A round iron tie-rod anchor plate on a wall: a disc of radius r, t thick, lying on the wall at point c with the
    wall's outward normal n, a domed boss in its middle and a square nut on the boss."""
    B.push(axis_frame(c, n))
    B.lathe([[(r, 0.0), (r, t)], [(r, t), (boss_r, t)], [(boss_r, t), (boss_r * 0.8, t + boss_h)], [(boss_r * 0.8, t + boss_h), (0.0, t + boss_h)]],
            mat, segs=20, smooth=False, var=var)
    block(B, -nut, nut, -nut, nut, t + boss_h - 0.005, t + boss_h + 0.035, mat, faces="xXyYZ", var=var)
    for k in range(6):                                        # bolt heads round the rim
        a = 2 * math.pi * k / 6
        x, y = 0.72 * r * math.cos(a), 0.72 * r * math.sin(a)
        block(B, x - 0.018, x + 0.018, y - 0.018, y + 0.018, t - 0.002, t + 0.02, mat, faces="xXyYZ", var=var)
    B.pop()


def tie_rod(B, pts, mat, r=0.04, knuckle=0.07, var=0.5):
    """An iron tie rod along a polyline of corner points (house coordinates), a straight round rod per leg and a
    square knuckle block at every inner corner."""
    for a, b in zip(pts, pts[1:]):
        B.tube([a, b], r, mat, sides=8, var=var)
    for p in pts[1:-1]:
        block(B, p[0] - knuckle, p[0] + knuckle, p[1] - knuckle, p[1] + knuckle, p[2] - knuckle * 2.2, p[2] + knuckle * 2.2, mat, var=var)
