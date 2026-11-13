"""Builds the bell tower of the Girona cathedral as sharp new stonework.

Reference photos: "Cathedral images/Tower_closeup.JPG", "Right_side*.jpg", "Right.jpg", "Rear_left.jpg",
"Rear_right.jpg". Coordinates are those of the Meshy model (front = -Y, bell tower = +X, metres), as in
build_front.py. The tower is an eight-sided shaft whose front face and front-right corner are part of the
main front (build_front.py, part "frame"); this script continues from the stubs that part leaves.

Parts:
  shaft    the right face from the ground to the parapet with its two windows, the clock and the small
           window above it; the back-right corner and the back above the side door's porch; plinth, string
           courses, main cornice and parapet.
  belfry   the eight-sided bell storey above the roof line: pedestal, eight open arches with balustrades
           and bells, pilasters, entablature, attic with ball finials, the ribbed dome, lantern and vane.

Run: see build_common.py. Writes Girona_Tower_<part>.blend and SM_Cathedral_Tower_<part>.fbx.
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
# Shared with build_front.py (part "frame"): keep in step
# ---------------------------------------------------------------------------------------------
WALL_Y = -42.2
CORNER = (16.05, WALL_Y + 3.05)         # where the front's 45 degree corner meets the tower's right face
STUB = 0.03                             # the front's mouldings end this far round that corner
TOP_Z = 32.95                           # top of the parapet
CORNICE_Z, CORNICE_S = 31.0, 1.3
PLINTH_H, PLINTH_OUT = 2.75, 0.22
STRING = [[(0.0, 0.0), (0.05, 0.03), (0.07, 0.07)], [(0.07, 0.07), (0.07, 0.13)], [(0.07, 0.13), (0.03, 0.16), (0.0, 0.16)]]
PLINTH_TOP = [[(PLINTH_OUT, 0.0), (PLINTH_OUT, 0.06)], [(PLINTH_OUT, 0.06), (PLINTH_OUT - 0.03, 0.10), (0.11, 0.2), (0.09, 0.26)],
              [(0.09, 0.26), (0.09, 0.30), (0.0, 0.34)]]

# ---------------------------------------------------------------------------------------------
# Measured on the template
# ---------------------------------------------------------------------------------------------
C_LOW, A_LOW = (11.184, -37.134), 4.866         # the shaft: centre and apothem of its octagon
C_TOP, A_TOP = (11.184, -36.9), 4.6             # the bell storey, a little smaller
PORCH_TOP = 17.2                                # the side door's porch hides the back-right corner below this
NAVE_X = 10.85                                  # face of the nave's upper wall, where the tower's back ends
R2 = math.sqrt(0.5)


def top_quad(B, x0, x1, z, depth=0.6):
    B.quad((x0, 0.0, z), (x1, 0.0, z), (x1, depth, z), (x0, depth, z), "Ashlar", want=(0, 0, 1))


def build_shaft(B):
    hw = A_LOW * gk.T8
    k = PLINTH_OUT * gk.T8
    # --- right face: windows, clock, wall, plinth ------------------------------------------------
    B.push(gk.face_matrix(C_LOW, A_LOW, 0.0))
    holes = [gk.glazed_window(B, -0.6, 0.6, 4.9, 8.3, "Limestone", "Glass", "Iron"),
             gk.glazed_window(B, -0.6, 0.6, 15.2, 19.4, "Limestone", "Glass", "Iron"),
             gk.clock_face(B, 0.0, 24.2, 1.35, "Limestone", "Marble", "Iron"),
             gk.glazed_window(B, -0.42, 0.42, 27.0, 28.1, "Limestone", "Glass", "Iron", frame_w=0.14, pane=(0.28, 0.4))]
    for i in range(11):                               # iron rail across the foot of the upper window
        x = -0.55 + i * 0.11
        B.box(x - 0.011, x + 0.011, 0.08, 0.10, 15.2, 16.1, "Iron", faces="xXy")
    for zr in (15.26, 16.08):
        B.box(-0.6, 0.6, 0.07, 0.11, zr, zr + 0.04, "Iron", faces="yzZ")
    gk.block_wall(B, -hw, hw, -0.1, TOP_Z, "Ashlar", holes=holes, seed=141)
    top_quad(B, -hw, hw, TOP_Z)
    gk.block_wall(B, -hw - k, hw + k, -0.1, PLINTH_H, "Ashlar", y=-PLINTH_OUT, course=0.46, length=(0.8, 1.3), seed=142)
    B.pop()
    # corner strips against the front's 45 degree corner: on the wall and on the base
    a, na, b, nb = (-R2, -R2), (R2, -R2), (0.0, 1.0), (1.0, 0.0)
    gk.corner_post(B, "Ashlar", CORNER, a, na, b, nb, PLINTH_H, TOP_Z)
    gk.corner_post(B, "Ashlar", (CORNER[0] + PLINTH_OUT, CORNER[1] - k), a, na, b, nb, -0.1, PLINTH_H, leg=0.2, proud=0.025)
    # mouldings carried on from the front: top of the base and the two string courses
    y0, y1 = CORNER[1] + STUB, C_LOW[1] + hw
    B.sweep([(CORNER[0], y0, PLINTH_H), (CORNER[0], y1, PLINTH_H)], PLINTH_TOP, "Limestone")
    for zs in (10.0, 12.95):
        B.sweep([(CORNER[0], y0, zs), (CORNER[0], y1, zs)], STRING, "Limestone")

    # --- above the porch: the back-right corner and the back, as far as the nave wall ---------------
    B.push(gk.face_matrix(C_LOW, A_LOW, 45.0))
    gk.block_wall(B, -hw, hw, PORCH_TOP, TOP_Z, "Ashlar", seed=143)
    top_quad(B, -hw, hw, TOP_Z)
    B.pop()
    back_end = C_LOW[0] - NAVE_X + 0.05               # local x of the nave wall on the back face (it runs towards -x)
    B.push(gk.face_matrix(C_LOW, A_LOW, 90.0))
    gk.block_wall(B, -hw, back_end, PORCH_TOP, TOP_Z, "Ashlar", seed=144)
    top_quad(B, -hw, back_end, TOP_Z)
    B.pop()
    for theta in (0.0, 45.0):
        gk.octagon_post(B, "Ashlar", C_LOW, A_LOW, theta, PORCH_TOP, TOP_Z)

    # --- main cornice, on from the front's stub round to the nave wall ------------------------------
    s = CORNICE_S
    prof = [[(u * s, v * s) for u, v in run_] for run_ in gk.entablature_profile()]
    prof.append([(gk.CORNICE_OUT * s, gk.ENTABLATURE_H * s), (0.0, gk.ENTABLATURE_H * s)])
    p1, p2 = gk.polygon_corner(C_LOW, A_LOW, 0.0), gk.polygon_corner(C_LOW, A_LOW, 45.0)
    path = [(CORNER[0], y0), p1, p2, (NAVE_X - 0.05, p2[1])]
    B.sweep([(x, y, CORNICE_Z) for x, y in path], prof, "Limestone")
    return {"top_z": TOP_Z, "right face y": (round(CORNER[1], 3), round(y1, 3)), "back y": round(p2[1], 3)}


# ---------------------------------------------------------------------------------------------
# Part "belfry"
# ---------------------------------------------------------------------------------------------
DRUM0, SILL = 32.2, 35.85               # foot of the pedestal (behind the parapet) and the floor of the bell storey
OPEN_W, SPRING = 1.5, 40.55             # the arches: width and springing height
WALL_T = 1.1
Z_ENT, ENT_S = 41.95, 1.35              # entablature over the arches
ATTIC_TOP = 43.95
DOME_H, DOME_R0, DOME_R1 = 4.3, 4.25, 0.72
SILL_MOULD = [[(0.0, 0.0), (0.0, 0.05)],
              [(0.0, 0.05)] + [(0.10 * (1 - math.cos(a)), 0.05 + 0.10 * math.sin(a)) for a in gk.QUARTER],
              [(0.10, 0.15), (0.16, 0.15), (0.16, 0.24)], [(0.16, 0.24), (0.19, 0.27), (0.19, 0.33), (0.0, 0.33)]]
COPING = [[(0.0, 0.0), (0.06, 0.03), (0.10, 0.10)], [(0.10, 0.10), (0.10, 0.19)], [(0.10, 0.19), (0.14, 0.19), (0.14, 0.27), (0.0, 0.27)]]


def build_belfry(B):
    hb = A_TOP * gk.T8
    hi = (A_TOP - WALL_T) * gk.T8
    ent_top = Z_ENT + gk.ENTABLATURE_H * ENT_S
    z_ceil = Z_ENT + 0.35
    thetas = [-90.0 + 45.0 * k for k in range(8)]
    for k, theta in enumerate(thetas):
        B.push(gk.face_matrix(C_TOP, A_TOP, theta))
        gk.block_wall(B, -hb, hb, DRUM0, SILL, "Ashlar", seed=200 + k)                       # pedestal
        hole = gk.arch_through(B, -OPEN_W / 2, OPEN_W / 2, SILL, SPRING, WALL_T, "Limestone", frame_w=0.2, proud=0.06)
        gk.block_wall(B, -hb, hb, SILL, Z_ENT, "Ashlar", holes=[hole], seed=210 + k)         # the bell storey
        gk.pierced_panel(B, -hi, hi, SILL, z_ceil, (-OPEN_W / 2, OPEN_W / 2, SILL, SPRING), "Ashlar", WALL_T)
        for sx in (-1, 1):                                                                   # pilasters beside the arch
            px = sx * (OPEN_W / 2 + 0.2 + 0.36)
            B.box(px - 0.24, px + 0.24, -0.08, 0.0, SILL, Z_ENT, "Limestone", faces="xXy")
            B.box(px - 0.29, px + 0.29, -0.11, 0.0, SILL, SILL + 0.42, "Limestone", faces="xXyZ")           # its base block
            gk.course(B, "Limestone", [(px - 0.30, Z_ENT - 0.16), (px + 0.30, Z_ENT - 0.16)], s=0.5)        # its cap
        # impost at the springing, between the pilasters and the arch
        for sx in (-1, 1):
            xa, xb = sorted((sx * (OPEN_W / 2 + 0.2), sx * (OPEN_W / 2 + 0.2 + 0.12)))
            B.box(xa, xb, -0.05, 0.0, SPRING - 0.1, SPRING + 0.06, "Limestone", faces="xXyzZ")
        gk.balustrade(B, "Limestone", (-OPEN_W / 2, 0.3), (OPEN_W / 2, 0.3), SILL, h=0.95, posts=(False, False))
        n = int(2 * hb / (0.085 * ENT_S))                                                    # dentils under the cornice
        for i in range(n):
            xc = -hb + (i + 0.5) * 2 * hb / n
            B.box(xc - 0.026 * ENT_S, xc + 0.026 * ENT_S, -0.105 * ENT_S, -0.05 * ENT_S, Z_ENT + 0.605 * ENT_S, Z_ENT + 0.685 * ENT_S,
                  "Limestone", faces="xXyz")
        gk.block_wall(B, -hb, hb, ent_top, ATTIC_TOP, "Ashlar", seed=220 + k)                # attic
        if k % 2 == 0:                                                                       # a bell in every second arch
            B.box(-OPEN_W / 2 - 0.05, OPEN_W / 2 + 0.05, WALL_T * 0.42, WALL_T * 0.42 + 0.2, SPRING + 0.12, SPRING + 0.34, "Timber", faces="yYzZ")
            B.push(gk.translate(0.0, WALL_T * 0.42 + 0.1, SPRING + 0.12))
            gk.bell(B, "Bronze", "Iron", r=0.52 if k % 4 == 0 else 0.44, h=0.9 if k % 4 == 0 else 0.76)
            B.pop()
        B.pop()
        gk.octagon_post(B, "Ashlar", C_TOP, A_TOP, theta, DRUM0, Z_ENT)
        gk.octagon_post(B, "Ashlar", C_TOP, A_TOP, theta, ent_top, ATTIC_TOP)
    # mouldings that run round all eight sides
    B.sweep(gk.ngon_ring(C_TOP, A_TOP, SILL - 0.33), SILL_MOULD, "Limestone", closed=True)
    prof = [[(u * ENT_S, v * ENT_S) for u, v in run_] for run_ in gk.entablature_profile()]
    prof.append([(gk.CORNICE_OUT * ENT_S, gk.ENTABLATURE_H * ENT_S), (0.0, gk.ENTABLATURE_H * ENT_S)])
    B.sweep(gk.ngon_ring(C_TOP, A_TOP, Z_ENT), prof, "Limestone", closed=True)
    B.sweep(gk.ngon_ring(C_TOP, A_TOP, ATTIC_TOP - 0.27), COPING, "Limestone", closed=True)
    # floor and ceiling of the bell chamber, the top of the attic
    B.fan(gk.ngon_ring(C_TOP, A_TOP - WALL_T + 0.02, SILL - 0.01), "Paving", (0, 0, 1))
    B.fan(gk.ngon_ring(C_TOP, A_TOP - WALL_T + 0.02, z_ceil), "Ashlar", (0, 0, -1), var=0.3)
    B.fan(gk.ngon_ring(C_TOP, A_TOP, ATTIC_TOP), "Limestone", (0, 0, 1))
    for theta in thetas:                                                                     # ball finials on the corners
        cx, cy = gk.polygon_corner(C_TOP, A_TOP - 0.3, theta)
        B.push(gk.translate(cx, cy, ATTIC_TOP))
        B.lathe([[(0.2, 0.0), (0.2, 0.08), (0.13, 0.12), (0.11, 0.30), (0.15, 0.34), (0.15, 0.38)]], "Limestone", segs=16)
        B.push(gk.translate(0.0, 0.0, 0.38))
        gk.ball(B, "Limestone", 0.21)
        B.pop()
        B.pop()

    # --- dome of stone courses with eight ribs --------------------------------------------------
    B.push(gk.translate(C_TOP[0], C_TOP[1], ATTIC_TOP))
    B.lathe([[(DOME_R0 + 0.12, 0.0), (DOME_R0 + 0.12, 0.12), (DOME_R0, 0.2)]], "Limestone", segs=72)      # its foot ring
    B.push(gk.translate(0.0, 0.0, 0.2))
    rad = gk.stepped_dome(B, "Ashlar", DOME_R0, DOME_R1, DOME_H, courses=16, bulge=0.8)
    for theta in thetas:
        a = math.radians(theta + 22.5)
        pts = [((rad(t) + 0.04) * math.cos(a), (rad(t) + 0.04) * math.sin(a), DOME_H * t) for t in np.linspace(0.0, 1.0, 19)]
        B.tube(pts, 0.13, "Limestone", sides=8, flat=0.8)
    # lantern: eight-sided drum with a moulded foot and cornice, a small dome, ball, rod and wind vane
    B.push(gk.translate(0.0, 0.0, DOME_H))
    B.lathe([[(DOME_R1 + 0.16, -0.05), (DOME_R1 + 0.16, 0.08), (DOME_R1 + 0.04, 0.16)]], "Limestone", segs=32)
    B.lathe([[(0.62, 0.16), (0.62, 1.5)]], "Ashlar", segs=8, a0=22.5, a1=382.5, smooth=False)
    for i in range(8):                                    # a narrow blind arch on every side of the drum
        B.push(gk.rot_z(45.0 * i) @ gk.translate(0.62 * math.cos(math.radians(22.5)), 0.0, 0.0) @ gk.rot_z(90.0))
        gk.opening(B, -0.12, 0.12, 0.4, 1.12, "Limestone", "Iron", recess=0.06, frame_w=0.05, proud=0.02)
        B.pop()
    B.lathe([[(0.62, 1.5), (0.70, 1.54), (0.78, 1.62), (0.78, 1.69), (0.66, 1.72)]], "Limestone", segs=32)
    B.push(gk.translate(0.0, 0.0, 1.72))
    gk.stepped_dome(B, "Ashlar", 0.66, 0.08, 0.74, courses=6, ledge=0.015, segs=32)
    B.push(gk.translate(0.0, 0.0, 0.74))
    gk.ball(B, "Limestone", 0.15)
    B.tube([(0.0, 0.0, 0.28), (0.0, 0.0, 2.2)], 0.022, "Iron", sides=8)
    for ang in (0.0, 90.0):                               # the four arms of the vane's cross
        e = np.array([math.cos(math.radians(ang)), math.sin(math.radians(ang)), 0.0])
        B.tube([tuple(-e * 0.42 + (0, 0, 1.0)), tuple(e * 0.42 + (0, 0, 1.0))], 0.014, "Iron", sides=6)
    B.push(gk.translate(0.0, 0.0, 1.45) @ gk.rot_z(35.0))  # the vane: an arrow plate that would turn with the wind
    B.box(-0.6, 0.45, -0.008, 0.008, -0.03, 0.03, "Iron")
    B.add(np.array([(0.45, 0.0, -0.14), (0.78, 0.0, 0.0), (0.45, 0.0, 0.14)]), np.array([(0, 1, 2), (2, 1, 0)]), "Iron")
    B.add(np.array([(-0.6, 0.0, 0.0), (-0.88, 0.0, 0.2), (-0.74, 0.0, 0.0), (-0.88, 0.0, -0.2)]), np.array([(0, 1, 2), (0, 2, 3), (2, 1, 0), (3, 2, 0)]), "Iron")
    B.pop()
    B.push(gk.translate(0.0, 0.0, 2.2))
    gk.ball(B, "Iron", 0.06)
    B.pop()
    B.pop()
    B.pop()
    B.pop()
    B.pop()
    B.pop()
    return {"sill": SILL, "entablature": (Z_ENT, round(ent_top, 3)), "attic top": ATTIC_TOP, "dome top": round(ATTIC_TOP + 0.2 + DOME_H, 2),
            "vane top": round(ATTIC_TOP + 0.2 + DOME_H + 1.72 + 0.74 + 2.2, 2)}


# ---------------------------------------------------------------------------------------------
PARTS = {"shaft": build_shaft, "belfry": build_belfry}
AUDITS = {
    "shaft": [((CORNER[0], 0.0, 0.0), (0.0, 1.0, 0.0), (CORNER[1] + 0.05, C_LOW[1] + A_LOW * gk.T8 - 0.05, 0.05, 30.9), 0.03,
               [("lane", (27.0, -44.0, 1.7)), ("street", (24.0, -37.0, 1.7)), ("far", (40.0, -30.0, 3.0))])],
}
SHOTS = {
    "shaft": [("side", (46.0, -44.0, 1.7), (16.0, -37.0, 17.0), 24, 1000, 1500),
              ("clock", (30.0, -42.0, 16.0), (16.05, -37.13, 25.0), 55, 1200, 1300),
              ("window", (23.0, -40.5, 14.0), (16.05, -37.13, 17.4), 45, 1100, 1300),
              ("base", (22.0, -43.5, 1.7), (16.05, -37.6, 5.5), 26, 1200, 1300),
              ("crown", (24.0, -46.0, 25.0), (15.0, -36.0, 31.5), 40, 1400, 1000)],
    "belfry": [("wide", (44.0, -66.0, 14.0), (11.2, -37.0, 41.0), 60, 1100, 1400),
               ("arches", (24.0, -52.0, 32.0), (11.2, -37.0, 39.5), 45, 1300, 1200),
               ("dome", (27.0, -53.0, 52.0), (11.2, -37.0, 47.5), 50, 1300, 1100),
               ("under", (19.0, -46.0, 30.0), (12.5, -39.0, 40.5), 30, 1300, 1100)],
}
build_common.run("Tower", PARTS, AUDITS, SHOTS)
