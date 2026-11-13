"""Builds the rear of the Girona cathedral as sharp new stonework: the two-tier apse and the low annexes
in the corners between the apse and the side chapels.

Reference photos: "Cathedral images/Rear_left.jpg", "Rear_right.jpg", "Rear_side.jpg". Coordinates are those of
the Meshy model (front = -Y, bell tower = +X, metres), as in build_front.py. On the template the apse is a
twelve-sided body round the point C with a buttress on every corner; a narrower twelve-sided upper tier
rises out of it.

Parts:
  low     the lower tier: twelve faces with tall traceried windows, stepped buttresses with pinnacles,
          cornice, pierced parapet and the tiled lean-to roof behind it.
  high    the upper tier: twelve faces with traceried windows, buttress strips with pinnacles, cornice,
          pierced parapet, the twelve-sided tiled roof and its finial.
  annex   both corners: two low flat-roofed annexes each side and the east wall of the chapels above them.

Run: see build_common.py. Writes Girona_Apse_<part>.blend and SM_Cathedral_Apse_<part>.fbx.
"""
import math
import os
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gothic_kit as gk  # noqa: E402
import build_common  # noqa: E402

# ---------------------------------------------------------------------------------------------
# Measured on the template
# ---------------------------------------------------------------------------------------------
C = (0.0, 39.3)                         # axis of the apse
AP1, AP2 = 12.9, 10.2                   # apothems of the lower and the upper tier
Z1 = 18.0                               # top of the lower tier's walls
Z2A, Z2 = 19.0, 30.6                    # the upper tier's walls: from behind the lean-to roof to their top
APEX = 36.9
T12, C12 = math.tan(math.radians(15.0)), math.cos(math.radians(15.0))
PLINTH_H, PLINTH_OUT = 2.75, 0.22       # as on the rest of the building
STRING = [[(0.0, 0.0), (0.05, 0.03), (0.07, 0.07)], [(0.07, 0.07), (0.07, 0.13)], [(0.07, 0.13), (0.03, 0.16), (0.0, 0.16)]]
PLINTH_TOP = [[(PLINTH_OUT, 0.0), (PLINTH_OUT, 0.06)], [(PLINTH_OUT, 0.06), (PLINTH_OUT - 0.03, 0.10), (0.11, 0.2), (0.09, 0.26)],
              [(0.09, 0.26), (0.09, 0.30), (0.0, 0.34)]]
A_CH, CH_TOP, CH_END = 17.0, 10.85, 24.0        # the side chapels (build_side.py): wall face, top, where their last bay ends
EAST_Y = 26.4                                   # east wall of the chapels


def ngon(ap, z, shrink=0.0):
    """Corners of the twelve-sided plan with apothem ap - shrink at height z, anticlockwise from angle 0."""
    r = (ap - shrink) / C12
    return [(C[0] + r * math.cos(math.radians(30.0 * j)), C[1] + r * math.sin(math.radians(30.0 * j)), z) for j in range(12)]


def liner(B, ap, z0, z1, top=None):
    """A plain closed body just inside the stonework, so that nothing can ever be seen through it."""
    lo, hi = ngon(ap, z0), ngon(ap, z1)
    for j in range(12):
        k = (j + 1) % 12
        B.quad(lo[j], lo[k], hi[k], hi[j], "Ashlar", want=(lo[j][0] + lo[k][0] - 2 * C[0], lo[j][1] + lo[k][1] - 2 * C[1], 0.0), var=0.15)
    if top is None:
        B.fan(hi, "Ashlar", (0, 0, 1), var=0.15)
    else:
        for j in range(12):
            k = (j + 1) % 12
            B.fan([hi[j], hi[k], (C[0], C[1], top)], "Ashlar", (hi[j][0] + hi[k][0] - 2 * C[0], hi[j][1] + hi[k][1] - 2 * C[1], 1.0), var=0.15)


def buttress(B, ap, phi, w, stages, seed, plinth=True, pin=(0.36, 1.7, 3.0)):
    """Buttress on the corner at angle phi: stages (z0, z1, projection) stepping back with weathered
    set-offs, a base course, a flat top and a pinnacle. Local frame: x along the tangent, y inwards."""
    yf = (w / 2) * T12 + 0.06             # where the flanks disappear into the faces beside the corner
    hw = w / 2
    B.push(gk.face_matrix(C, ap / C12, phi))
    for i, (z0, z1, p) in enumerate(stages):
        gk.block_wall(B, -hw, hw, z0, z1, "Ashlar", y=-p, seed=seed + i)
        for (pa, pb), sd in ((((-hw, yf), (-hw, -p)), 10), (((hw, -p), (hw, yf)), 20)):
            fm, fl = gk.wall_frame(pa, pb)
            B.push(fm)
            gk.block_wall(B, 0.0, fl, z0, z1, "Ashlar", seed=seed + sd + i)
            B.pop()
        zc = PLINTH_H if (i == 0 and plinth) else z0
        gk.corner_post(B, "Ashlar", (-hw, -p), (0.0, 1.0), (-1.0, 0.0), (1.0, 0.0), (0.0, -1.0), zc, z1)
        gk.corner_post(B, "Ashlar", (hw, -p), (-1.0, 0.0), (0.0, -1.0), (0.0, 1.0), (1.0, 0.0), zc, z1)
        B.sweep([(-hw, yf, z1 - 0.16), (-hw, -p, z1 - 0.16), (hw, -p, z1 - 0.16), (hw, yf, z1 - 0.16)], STRING, "Limestone")     # drip mould
        if i + 1 < len(stages):                           # weathered set-off up to the next stage
            pn, rise = stages[i + 1][2], 0.55
            B.quad((-hw, -p, z1), (hw, -p, z1), (hw, -pn, z1 + rise), (-hw, -pn, z1 + rise), "Limestone", want=(0, -0.6, 0.8))
            for sx in (-1, 1):
                B.fan([(sx * hw, -p, z1), (sx * hw, -pn, z1), (sx * hw, -pn, z1 + rise)], "Limestone", (sx, 0, 0))
    z_top, p = stages[-1][1], stages[-1][2]
    B.quad((-hw, -p, z_top), (hw, -p, z_top), (hw, yf + 0.6, z_top), (-hw, yf + 0.6, z_top), "Limestone", want=(0, 0, 1))
    if plinth:
        po, p0 = PLINTH_OUT, stages[0][2]
        gk.block_wall(B, -hw - po, hw + po, -0.1, PLINTH_H, "Ashlar", y=-p0 - po, course=0.46, length=(0.8, 1.3), seed=seed + 30)
        for (pa, pb), sd in ((((-hw - po, yf + 0.3), (-hw - po, -p0 - po)), 31), (((hw + po, -p0 - po), (hw + po, yf + 0.3)), 32)):
            fm, fl = gk.wall_frame(pa, pb)
            B.push(fm)
            gk.block_wall(B, 0.0, fl, -0.1, PLINTH_H, "Ashlar", course=0.46, length=(0.8, 1.3), seed=seed + sd)
            B.pop()
        B.sweep([(-hw, yf + 0.3, PLINTH_H), (-hw, -p0, PLINTH_H), (hw, -p0, PLINTH_H), (hw, yf + 0.3, PLINTH_H)], PLINTH_TOP, "Limestone")
        gk.corner_post(B, "Ashlar", (-hw - po, -p0 - po), (0.0, 1.0), (-1.0, 0.0), (1.0, 0.0), (0.0, -1.0), -0.1, PLINTH_H, leg=0.2, proud=0.025)
        gk.corner_post(B, "Ashlar", (hw + po, -p0 - po), (-1.0, 0.0), (0.0, -1.0), (0.0, 1.0), (1.0, 0.0), -0.1, PLINTH_H, leg=0.2, proud=0.025)
    half, shaft, spire = pin
    B.push(gk.translate(0.0, min(-p + half + 0.04, 0.0), z_top))
    gk.pinnacle(B, "Limestone", half=half, shaft=shaft, spire=spire)
    B.pop()
    B.pop()


# ---------------------------------------------------------------------------------------------
# Part "low"
# ---------------------------------------------------------------------------------------------
BW1 = 1.5
STAGES1 = [(-0.1, 7.5, 1.2), (7.5, 13.0, 0.95), (13.0, Z1, 0.7)]
PLAIN = (255, 285)                      # the two faces that look at the nave across the roofs: no windows
NO_BUTTRESS = (270,)                    # this corner is where the nave joins


def build_low(B):
    hw = AP1 * T12
    fo = (BW1 / 2) / C12                 # a buttress flank meets the face this far from the corner
    po = PLINTH_OUT
    for k in range(12):
        th = 15 + 30 * k
        B.push(gk.face_matrix(C, AP1, th))
        x0, x1 = -hw + fo - 0.05, hw - fo + 0.05
        holes = []
        if th not in PLAIN:
            holes = gk.tracery_window(B, -1.15, 1.15, 4.6, 13.0, "Limestone", "Glass", "Iron", rise=2.0, lights=3, recess=0.55, frame_w=0.26,
                                      pane=(0.26, 0.4))
        gk.block_wall(B, x0, x1, -0.1, Z1, "Ashlar", holes=holes, seed=700 + k)
        gk.block_wall(B, x0, x1, -0.1, PLINTH_H, "Ashlar", y=-po, course=0.46, length=(0.8, 1.3), seed=720 + k)
        B.sweep([(x0, 0.0, PLINTH_H), (x1, 0.0, PLINTH_H)], PLINTH_TOP, "Limestone")
        gk.course(B, "Limestone", [(-hw, Z1 - 0.3), (hw, Z1 - 0.3)], s=1.0, caps=False)
        B.quad((-hw, -0.34, Z1), (hw, -0.34, Z1), (hw, 0.9, Z1), (-hw, 0.9, Z1), "Limestone", want=(0, 0, 1))           # wall top and gutter
        gk.pierced_parapet(B, "Limestone", (-hw, 0.1), (hw, 0.1), Z1, h=1.05)
        ye, yt = 0.45, AP1 - AP2 - 0.02                     # the lean-to roof: from behind the parapet up to the upper tier
        gk.tile_facet(B, "Tile", (-(AP1 - ye) * T12, ye, Z1 + 0.03), ((AP1 - ye) * T12, ye, Z1 + 0.03),
                      ((AP1 - yt) * T12, yt, 19.75), (-(AP1 - yt) * T12, yt, 19.75), seed=740 + k)
        B.pop()
    for j in range(12):
        if 30 * j not in NO_BUTTRESS:
            buttress(B, AP1, 30.0 * j, BW1, STAGES1, 760 + 40 * j)
    liner(B, AP1 - 0.9, -0.1, Z1 + 0.3)
    return {"corner radius": round(AP1 / C12, 3), "face width": round(2 * hw, 3), "top": Z1}


# ---------------------------------------------------------------------------------------------
# Part "high"
# ---------------------------------------------------------------------------------------------
BW2 = 0.9


def build_high(B):
    hw = AP2 * T12
    fo = (BW2 / 2) / C12
    ye = 0.45
    for k in range(12):
        th = 15 + 30 * k
        B.push(gk.face_matrix(C, AP2, th))
        holes = gk.tracery_window(B, -1.0, 1.0, 21.6, 26.6, "Limestone", "Glass", "Iron", rise=1.8, lights=3, recess=0.5, frame_w=0.24, pane=(0.25, 0.38))
        gk.block_wall(B, -hw + fo - 0.05, hw - fo + 0.05, Z2A, Z2, "Ashlar", holes=holes, seed=900 + k)
        gk.course(B, "Limestone", [(-hw, Z2 - 0.3), (hw, Z2 - 0.3)], s=1.0, caps=False)
        B.quad((-hw, -0.34, Z2), (hw, -0.34, Z2), (hw, 0.8, Z2), (-hw, 0.8, Z2), "Limestone", want=(0, 0, 1))
        gk.pierced_parapet(B, "Limestone", (-hw, 0.1), (hw, 0.1), Z2, h=1.0)
        gk.tile_facet(B, "Tile", (-(AP2 - ye) * T12, ye, Z2 + 0.05), ((AP2 - ye) * T12, ye, Z2 + 0.05), (0.0, AP2, APEX), (0.0, AP2, APEX),
                      pitch=0.42, seed=920 + k)
        B.pop()
    for j in range(12):
        buttress(B, AP2, 30.0 * j, BW2, [(Z2A, Z2, 0.6)], 940 + 40 * j, plinth=False, pin=(0.3, 1.3, 2.5))
    eave = ngon(AP2, Z2 + 0.09, shrink=ye)
    for p in eave:                                          # hip rolls
        B.tube([p, (C[0] + (p[0] - C[0]) * 0.5, C[1] + (p[1] - C[1]) * 0.5, (p[2] + APEX) / 2 + 0.02), (C[0], C[1], APEX + 0.04)], 0.1, "Tile", sides=6, var=0.35)
    B.push(gk.translate(C[0], C[1], APEX - 0.1))             # finial: stone knob, iron cross
    B.lathe([[(0.42, 0.0), (0.42, 0.18), (0.26, 0.3), (0.16, 0.62), (0.26, 0.74), (0.26, 0.82), (0.12, 0.9), (0.08, 1.05)]], "Limestone", segs=12)
    B.tube([(0.0, 0.0, 1.0), (0.0, 0.0, 2.9)], 0.03, "Iron", sides=8)
    B.tube([(-0.42, 0.0, 2.35), (0.42, 0.0, 2.35)], 0.025, "Iron", sides=6)
    B.push(gk.translate(0.0, 0.0, 1.55))
    gk.ball(B, "Iron", 0.11)
    B.pop()
    B.pop()
    liner(B, AP2 - 0.9, Z1 - 0.5, Z2 + 0.2, top=APEX - 0.6)
    return {"face width": round(2 * hw, 3), "eave": Z2, "apex": APEX}


# ---------------------------------------------------------------------------------------------
# Part "annex"
# ---------------------------------------------------------------------------------------------
AX_A = (18.0, CH_END, 27.9, 6.2)        # annex against the chapels: outer face, from y, to y, top
AX_B = (16.0, 27.9, 31.1, 4.6)          # the lower one beside the apse


def owall(sx, a, ya, yb):
    """Frame of a wall at distance a from the axis that faces away from it, between ya < yb."""
    return gk.wall_frame((a, ya), (a, yb)) if sx > 0 else gk.wall_frame((-a, yb), (-a, ya))


def cwall(sx, yp, a0, a1, facing):
    """Frame of a wall across the side at y = yp from a0 to a1; facing -1 looks at the front, +1 at the rear."""
    xs = sorted((sx * a0, sx * a1))
    return gk.wall_frame((xs[0], yp), (xs[1], yp)) if facing < 0 else gk.wall_frame((xs[1], yp), (xs[0], yp))


def small_window(B, cx, z0):
    return gk.tracery_window(B, cx - 0.36, cx + 0.36, z0, z0 + 1.5, "Limestone", "Glass", "Iron", lights=2, recess=0.35, frame_w=0.15, pane=(0.18, 0.3))


def flat_top(B, sx, a0, a1, y0, y1, z, seed):
    xs = sorted((sx * a0, sx * a1))
    gk.slab_floor(B, xs[0], xs[1], y0, y1, z, "Paving", size=(1.1, 0.8), seed=seed)


def build_annex(B):
    po = PLINTH_OUT
    for sx in (-1, 1):
        s = 500 if sx > 0 else 550
        n_out = (float(sx), 0.0)
        a_a, ya0, ya1, za = AX_A
        a_b, yb0, yb1, zb = AX_B
        # --- annex A: outer face with a small window, its short return at the chapel wall, its east face -----
        for (frame, z0, hole_at, sd, base) in ((owall(sx, a_a, ya0, ya1), -0.1, 0.5, 1, True), (cwall(sx, ya0, A_CH, a_a, -1), -0.1, None, 2, True),
                                               (cwall(sx, ya1, a_b, a_a, 1), -0.1, None, 3, True), (cwall(sx, ya1, 6.0, a_b, 1), zb, None, 4, False)):
            m, length = frame
            B.push(m)
            holes = small_window(B, length * hole_at, 2.9) if hole_at else []
            gk.block_wall(B, 0.0, length, z0, za, "Ashlar", holes=holes, seed=s + sd)
            gk.course(B, "Limestone", [(0.0, za - 0.12), (length, za - 0.12)], s=0.4, caps=False)
            if base:
                gk.block_wall(B, -po, length + po, -0.1, PLINTH_H, "Ashlar", y=-po, course=0.46, length=(0.8, 1.3), seed=s + 10 + sd)
                B.sweep([(0.0, 0.0, PLINTH_H), (length, 0.0, PLINTH_H)], PLINTH_TOP, "Limestone")
            B.pop()
        for yc, f in ((ya0, -1.0), (ya1, 1.0)):
            gk.corner_post(B, "Ashlar", (sx * a_a, yc), (-float(sx), 0.0), (0.0, f), (0.0, -f), n_out, PLINTH_H, za)
            gk.corner_post(B, "Ashlar", (sx * (a_a + po), yc + f * po), (-float(sx), 0.0), (0.0, f), (0.0, -f), n_out, -0.1, PLINTH_H, leg=0.2, proud=0.025)
        gk.seal_inside_corner(B, "Ashlar", (sx * A_CH, ya0), n_out, (0.0, -1.0), -0.1, za)
        flat_top(B, sx, 6.0, a_a, ya0, ya1, za - 0.02, s + 20)
        # --- annex B: outer face with a small window, its east face ------------------------------------------
        for (frame, hole_at, sd) in ((owall(sx, a_b, yb0, yb1), 0.5, 5), (cwall(sx, yb1, 11.4, a_b, 1), None, 6)):
            m, length = frame
            B.push(m)
            holes = small_window(B, length * hole_at, 2.2) if hole_at else []
            gk.block_wall(B, 0.0, length, -0.1, zb, "Ashlar", holes=holes, seed=s + sd)
            gk.course(B, "Limestone", [(0.0, zb - 0.12), (length, zb - 0.12)], s=0.4, caps=False)
            gk.block_wall(B, -po, length + po, -0.1, PLINTH_H, "Ashlar", y=-po, course=0.46, length=(0.8, 1.3), seed=s + 10 + sd)
            B.sweep([(0.0, 0.0, PLINTH_H), (length, 0.0, PLINTH_H)], PLINTH_TOP, "Limestone")
            B.pop()
        gk.corner_post(B, "Ashlar", (sx * a_b, yb1), (-float(sx), 0.0), (0.0, 1.0), (0.0, -1.0), n_out, PLINTH_H, zb)
        gk.corner_post(B, "Ashlar", (sx * (a_b + po), yb1 + po), (-float(sx), 0.0), (0.0, 1.0), (0.0, -1.0), n_out, -0.1, PLINTH_H, leg=0.2, proud=0.025)
        gk.seal_inside_corner(B, "Ashlar", (sx * a_b, ya1), n_out, (0.0, 1.0), -0.1, zb)
        flat_top(B, sx, 6.0, a_b, yb0, yb1, zb - 0.02, s + 21)
        # --- above the annexes: the chapel wall carried on to its corner, and the east wall of the chapels ---
        m, length = owall(sx, A_CH, CH_END, EAST_Y)
        B.push(m)
        gk.block_wall(B, 0.0, length, za, CH_TOP, "Ashlar", seed=s + 30)
        gk.course(B, "Limestone", [(0.0, CH_TOP - 0.3), (length, CH_TOP - 0.3)], s=1.0, caps=False)
        B.quad((0.0, -0.34, CH_TOP), (length, -0.34, CH_TOP), (length, 0.6, CH_TOP), (0.0, 0.6, CH_TOP), "Limestone", want=(0, 0, 1))
        B.pop()
        m, length = cwall(sx, EAST_Y, 6.0, A_CH, 1)
        B.push(m)
        gk.block_wall(B, 0.0, length, za, CH_TOP, "Ashlar", seed=s + 31)
        gk.course(B, "Limestone", [(0.0, CH_TOP - 0.3), (length, CH_TOP - 0.3)], s=1.0, caps=False)
        B.quad((0.0, -0.34, CH_TOP), (length, -0.34, CH_TOP), (length, 0.6, CH_TOP), (0.0, 0.6, CH_TOP), "Limestone", want=(0, 0, 1))
        B.pop()
        gk.corner_post(B, "Ashlar", (sx * A_CH, EAST_Y), (0.0, -1.0), n_out, (-float(sx), 0.0), (0.0, 1.0), za, CH_TOP)
    return {"annex A": AX_A, "annex B": AX_B, "east wall y": EAST_Y}


# ---------------------------------------------------------------------------------------------
def face_audit(ap, th, z0, z1, step=0.05):
    """The see-through check on the plane of the face with normal angle th, from two places outside it."""
    a = math.radians(th)
    n, t = np.array([math.cos(a), math.sin(a)]), np.array([-math.sin(a), math.cos(a)])
    o = np.array(C) + n * ap
    eyes = [(f"near {th}", tuple(o + n * 7.0 + t * 3.0) + (1.7,)), (f"far {th}", tuple(o + n * 24.0 - t * 9.0) + (1.7,))]
    # only the face itself: a sight line past the corner goes on over the neighbouring face's top or down to the ground
    return ((o[0], o[1], 0.0), (t[0], t[1], 0.0), (-ap * T12 + 0.1, ap * T12 - 0.1, z0, z1), step, eyes)


PARTS = {"low": build_low, "high": build_high, "annex": build_annex}
AUDITS = {
    "low": [face_audit(AP1, th, 0.061, Z1 - 0.4) for th in (15, 75, 105, 165, 195)],
    # each annex face on its own plane, both sides of the church
    "annex": [((sx * a, 0.0, 0.0), (0.0, 1.0, 0.0), (ya + 0.05, yb - 0.05, 0.061, zt - 0.2), 0.04,
               [("lane", (sx * 23.0, 22.0, 1.7)), ("rear", (sx * 22.5, 36.0, 1.7))])
              for sx in (-1, 1) for a, ya, yb, zt in (AX_A, AX_B)]
             + [((0.0, yp, 0.0), (float(sx), 0.0, 0.0), (a0 + 0.05, a1 - 0.05, 0.061, zt - 0.2), 0.04, [("rear", (sx * 21.0, 40.0, 1.7))])
                for sx in (-1, 1) for yp, a0, a1, zt in ((AX_A[2], AX_B[0], AX_A[0], AX_A[3]), (AX_B[2], 12.9, AX_B[0], AX_B[3]))],
}
SHOTS = {
    "low": [("rear", (0.0, 96.0, 1.7), (0.0, 50.0, 12.0), 28, 1500, 1100),
            ("bay", (5.0, 66.0, 1.7), (3.3, 52.0, 9.5), 24, 1100, 1500),
            ("oblique", (30.0, 62.0, 1.7), (9.0, 47.0, 9.0), 26, 1300, 1300),
            ("buttress", (10.5, 60.5, 1.7), (7.3, 52.3, 5.0), 26, 1100, 1400),
            ("parapet", (16.0, 66.0, 22.0), (7.0, 51.0, 19.0), 45, 1500, 1000)],
    "high": [("rear", (0.0, 110.0, 12.0), (0.0, 45.0, 22.0), 35, 1300, 1300),
             ("air", (40.0, 86.0, 44.0), (0.0, 40.0, 26.0), 40, 1500, 1100),
             ("windows", (17.0, 66.0, 24.0), (5.0, 48.0, 25.5), 40, 1400, 1100)],
    "annex": [("right", (34.0, 44.0, 10.0), (13.0, 28.0, 4.0), 30, 1500, 1100),
              ("lane", (22.5, 20.0, 1.7), (17.0, 29.0, 3.5), 24, 1300, 1100),
              ("left", (-34.0, 44.0, 10.0), (-13.0, 28.0, 4.0), 30, 1500, 1100)],
}
build_common.run("Apse", PARTS, AUDITS, SHOTS)
