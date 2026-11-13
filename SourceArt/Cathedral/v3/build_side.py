"""Builds the long sides of the Girona cathedral as sharp new stonework.

Reference photos: "Cathedral images/Right_side*.jpg", "Right_door.jpg", "Left_door.jpg", "Left_side_door.jpg",
"Rear_right.jpg", "Tower_closeup.JPG" (the nave window beside the tower). Coordinates are those of the Meshy
model (front = -Y, bell tower = +X, metres), as in build_front.py.

  blender -b --factory-startup --python build_side.py -- --side R --out <folder> --part <name> [--audit] ...
--side R (default) builds the right side, the one with the bell tower; --side L the left side. Everything
is laid out by its distance `a` from the church's long axis, so the left side is the same building turned
round, not a mirrored mesh.

Parts:
  portal    (right side) the deep Gothic doorway between the bell tower and the first buttress: porch wall,
            twelve moulded orders over a pointed arch, flat jambs with blind tracery and six niches each
            (corbels and canopies, ready for figures), tympanum, lintel, plank door, steps.
  chapels   the low chapel wall between the buttresses: base, traceried windows, cornice.
  nave      the buttresses from the ground to their sloping caps, with gargoyles, and the high wall of the
            nave behind them with its tall traceried windows and eaves cornice.

Writes Girona_Side<R|L>_<part>.blend and SM_Cathedral_Side<R|L>_<part>.fbx.
"""
import math
import os
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gothic_kit as gk  # noqa: E402
import build_common  # noqa: E402

SIDE = -1 if "--side" in sys.argv and sys.argv[sys.argv.index("--side") + 1].upper().startswith("L") else 1
ZONE = "SideR" if SIDE > 0 else "SideL"

# ---------------------------------------------------------------------------------------------
# Measured on the template (a = distance from the church's long axis, y along it, z up)
# ---------------------------------------------------------------------------------------------
A_TOWER = 16.05                         # right face of the bell tower and face of the porch
TOWER_Y1 = -35.118                      # where the tower's right face ends (its back-right corner starts)
TOWER_BACK = -32.268                    # the tower's back face
A_CH = 17.0                             # face of the chapel wall
A_NAVE = 10.85                          # face of the nave's high wall
A_BUT, A_BUT_UP = 17.7, 16.05           # front of the buttresses below and above their set-off
BUT_HW = 0.92                           # half width of a buttress (the template's are 0.8 to 0.9: they stay hidden inside)
BUTTS = (-22.9, -13.4, -1.1)            # centres of the three buttresses
NAVE_END, EAST_END = 20.5, 24.0         # where the nave's high wall and the chapel wall end, towards the apse
if SIDE > 0:
    HIGH_Y0, LOW_Y0 = TOWER_BACK, TOWER_Y1                # the high wall starts behind the bell tower, the porch beside it
    OLD_TOWER, A_OLD = (7.1, 14.1), 18.1                   # the old Romanesque tower: its span and its front
    CH_WINDOWS = [(-18.15,), (-10.2, -4.3), (3.4,), (17.0, 21.2)]     # chapel windows per bay (y of their axes)
else:
    HIGH_Y0 = LOW_Y0 = -35.6                               # back of the block behind the front's left corner
    OLD_TOWER, A_OLD = (13.9, 20.65), 17.8
    CH_WINDOWS = [(-18.15,), (-10.2, -4.3), (3.4, 9.6), (22.4,)]
CH_TOP = 10.85                          # top of the chapel wall
Z_SET0, Z_SET1 = 21.6, 23.0             # the buttresses step back between these heights
Z_BUT_TOP, Z_BUT_BACK = 27.8, 28.5      # their caps: front edge and where they meet the wall
EAVE = 29.95
PORCH_H = 17.2                          # keep in step with PORCH_TOP in build_bell_tower.py
PORTAL_Y = -30.0                        # axis of the side door
PLINTH_H, PLINTH_OUT = 2.75, 0.22       # as on the front and the tower
STRING = [[(0.0, 0.0), (0.05, 0.03), (0.07, 0.07)], [(0.07, 0.07), (0.07, 0.13)], [(0.07, 0.13), (0.03, 0.16), (0.0, 0.16)]]
PLINTH_TOP = [[(PLINTH_OUT, 0.0), (PLINTH_OUT, 0.06)], [(PLINTH_OUT, 0.06), (PLINTH_OUT - 0.03, 0.10), (0.11, 0.2), (0.09, 0.26)],
              [(0.09, 0.26), (0.09, 0.30), (0.0, 0.34)]]
BAYS = [(BUTTS[0] + BUT_HW, BUTTS[1] - BUT_HW), (BUTTS[1] + BUT_HW, BUTTS[2] - BUT_HW), (BUTTS[2] + BUT_HW, OLD_TOWER[0]),
        (OLD_TOWER[1], EAST_END)]
NAVE_BAYS = [(HIGH_Y0, BUTTS[0] - BUT_HW)] + BAYS[:3]      # the high wall also shows above the porch
if NAVE_END - OLD_TOWER[1] > 3.0:
    NAVE_BAYS.append((OLD_TOWER[1], NAVE_END))


# ---------------------------------------------------------------------------------------------
# Laying things out on either side
# ---------------------------------------------------------------------------------------------
def P(a, y, z=None):
    return (SIDE * a, y) if z is None else (SIDE * a, y, z)


def seq(items):
    """Order along the side as someone outside sees it from left to right."""
    return list(items) if SIDE > 0 else list(items)[::-1]


def outer_wall(a, ya, yb):
    """Frame of a wall that faces away from the church, at distance a, between ya < yb: (matrix, length)."""
    return gk.wall_frame((a, ya), (a, yb)) if SIDE > 0 else gk.wall_frame((-a, yb), (-a, ya))


def inner_wall(a, ya, yb):
    """Frame of a wall at distance a that faces the church's long axis (seen across the roofs)."""
    return gk.wall_frame((a, yb), (a, ya)) if SIDE > 0 else gk.wall_frame((-a, ya), (-a, yb))


def lx(y, ya, yb):
    """Local x in outer_wall(a, ya, yb) of the point at y."""
    return y - ya if SIDE > 0 else yb - y


def cross_wall(yp, a0, a1, facing):
    """Frame of a wall across the side (a buttress flank) at y = yp from a0 to a1; facing -1 looks
    towards the main front, +1 towards the apse."""
    xs = sorted((SIDE * a0, SIDE * a1))
    return gk.wall_frame((xs[0], yp), (xs[1], yp)) if facing < 0 else gk.wall_frame((xs[1], yp), (xs[0], yp))


def n_out():
    return (float(SIDE), 0.0)


def wall(B, frame, z0, z1, holes=(), seed=1, y=0.0, **kw):
    m, length = frame
    B.push(m)
    gk.block_wall(B, 0.0, length, z0, z1, "Ashlar", holes=holes, seed=seed, y=y, **kw)
    B.pop()


# ---------------------------------------------------------------------------------------------
# Part "portal" (right side)
# ---------------------------------------------------------------------------------------------
HALF_IN, HALF_OUT = 1.75, 4.45
P_DEPTH, P_REVEAL = 2.8, 0.25
P_Z0, P_SPRING, P_RISE = 0.45, 8.7, 3.3
DOOR_TOP, LINTEL_TOP = 6.9, 7.6


def plank_door(B, x0, x1, z0, z1, y, wood, iron, seed=3):
    """One leaf of a plank door in the plane `y`, facing -y: boards with open joints, three iron straps
    with split ends, rows of nails."""
    rng = np.random.default_rng(seed)
    n = max(3, int(round((x1 - x0) / 0.24)))
    w = (x1 - x0) / n
    B.quad((x0, y + 0.03, z0), (x1, y + 0.03, z0), (x1, y + 0.03, z1), (x0, y + 0.03, z1), iron, want=(0, -1, 0), var=0.2)   # dark behind the joints
    for i in range(n):
        B.box(x0 + i * w + 0.004, x0 + (i + 1) * w - 0.004, y, y + 0.03, z0, z1, wood, var=float(np.clip(rng.normal(0.5, 0.12), 0, 1)), faces="xXyzZ")
    h = z1 - z0
    for f in (0.12, 0.5, 0.86):
        zs = z0 + h * f
        B.box(x0 + 0.02, x1 - 0.06, y - 0.014, y, zs - 0.045, zs + 0.045, iron, faces="xXyzZ")
        for k in range(int((x1 - x0 - 0.1) / 0.16)):
            B.push(gk.translate(x0 + 0.08 + k * 0.16, y - 0.014, zs) @ gk.FACE_OUT)
            gk.stud(B, iron, 0.03, 0.018)
            B.pop()
    for i in range(n):                                    # nails top and bottom of every board
        for zz in (z0 + 0.1, z1 - 0.1):
            B.push(gk.translate(x0 + (i + 0.5) * w, y, zz) @ gk.FACE_OUT)
            gk.stud(B, iron, 0.022, 0.012)
            B.pop()


def build_portal(B):
    ya, yb = TOWER_Y1, BUTTS[0] - BUT_HW
    m, length = outer_wall(A_TOWER, ya, yb)
    xc = lx(PORTAL_Y, ya, yb)
    B.push(m @ gk.translate(xc, 0.0, 0.0))                 # x = 0 is the door's axis, y = 0 the porch face
    xl, xr = -xc, length - xc
    # --- the moulded doorway ------------------------------------------------------------------
    # Twelve fine orders over the arch only, springing from an impost band. Below it each jamb is a flat splayed
    # wall as on the Portal of the Apostles: blind tracery, a ledge, six niches with corbels and canopies.
    z_imp = P_SPRING - 0.42
    _pts, half_o, hood_top = gk.pointed_portal(B, HALF_IN, HALF_OUT, P_DEPTH, P_Z0, P_SPRING, P_RISE, "Limestone", "Limestone",
                                                orders=12, reveal=P_REVEAL, bands=[(z_imp, 0.42)], jambs=False)
    yd = P_DEPTH + P_REVEAL                                # the door plane
    slen = math.hypot(HALF_OUT - HALF_IN, P_DEPTH)
    niches = []
    for sx in (-1, 1):
        fm, _fl = gk.wall_frame((HALF_IN, P_DEPTH), (HALF_OUT, 0.0)) if sx > 0 else gk.wall_frame((-HALF_OUT, 0.0), (-HALF_IN, P_DEPTH))
        B.push(fm)
        spots = gk.portal_jamb(B, slen, P_Z0, z_imp, "Limestone")
        # a hidden plate behind the jamb, carried on behind the porch wall and up behind the impost band, so no
        # slanting sight line slips between the jamb, the wall and the band
        B.quad((-0.3, 0.26, P_Z0 - 0.3), (slen + 0.8, 0.26, P_Z0 - 0.3), (slen + 0.8, 0.26, P_SPRING + 0.6), (-0.3, 0.26, P_SPRING + 0.6),
               "Ashlar", want=(0, -1, 0), var=0.2)
        B.pop()
        for xs, zs, hs in spots:                           # where the twelve figures stand, in the building's coordinates
            q = B.stack[-1] @ fm @ np.array([xs, 0.2, zs, 1.0])
            niches.append((round(float(q[0]), 2), round(float(q[1]), 2), round(float(q[2]), 2), round(hs, 2)))
        # the reveal between the jamb and the door, and a carved stop where the hood mould ends
        B.quad((sx * HALF_IN, P_DEPTH - 0.01, P_Z0), (sx * HALF_IN, yd, P_Z0), (sx * HALF_IN, yd, z_imp), (sx * HALF_IN, P_DEPTH - 0.01, z_imp),
               "Limestone", want=(-sx, 0, 0))
        B.push(gk.translate(sx * (HALF_OUT + 0.13), 0.0, P_SPRING - 0.42 * 0.205 / 0.27))
        gk.figure_corbel(B, "Limestone", r=0.205)
        B.pop()
    for sx in (-1, 1):                                    # plain bases under the moulded jambs, a little proud of them
        nx, ny = -sx * P_DEPTH / slen * 0.14, -(HALF_OUT - HALF_IN) / slen * 0.14
        q = [(sx * (HALF_OUT + 0.3), -0.09), (sx * HALF_OUT + nx, -0.09), (sx * HALF_OUT + nx, ny), (sx * HALF_IN + nx, P_DEPTH + ny), (sx * HALF_IN + nx, yd)]
        for (xa, ya_), (xb, yb_) in zip(q, q[1:]):
            B.quad((xa, ya_, -0.1), (xb, yb_, -0.1), (xb, yb_, P_Z0 + 0.02), (xa, ya_, P_Z0 + 0.02), "Limestone", want=(-sx * abs(yb_ - ya_) - 1e-6 * sx, -abs(xb - xa) - 1e-6, 0.0))
        B.fan([(x, y, P_Z0 + 0.02) for x, y in q] + [(sx * (HALF_OUT + 0.3), yd, P_Z0 + 0.02)], "Limestone", (0, 0, 1))
    # --- porch wall with the arch cut out, its base, cornice and top --------------------------------
    holes = [(-HALF_OUT, HALF_OUT, -0.1, P_SPRING, "t")] + gk.stair_holes(0.0, half_o, P_SPRING, hood_top)
    gk.block_wall(B, xl, xr, -0.1, PORCH_H, "Ashlar", holes=holes, seed=301)
    k = PLINTH_OUT * gk.T8
    for x0, x1, sd in ((xl + k, -HALF_OUT - 0.26, 302), (HALF_OUT + 0.26, xr, 303)):        # base of the two piers
        gk.block_wall(B, x0, x1, -0.1, PLINTH_H, "Ashlar", y=-PLINTH_OUT, course=0.46, length=(0.8, 1.3), seed=sd)
        B.sweep([(min(x0, xl) if x0 < 0 else x0, 0.0, PLINTH_H), (x1, 0.0, PLINTH_H)], PLINTH_TOP, "Limestone")    # from the tower's moulding on
    for xe, nx in ((-HALF_OUT - 0.26, 1), (HALF_OUT + 0.26, -1)):                           # their ends at the arch
        B.quad((xe, -PLINTH_OUT, -0.1), (xe, 0.0, -0.1), (xe, 0.0, PLINTH_H + 0.3), (xe, -PLINTH_OUT, PLINTH_H + 0.3), "Limestone", want=(nx, 0, 0))
    for zs in (10.0, 12.95):                              # the tower's string courses run on to the hood mould
        B.sweep([(xl, 0.0, zs), (-half_o(zs) - 0.2, 0.0, zs)], STRING, "Limestone")
    gk.course(B, "Limestone", [(xl, PORCH_H - 0.39), (xr, PORCH_H - 0.39)], s=1.3, caps=False)
    B.push(gk.translate(0.0, -0.03, hood_top + 0.5) @ gk.FACE_OUT)                            # a boss over the apex
    gk.rosette(B, "Limestone", 0.3, 0.1, petals=8)
    B.pop()
    # --- tympanum, lintel, door ---------------------------------------------------------------------
    inner, half_i, _top = gk.pointed_arch(-HALF_IN, HALF_IN, P_SPRING, P_RISE)
    outside = []
    z = LINTEL_TOP
    while z < P_SPRING + P_RISE - 1e-9:
        zt = min(z + 0.1, P_SPRING + P_RISE)
        h = half_i(z)
        outside += [(-HALF_IN - 0.01, -h, z, zt, "tb"), (h, HALF_IN + 0.01, z, zt, "tb")]
        z = zt
    gk.block_wall(B, -HALF_IN, HALF_IN, LINTEL_TOP, P_SPRING + P_RISE, "Ashlar", holes=outside, y=yd, seed=304)
    blind, _h, _t = gk.pointed_arch(-HALF_IN + 0.4, HALF_IN - 0.4, P_SPRING - 0.45, P_RISE - 0.35)      # blind arch on the tympanum
    B.sweep([(x, yd, zz) for x, zz in [(-HALF_IN + 0.4, LINTEL_TOP + 0.02)] + blind + [(HALF_IN - 0.4, LINTEL_TOP + 0.02)]],
            [[(0.09, 0.0), (0.09, 0.04), (0.05, 0.04)], gk.half_round(0.05, -0.05, 0.04), [(-0.05, 0.04), (-0.09, 0.04), (-0.09, 0.0)]],
            "Limestone", b=(0, -1, 0), side=-1)
    B.push(gk.translate(0.0, yd - 0.01, P_SPRING + 1.05) @ gk.FACE_OUT)
    gk.rosette(B, "Limestone", 0.42, 0.11, petals=8)
    B.pop()
    B.box(-HALF_IN, HALF_IN, yd - 0.14, yd, DOOR_TOP, LINTEL_TOP, "Limestone", faces="yzZ")           # lintel
    for i in range(7):                                    # a row of small flowers along it
        B.push(gk.translate(-1.38 + i * 0.46, yd - 0.14, (DOOR_TOP + LINTEL_TOP) / 2) @ gk.FACE_OUT)
        gk.rosette(B, "Limestone", 0.15, 0.045, petals=6, layers=1)
        B.pop()
    B.sweep([(-HALF_IN, yd - 0.14, LINTEL_TOP), (HALF_IN, yd - 0.14, LINTEL_TOP)], STRING, "Limestone")
    ztop = P_SPRING + P_RISE + 0.3                        # a dark plate behind the leaves and the whole tympanum
    B.quad((-HALF_IN - 0.3, yd + 0.12, P_Z0 - 0.3), (HALF_IN + 0.3, yd + 0.12, P_Z0 - 0.3), (HALF_IN + 0.3, yd + 0.12, ztop),
           (-HALF_IN - 0.3, yd + 0.12, ztop), "Iron", want=(0, -1, 0), var=0.2)
    for sx in (-1, 1):                                    # the jamb carries on to that plate
        B.quad((sx * HALF_IN, yd, P_Z0), (sx * HALF_IN, yd + 0.12, P_Z0), (sx * HALF_IN, yd + 0.12, DOOR_TOP), (sx * HALF_IN, yd, DOOR_TOP),
               "Limestone", want=(-sx, 0, 0))
    for sx in (-1, 1):
        x0, x1 = sorted((sx * 0.012, sx * (HALF_IN - 0.02)))
        plank_door(B, x0, x1, P_Z0 + 0.01, DOOR_TOP - 0.01, yd + 0.06, "Bronze", "Iron", seed=5 + sx)
    B.push(gk.translate(0.22, yd + 0.046, P_Z0 + 1.25) @ gk.FACE_OUT)                                  # ring handle
    gk.rosette(B, "Iron", 0.07, 0.03, petals=8, layers=1)
    B.pop()
    B.tube([(0.22 + 0.09 * math.cos(t), yd + 0.02, P_Z0 + 1.13 + 0.09 * math.sin(t)) for t in np.radians(np.linspace(0, 360, 17)[:-1])], 0.012, "Iron", sides=6, closed=True)
    # --- floor of the recess and three steps between the splayed jambs -------------------------------------
    def half_at(y):                                        # half width of the recess at depth y
        return HALF_IN + (HALF_OUT - HALF_IN) * (1.0 - min(max(y, 0.0), P_DEPTH) / P_DEPTH) + 0.4
    for kstep, (yf, zt) in enumerate(((1.75, P_Z0), (1.3, P_Z0 - 0.15), (0.85, P_Z0 - 0.30))):
        ybk = yd if kstep == 0 else (1.75, 1.3)[kstep - 1]
        hf, hb = half_at(yf), half_at(ybk)
        B.fan([(-hf, yf, zt), (hf, yf, zt), (hb, ybk, zt), (-hb, ybk, zt)], "Paving", (0, 0, 1), var=0.5 + 0.08 * kstep)
        B.quad((-hf, yf, zt - 0.15), (hf, yf, zt - 0.15), (hf, yf, zt), (-hf, yf, zt), "Paving", want=(0, -1, 0), var=0.42)
    hf0, hf1 = half_at(0.0), half_at(0.85)                 # threshold slab in the mouth of the doorway, just above the street
    B.fan([(-hf0, -0.05, 0.012), (hf0, -0.05, 0.012), (hf1, 0.85, 0.012), (-hf1, 0.85, 0.012)], "Paving", (0, 0, 1), var=0.45)
    B.pop()
    # --- flat roof of the porch, back to the nave wall, round the tower's corner -----------------------
    roof = [P(A_TOWER, ya), P(A_TOWER, yb), P(A_NAVE, yb), P(A_NAVE, TOWER_BACK), P(13.199, TOWER_BACK)]
    B.fan([(x, y, PORCH_H) for x, y in roof], "Paving", (0, 0, 1), var=0.35)
    # the inside corner against the first buttress
    gk.seal_inside_corner(B, "Ashlar", P(A_TOWER, yb), n_out(), (0.0, -1.0), -0.1, PORCH_H)
    return {"hood top": round(hood_top, 2), "door plane a": round(A_TOWER - yd, 2), "span": (round(xl, 3), round(xr, 3)), "niches (x, y, z, clear height)": niches}


# ---------------------------------------------------------------------------------------------
# Part "chapels"
# ---------------------------------------------------------------------------------------------
def build_chapels(B):
    for i, ((ya, yb), wins) in enumerate(zip(BAYS, CH_WINDOWS)):
        frame = outer_wall(A_CH, ya, yb)
        m, length = frame
        B.push(m)
        holes = []
        for yw in wins:
            xw = lx(yw, ya, yb)
            holes += gk.tracery_window(B, xw - 0.75, xw + 0.75, 4.2, 7.4, "Limestone", "Glass", "Iron", lights=2, recess=0.45, frame_w=0.2)
        gk.block_wall(B, 0.0, length, -0.1, CH_TOP, "Ashlar", holes=holes, seed=320 + i)
        gk.block_wall(B, 0.0, length, -0.1, PLINTH_H, "Ashlar", y=-PLINTH_OUT, course=0.46, length=(0.8, 1.3), seed=330 + i)
        B.sweep([(0.0, 0.0, PLINTH_H), (length, 0.0, PLINTH_H)], PLINTH_TOP, "Limestone")
        gk.course(B, "Limestone", [(0.0, CH_TOP - 0.3), (length, CH_TOP - 0.3)], s=1.0, caps=False)
        B.quad((0.0, -0.34, CH_TOP), (length, -0.34, CH_TOP), (length, 0.6, CH_TOP), (0.0, 0.6, CH_TOP), "Limestone", want=(0, 0, 1))
        B.pop()
        # inside corners against the buttress flanks (and the old tower at the last bay)
        gk.seal_inside_corner(B, "Ashlar", P(A_CH, ya), n_out(), (0.0, 1.0), -0.1, CH_TOP)
        gk.seal_inside_corner(B, "Ashlar", P(A_CH, yb), n_out(), (0.0, -1.0), -0.1, CH_TOP)
    return {"bays": [(round(a, 2), round(b, 2)) for a, b in BAYS], "top": CH_TOP}


# ---------------------------------------------------------------------------------------------
# Part "nave": buttresses and the high wall
# ---------------------------------------------------------------------------------------------
def tri(B, a, b, c, mat, want, var=0.5):
    B.add(np.array([a, b, c], dtype=np.float64), np.array([(0, 1, 2)] if np.dot(np.cross(np.subtract(b, a), np.subtract(c, a)), want) > 0 else [(2, 1, 0)]), mat, var=var)


def buttress(B, yc, idx, first=False):
    y0, y1 = yc - BUT_HW, yc + BUT_HW
    po = PLINTH_OUT
    # front, below and above the set-off
    wall(B, outer_wall(A_BUT, y0, y1), -0.1, Z_SET0, seed=340 + idx)
    wall(B, outer_wall(A_BUT_UP, y0, y1), Z_SET1, Z_BUT_TOP, seed=350 + idx)
    for yp, facing, sd in ((y0, -1, 360), (y1, 1, 370)):
        low_a0 = A_TOWER if (first and facing < 0) else A_CH                 # the first buttress stands against the porch
        low_top = PORCH_H if (first and facing < 0) else CH_TOP
        wall(B, cross_wall(yp, low_a0, A_BUT, facing), -0.1, low_top, seed=sd + idx)          # beside the chapel wall
        wall(B, cross_wall(yp, A_NAVE, A_BUT, facing), low_top, Z_SET0, seed=sd + 3 + idx)    # above the chapel roof
        wall(B, cross_wall(yp, A_NAVE, A_BUT_UP, facing), Z_SET0, Z_BUT_TOP, seed=sd + 6 + idx)
        nrm = (0.0, float(facing), 0.0)
        tri(B, P(A_BUT_UP, yp, Z_SET0), P(A_BUT, yp, Z_SET0), P(A_BUT_UP, yp, Z_SET1), "Limestone", nrm)      # under the set-off
        tri(B, P(A_NAVE, yp, Z_BUT_TOP), P(A_BUT_UP, yp, Z_BUT_TOP), P(A_NAVE, yp, Z_BUT_BACK), "Ashlar", nrm)  # under the cap
        # outside corners of both stages
        along = (-float(SIDE), 0.0)
        gk.corner_post(B, "Ashlar", P(A_BUT, yp), along, (0.0, float(facing)), (0.0, -float(facing)), n_out(), PLINTH_H, Z_SET0)
        gk.corner_post(B, "Ashlar", P(A_BUT_UP, yp), along, (0.0, float(facing)), (0.0, -float(facing)), n_out(), Z_SET1, Z_BUT_TOP)
    # the set-off: a weathered slope with a drip moulding under it
    B.quad(P(A_BUT, y0, Z_SET0), P(A_BUT, y1, Z_SET0), P(A_BUT_UP, y1, Z_SET1), P(A_BUT_UP, y0, Z_SET1), "Limestone", want=(SIDE * 0.6, 0, 0.8))
    drip = seq([(A_BUT - 1.2, y0), (A_BUT, y0), (A_BUT, y1), (A_BUT - 1.2, y1)])
    B.sweep([P(a, y, Z_SET0 - 0.16) for a, y in drip], STRING, "Limestone")
    # base course round the three free faces: its proud faces, then its moulded top along the wall faces
    a_low = A_TOWER if first else A_CH                    # what the flank towards the front stands against
    proud = seq([(a_low + po, y0 - po), (A_BUT + po, y0 - po), (A_BUT + po, y1 + po), (A_CH + po, y1 + po)])
    path = [P(a, y) for a, y in proud]
    for i, (p, q) in enumerate(zip(path, path[1:])):
        m, length = gk.wall_frame(p, q)
        B.push(m)
        gk.block_wall(B, 0.0, length, -0.1, PLINTH_H, "Ashlar", course=0.46, length=(0.8, 1.3), seed=380 + idx * 3 + i)
        B.pop()
    faces = seq([(a_low, y0), (A_BUT, y0), (A_BUT, y1), (A_CH, y1)])
    B.sweep([P(a, y, PLINTH_H) for a, y in faces], PLINTH_TOP, "Limestone")
    for yp, facing in ((y0 - po, -1.0), (y1 + po, 1.0)):
        gk.corner_post(B, "Ashlar", P(A_BUT + po, yp), (-float(SIDE), 0.0), (0.0, facing), (0.0, -facing), n_out(), -0.1, PLINTH_H, leg=0.2, proud=0.025)
    # cap and gargoyle
    B.push(gk.translate(0.0, yc, 0.0) @ gk.rot_z(0.0 if SIDE > 0 else 180.0))
    gk.buttress_cap(B, "Limestone", A_NAVE, A_BUT_UP, BUT_HW, Z_BUT_TOP, Z_BUT_BACK)
    B.push(gk.translate(A_BUT_UP, 0.0, Z_BUT_TOP - 1.3))
    gk.gargoyle(B, "Limestone", 1.35)
    B.pop()
    B.pop()


def build_nave(B):
    for idx, yc in enumerate(BUTTS):
        buttress(B, yc, idx, first=(idx == 0))
    # the high wall of the nave: one tall traceried window in every bay
    for i, (ya, yb) in enumerate(NAVE_BAYS):
        z0 = PORCH_H if i == 0 else CH_TOP
        m, length = outer_wall(A_NAVE, ya, yb)
        B.push(m)
        sill = max(17.6, z0 + 1.2)
        holes = []
        nw = 1 if length < 11.0 else 2
        for kw in range(nw):
            xw = length * (kw + 0.5) / nw
            holes += gk.tracery_window(B, xw - 1.2, xw + 1.2, sill, 24.6, "Limestone", "Glass", "Iron", lights=3, recess=0.5, frame_w=0.24, pane=(0.24, 0.36))
        gk.block_wall(B, 0.0, length, z0, EAVE, "Ashlar", holes=holes, seed=400 + i)
        gk.course(B, "Limestone", [(0.0, EAVE - 0.39), (length, EAVE - 0.39)], s=1.3, caps=False)
        B.quad((0.0, -0.44, EAVE), (length, -0.44, EAVE), (length, 0.6, EAVE), (0.0, 0.6, EAVE), "Limestone", want=(0, 0, 1))
        B.pop()
        lo = max(z0, CH_TOP)
        gk.seal_inside_corner(B, "Ashlar", P(A_NAVE, ya), n_out(), (0.0, 1.0), lo, Z_BUT_TOP)
        gk.seal_inside_corner(B, "Ashlar", P(A_NAVE, yb), n_out(), (0.0, -1.0), lo, Z_BUT_TOP)
    return {"buttresses": BUTTS, "eave": EAVE}


# ---------------------------------------------------------------------------------------------
# ---------------------------------------------------------------------------------------------
# Part "oldtower": the Romanesque tower that rises through the chapels on each side
# ---------------------------------------------------------------------------------------------
OT_BASE, OT_STOREY, OT_N = CH_TOP + 0.35, 3.95, 5
OT_TOP = OT_BASE + OT_STOREY * OT_N


def tower_face(B, frame, z0, z1, seed, lesene=0.55, twins=2):
    """One storey of one face of the old tower: plain corner strips, between them a sunk field with a
    row of small arches along its top and twin windows in it, and a string course on top."""
    m, length = frame
    D = 0.14
    px0, px1, pz0, pz1 = lesene, length - lesene, z0 + 0.25, z1 - 0.32
    B.push(m)
    gk.block_wall(B, 0.0, length, z0, z1, "Ashlar", holes=[(px0, px1, pz0, pz1)], seed=seed)
    zs = gk.lombard_band(B, px0, px1, pz1, D, "Limestone")
    holes = []
    B.push(gk.translate(0.0, D, 0.0))
    for k in range(twins):
        holes += gk.bifora(B, px0 + (px1 - px0) * (k + 0.5) / twins, pz0 + 0.75, "Limestone", "Iron")
    B.pop()
    gk.block_wall(B, px0, px1, pz0, pz1, "Ashlar", holes=holes, y=D, seed=seed + 50)
    B.quad((px0, 0.0, pz0), (px0, D, pz0), (px0, D, zs), (px0, 0.0, zs), "Ashlar", want=(1, 0, 0))
    B.quad((px1, 0.0, pz0), (px1, D, pz0), (px1, D, zs), (px1, 0.0, zs), "Ashlar", want=(-1, 0, 0))
    B.quad((px0, 0.0, pz0), (px1, 0.0, pz0), (px1, D, pz0), (px0, D, pz0), "Limestone", want=(0, 0, 1))
    gk.course(B, "Limestone", [(0.0, z1 - 0.12), (length, z1 - 0.12)], s=0.4, caps=False)
    B.pop()


def build_oldtower(B):
    y0, y1 = OLD_TOWER
    po = PLINTH_OUT
    # the foot, down among the chapels: front and the two short flanks, with a slit window and the base course
    m, length = outer_wall(A_OLD, y0, y1)
    B.push(m)
    slit = gk.opening(B, length / 2 - 0.16, length / 2 + 0.16, 4.6, 6.6, "Limestone", "Iron", arched=True, recess=0.4, frame_w=0.1, proud=0.03)
    gk.block_wall(B, 0.0, length, -0.1, OT_BASE, "Ashlar", holes=[slit], seed=600)
    gk.course(B, "Limestone", [(0.0, OT_BASE - 0.12), (length, OT_BASE - 0.12)], s=0.4, caps=False)
    B.pop()
    for yp, facing, sd in ((y0, -1, 601), (y1, 1, 602)):
        wall(B, cross_wall(yp, A_CH, A_OLD, facing), -0.1, OT_BASE, seed=sd)
        gk.corner_post(B, "Ashlar", P(A_OLD, yp), (-float(SIDE), 0.0), (0.0, float(facing)), (0.0, -float(facing)), n_out(), PLINTH_H, OT_TOP + 0.65)
    proud = seq([(A_CH + po, y0 - po), (A_OLD + po, y0 - po), (A_OLD + po, y1 + po), (A_CH + po, y1 + po)])
    path = [P(a, y) for a, y in proud]
    for i, (p, q) in enumerate(zip(path, path[1:])):
        m, length = gk.wall_frame(p, q)
        B.push(m)
        gk.block_wall(B, 0.0, length, -0.1, PLINTH_H, "Ashlar", course=0.46, length=(0.8, 1.3), seed=603 + i)
        B.pop()
    B.sweep([P(a, y, PLINTH_H) for a, y in seq([(A_CH, y0), (A_OLD, y0), (A_OLD, y1), (A_CH, y1)])], PLINTH_TOP, "Limestone")
    for yp, facing in ((y0 - po, -1.0), (y1 + po, 1.0)):
        gk.corner_post(B, "Ashlar", P(A_OLD + po, yp), (-float(SIDE), 0.0), (0.0, facing), (0.0, -facing), n_out(), -0.1, PLINTH_H, leg=0.2, proud=0.025)
    # five arcaded storeys on the three free faces
    for s in range(OT_N):
        z0 = OT_BASE + s * OT_STOREY
        tower_face(B, outer_wall(A_OLD, y0, y1), z0, z0 + OT_STOREY, 610 + s * 3)
        tower_face(B, cross_wall(y0, A_NAVE, A_OLD, -1), z0, z0 + OT_STOREY, 611 + s * 3)
        tower_face(B, cross_wall(y1, A_NAVE, A_OLD, 1), z0, z0 + OT_STOREY, 612 + s * 3)
    # parapet and flat top
    zt = OT_TOP + 0.65
    for frame, sd in ((outer_wall(A_OLD, y0, y1), 630), (cross_wall(y0, A_NAVE, A_OLD, -1), 631), (cross_wall(y1, A_NAVE, A_OLD, 1), 632)):
        m, length = frame
        B.push(m)
        gk.block_wall(B, 0.0, length, OT_TOP, zt, "Ashlar", seed=sd)
        gk.course(B, "Limestone", [(0.0, zt - 0.2), (length, zt - 0.2)], s=0.66, caps=False)
        B.quad((0.0, -0.22, zt), (length, -0.22, zt), (length, 0.5, zt), (0.0, 0.5, zt), "Limestone", want=(0, 0, 1))
        B.pop()
    B.fan([P(A_NAVE, y0, zt - 0.3), P(A_OLD, y0, zt - 0.3), P(A_OLD, y1, zt - 0.3), P(A_NAVE, y1, zt - 0.3)], "Paving", (0, 0, 1), var=0.35)
    wall(B, inner_wall(A_NAVE, y0, y1), EAVE - 0.6, zt, seed=633)       # its back, seen across the nave roof
    return {"y": OLD_TOWER, "front a": A_OLD, "top": round(zt, 2)}


# ---------------------------------------------------------------------------------------------
# Part "corner" (left side): the block behind the front's left corner, the bell tower's counterpart
# ---------------------------------------------------------------------------------------------
FRONT_CORNER_Y = -42.2 + 3.05           # where the front's 45 degree corner meets this side (build_front.py)
STUB = 0.03
TOP_Z = 32.95
CORNICE_Z, CORNICE_S = 31.0, 1.3


def build_corner(B):
    ya, yb = FRONT_CORNER_Y, LOW_Y0
    r2 = math.sqrt(0.5)
    m, length = outer_wall(A_TOWER, ya, yb)
    B.push(m)
    gk.block_wall(B, 0.0, length, -0.1, TOP_Z, "Ashlar", seed=520)
    B.quad((0.0, 0.0, TOP_Z), (length, 0.0, TOP_Z), (length, 0.6, TOP_Z), (0.0, 0.6, TOP_Z), "Ashlar", want=(0, 0, 1))
    k = PLINTH_OUT * gk.T8
    xk0, xk1 = (0.0, length + k) if SIDE < 0 else (-k, length)             # the base reaches the front's corner with a mitre
    gk.block_wall(B, xk0, xk1, -0.1, PLINTH_H, "Ashlar", y=-PLINTH_OUT, course=0.46, length=(0.8, 1.3), seed=521)
    B.pop()
    # corner strips against the front's 45 degree corner, on the wall and on the base
    a, na, b, nb = (-SIDE * r2, -r2), (SIDE * r2, -r2), (0.0, 1.0), n_out()
    gk.corner_post(B, "Ashlar", P(A_TOWER, ya), a, na, b, nb, PLINTH_H, TOP_Z)
    gk.corner_post(B, "Ashlar", P(A_TOWER + PLINTH_OUT, ya - k), a, na, b, nb, -0.1, PLINTH_H, leg=0.2, proud=0.025)
    # mouldings carried on from the front's stubs
    run = seq([(A_TOWER, ya + STUB), (A_TOWER, yb)])
    B.sweep([P(a_, y, PLINTH_H) for a_, y in run], PLINTH_TOP, "Limestone")
    for zs in (10.0, 12.95):
        B.sweep([P(a_, y, zs) for a_, y in run], STRING, "Limestone")
    # its back, above the porch, as far as the nave wall
    wall(B, cross_wall(yb, A_NAVE - 0.05, A_TOWER, 1), PORCH_H, TOP_Z, seed=522)
    gk.corner_post(B, "Ashlar", P(A_TOWER, yb), (0.0, -1.0), n_out(), (-float(SIDE), 0.0), (0.0, 1.0), PORCH_H, TOP_Z)
    s = CORNICE_S
    prof = [[(u * s, v * s) for u, v in run_] for run_ in gk.entablature_profile()]
    prof.append([(gk.CORNICE_OUT * s, gk.ENTABLATURE_H * s), (0.0, gk.ENTABLATURE_H * s)])
    B.sweep([P(a_, y, CORNICE_Z) for a_, y in seq([(A_TOWER, ya + STUB), (A_TOWER, yb), (A_NAVE - 0.05, yb)])], prof, "Limestone")
    roof = [P(A_TOWER, ya), P(A_TOWER, yb), P(A_NAVE, yb), P(A_NAVE, ya - 2.3), P(13.2, ya - 2.3)]
    B.fan([(x, y, TOP_Z - 0.012) for x, y in roof], "Paving", (0, 0, 1), var=0.35)
    wall(B, inner_wall(A_NAVE - 0.05, ya - 2.3, yb), EAVE - 0.6, TOP_Z, seed=523)       # its inner side, seen across the nave roof
    return {"side face y": (round(ya, 3), yb), "top": TOP_Z}


# ---------------------------------------------------------------------------------------------
# Part "doorway" (left side): the columned doorway of "Door_somewhere.webp", set back in the porch wall
# ---------------------------------------------------------------------------------------------
DW_HALF, DW_TOP, DW_DEPTH, DW_FLOOR = 3.9, 8.6, 1.9, 0.45


def build_doorway(B):
    ya, yb = LOW_Y0, BUTTS[0] - BUT_HW
    m, length = outer_wall(A_TOWER, ya, yb)
    xc = length / 2
    B.push(m @ gk.translate(xc, 0.0, 0.0))                 # x = 0 is the door's axis, y = 0 the porch face
    xl, xr = -xc, length - xc
    H, D, fz = DW_HALF, DW_DEPTH, DW_FLOOR
    # --- porch wall with the recess cut out, the frame round the recess, base course, cornice -----------
    gk.block_wall(B, xl, xr, -0.1, PORCH_H, "Ashlar", holes=[(-H, H, -0.1, DW_TOP)], seed=501)
    B.sweep([(-H, 0.0, 0.0), (-H, 0.0, DW_TOP), (H, 0.0, DW_TOP), (H, 0.0, 0.0)], gk.frame_profile(0.24, 0.07), "Limestone", b=(0, -1, 0), side=-1)
    for x0, x1, sd in ((xl, -H - 0.24, 502), (H + 0.24, xr, 503)):
        gk.block_wall(B, x0, x1, -0.1, PLINTH_H, "Ashlar", y=-PLINTH_OUT, course=0.46, length=(0.8, 1.3), seed=sd)
        B.sweep([(x0, 0.0, PLINTH_H), (x1, 0.0, PLINTH_H)], PLINTH_TOP, "Limestone")
    for xe, nx in ((-H - 0.24, 1), (H + 0.24, -1)):
        B.quad((xe, -PLINTH_OUT, -0.1), (xe, 0.0, -0.1), (xe, 0.0, PLINTH_H + 0.3), (xe, -PLINTH_OUT, PLINTH_H + 0.3), "Limestone", want=(nx, 0, 0))
    for zs in (10.0, 12.95):
        B.sweep([(xl, 0.0, zs), (xr, 0.0, zs)], STRING, "Limestone")
    gk.course(B, "Limestone", [(xl, PORCH_H - 0.39), (xr, PORCH_H - 0.39)], s=1.3, caps=False)
    # --- the recess: side walls, ceiling ------------------------------------------------------------------
    for sx in (-1, 1):
        fm, fl = gk.wall_frame((-H, 0.0), (-H, D)) if sx < 0 else gk.wall_frame((H, D), (H, 0.0))
        B.push(fm)
        gk.block_wall(B, 0.0, fl, -0.1, DW_TOP, "Ashlar", seed=504 + sx)
        B.pop()
        gk.corner_post(B, "Limestone", (sx * H, 0.0), (float(sx), 0.0), (0.0, -1.0), (0.0, 1.0), (-float(sx), 0.0), -0.1, DW_TOP, leg=0.12, proud=0.012)
        gk.seal_inside_corner(B, "Ashlar", (sx * H, D), (-float(sx), 0.0), (0.0, -1.0), -0.1, DW_TOP)
    B.quad((-H - 0.03, 0.0, DW_TOP), (H + 0.03, 0.0, DW_TOP), (H + 0.03, D + 0.03, DW_TOP), (-H - 0.03, D + 0.03, DW_TOP), "Limestone", want=(0, 0, -1))
    # --- the back wall of the recess: door, four columns on pedestals, two shell niches, entablature ----------
    B.push(gk.translate(0.0, D, 0.0))                       # y = 0 is now the back wall
    cols, niches = (-3.25, -1.72, 1.72, 3.25), (-2.485, 2.485)
    ped_h, shaft_h, cap_h, out = 1.05, 3.2, 0.5, 0.3
    for cx in cols:
        B.push(gk.translate(cx, 0.0, fz))
        gk.pedestal(B, "Limestone", 0.66, 0.5 + 0.12, ped_h)
        B.pop()
        B.push(gk.translate(cx, -out, fz + ped_h))
        bh = gk.attic_base(B, "Limestone", r=0.2, plinth=0.56)
        B.push(gk.translate(0.0, 0.0, bh))
        gk.column_shaft(B, "Marble", 0.2, 0.17, shaft_h)
        B.push(gk.translate(0.0, 0.0, shaft_h))
        gk.corinthian_capital(B, "Limestone", r=0.17, height=cap_h, abacus=0.56)
        B.pop()
        B.pop()
        B.pop()
    ent_z = fz + ped_h + 0.253 + shaft_h + cap_h
    path = [(-H, -0.08)]
    for cx in cols:
        path += [(cx - 0.36, -0.08), (cx - 0.36, -0.54), (cx + 0.36, -0.54), (cx + 0.36, -0.08)]
    path += [(H, -0.08)]
    gk.entablature(B, "Limestone", path, ent_z, end_caps=False)
    ent_top = ent_z + gk.ENTABLATURE_H
    for cx in cols:                                         # blocks over the columns, each with a carved boss
        B.box(cx - 0.3, cx + 0.3, -0.5, 0.0, ent_top, ent_top + 0.72, "Limestone", faces="xXyZ")
        B.push(gk.translate(cx, -0.5, ent_top + 0.36) @ gk.FACE_OUT)
        gk.rosette(B, "Limestone", 0.2, 0.09, petals=8)
        B.pop()
    gk.course(B, "Limestone", [(-H, ent_top + 0.72), (H, ent_top + 0.72)], s=0.9, caps=False)
    holes = []
    for nx in niches:
        B.push(gk.translate(nx, 0.0, 0.0))
        gk.shell_niche(B, "Limestone", 0.36, fz + ped_h, fz + ped_h + 1.75, ribs=9)
        B.pop()
        holes.append((nx - 0.36, nx + 0.36, fz + ped_h, fz + ped_h + 1.75 + 0.36))
    hw, dz1, rev = 1.15, fz + 3.9, 0.3
    holes.append((-hw, hw, fz - 0.05, dz1))
    B.quad((-hw, -0.02, fz), (hw, -0.02, fz), (hw, rev + 0.08, fz), (-hw, rev + 0.08, fz), "Paving", want=(0, 0, 1), var=0.4)      # threshold
    B.sweep([(-hw, 0.0, fz), (-hw, 0.0, dz1), (hw, 0.0, dz1), (hw, 0.0, fz)], gk.frame_profile(0.2, 0.075), "Marble", b=(0, -1, 0), side=-1)
    B.quad((-hw, 0.0, fz), (-hw, rev, fz), (-hw, rev, dz1), (-hw, 0.0, dz1), "Marble", want=(1, 0, 0))
    B.quad((hw, 0.0, fz), (hw, rev, fz), (hw, rev, dz1), (hw, 0.0, dz1), "Marble", want=(-1, 0, 0))
    B.quad((-hw, 0.0, dz1), (hw, 0.0, dz1), (hw, rev, dz1), (-hw, rev, dz1), "Marble", want=(0, 0, -1))
    B.quad((-hw - 0.3, rev + 0.08, fz - 0.3), (hw + 0.3, rev + 0.08, fz - 0.3), (hw + 0.3, rev + 0.08, dz1 + 0.3), (-hw - 0.3, rev + 0.08, dz1 + 0.3),
           "Iron", want=(0, -1, 0), var=0.2)
    for sx in (-1, 1):
        x0, x1 = sorted((sx * 0.01, sx * (hw - 0.015)))
        plank_door(B, x0, x1, fz + 0.01, dz1 - 0.01, rev, "PaintGrey", "Iron", seed=9 + sx)
    gk.block_wall(B, -H, H, fz - 0.05, DW_TOP, "Limestone", holes=holes, course=0.42, length=(0.7, 1.2), seed=506)
    # floor of the recess and three steps in its mouth
    gk.slab_floor(B, -H, H, -D + 0.9, 0.0, fz, "Paving", size=(1.0, 0.75), seed=507)
    for kstep, (yf, zt) in enumerate(((-D + 0.9, fz), (-D + 0.6, fz - 0.15), (-D + 0.3, fz - 0.30))):
        if kstep:
            B.quad((-H, yf, zt), (H, yf, zt), (H, yf + 0.3, zt), (-H, yf + 0.3, zt), "Paving", want=(0, 0, 1), var=0.5 + 0.06 * kstep)
        B.quad((-H, yf, zt - 0.15), (H, yf, zt - 0.15), (H, yf, zt), (-H, yf, zt), "Paving", want=(0, -1, 0), var=0.42)
    B.quad((-H, -D - 0.05, 0.012), (H, -D - 0.05, 0.012), (H, -D + 0.3, 0.012), (-H, -D + 0.3, 0.012), "Paving", want=(0, 0, 1), var=0.45)
    B.pop()
    B.pop()
    # flat roof of the porch, back to the nave wall; the inside corner against the first buttress
    roof = [P(A_TOWER, ya), P(A_TOWER, yb), P(A_NAVE, yb), P(A_NAVE, ya)]
    B.fan([(x, y, PORCH_H) for x, y in roof], "Paving", (0, 0, 1), var=0.35)
    gk.seal_inside_corner(B, "Ashlar", P(A_TOWER, yb), n_out(), (0.0, -1.0), -0.1, PORCH_H)
    return {"span": (round(xl, 3), round(xr, 3)), "entablature": (round(ent_z, 3), round(ent_top, 3))}


PARTS = {"chapels": build_chapels, "nave": build_nave, "oldtower": build_oldtower}
if SIDE > 0:
    PARTS["portal"] = build_portal
else:
    PARTS["corner"] = build_corner
    PARTS["doorway"] = build_doorway
LANE = [("lane low", (SIDE * 20.5, -30.0, 1.7)), ("lane front", (SIDE * 20.5, -40.0, 1.7)), ("lane rear", (SIDE * 20.5, -8.0, 1.7)), ("far", (SIDE * 30.0, -18.0, 3.0))]
AUDITS = {
    "portal": [((SIDE * A_TOWER, 0.0, 0.0), (0.0, 1.0, 0.0), (TOWER_Y1 + 0.05, BUTTS[0] - BUT_HW - 0.05, 0.061, PORCH_H - 0.05), 0.03, LANE)],
    "doorway": [((SIDE * A_TOWER, 0.0, 0.0), (0.0, 1.0, 0.0), (LOW_Y0 + 0.05, BUTTS[0] - BUT_HW - 0.05, 0.061, PORCH_H - 0.05), 0.03, LANE)],
    "corner": [((SIDE * A_TOWER, 0.0, 0.0), (0.0, 1.0, 0.0), (FRONT_CORNER_Y + 0.05, LOW_Y0 - 0.05, 0.061, 30.9), 0.04, LANE[:2])],   # 0.061: no grid line on a moulding edge
    # as far as the old tower: the bay beyond it ends open towards the apse, which is built later
    "chapels": [((SIDE * A_CH, 0.0, 0.0), (0.0, 1.0, 0.0), (BAYS[0][0] + 0.05, BAYS[2][1] - 0.05, 0.05, CH_TOP - 0.05), 0.04, LANE)],
    # only as far as the last buttress and below the chapel roof: beyond are the old tower and the roofs, built later
    "nave": [((SIDE * A_BUT, 0.0, 0.0), (0.0, 1.0, 0.0), (BUTTS[0] - BUT_HW + 0.05, BUTTS[-1] + BUT_HW - 0.05, 0.3, 9.0), 0.04, LANE[:3])],
}
S = SIDE
OTC = (OLD_TOWER[0] + OLD_TOWER[1]) / 2
SHOTS = {
    "doorway": [("front", (S * 34.0, -29.7, 1.7), (S * 15.0, -29.7, 6.0), 28, 1300, 1300),
                ("oblique", (S * 24.0, -38.0, 1.7), (S * 14.8, -30.0, 4.5), 28, 1300, 1300),
                ("door", (S * 19.5, -29.2, 1.7), (S * 14.4, -29.7, 3.4), 24, 1300, 1300),
                ("capitals", (S * 18.2, -27.6, 2.6), (S * 14.9, -27.9, 5.9), 40, 1300, 1000)],
    "corner": [("side", (S * 40.0, -48.0, 1.7), (S * 15.0, -37.0, 16.0), 24, 1000, 1500)],
    "oldtower": [("wide", (S * 50.0, OTC - 16.0, 14.0), (S * 15.0, OTC, 19.0), 35, 1100, 1500),
                 ("storey", (S * 27.0, OTC - 3.0, 20.0), (S * 18.0, OTC, 21.0), 40, 1300, 1100),
                 ("band", (S * 21.5, OTC - 1.5, 16.6), (S * 18.0, OTC - 0.6, 18.3), 40, 1300, 1000)],
    "portal": [("front", (S * 34.0, -30.0, 1.7), (S * 15.0, -30.0, 8.5), 28, 1200, 1400),
               ("oblique", (S * 24.0, -39.0, 1.7), (S * 14.5, -30.5, 6.5), 28, 1200, 1400),
               ("door", (S * 20.5, -30.6, 1.7), (S * 13.0, -30.0, 4.6), 28, 1100, 1400),
               ("orders", (S * 18.2, -33.2, 1.7), (S * 14.2, -32.2, 3.2), 28, 1300, 1100)],
    "chapels": [("bays", (S * 30.0, -18.0, 1.7), (S * 17.0, -10.0, 6.0), 26, 1500, 1000),
                ("window", (S * 21.0, -19.6, 1.7), (S * 17.0, -18.15, 6.6), 30, 1100, 1300),
                ("base", (S * 19.5, -8.0, 1.5), (S * 17.0, -6.0, 2.0), 26, 1400, 1000)],
    "nave": [("wide", (S * 52.0, -30.0, 12.0), (S * 12.0, -12.0, 17.0), 30, 1600, 1100),
             ("buttress", (S * 26.0, -19.0, 1.7), (S * 15.0, -13.4, 16.0), 22, 1100, 1500),
             ("window", (S * 24.0, -22.5, 22.0), (S * 10.85, -18.15, 22.5), 40, 1100, 1400),
             ("gargoyle", (S * 20.5, -16.5, 24.5), (S * 16.6, -13.4, 26.6), 60, 1300, 1000)],
}
build_common.run(ZONE, PARTS, AUDITS, SHOTS)
