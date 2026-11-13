"""Builds the main (west) front of the Girona cathedral as sharp new stonework.

Reference photos: "Cathedral images/Front_*.jpg", "front_closeup_2.jpg", "Door_somewhere_2.jpg".
The template is the Meshy model in /Game/Buildings/Cathedral/Version_0_2: everything here is in
that model's Blender coordinates (front = -Y, bell tower = +X, metres), so the result lines up
when placed with the same transform. Level position = (21180 + 100*y, 3430 + 100*x, 1300 + 100*z).

Run (Blender 5.2, no UI):
  blender -b --factory-startup --python build_front.py -- --out <folder> [--part doorbay]
      [--audit] [--renders <folder>] [--template <blend with the Meshy model>]
--audit casts sight lines from where a player can stand and must report 0 that see through.

Parts:
  doorbay   the ground tier around the main door: wall, door, four pairs of columns on pedestals,
            two shell niches and the entablature (the stage-0 sample).
  centre    the rest of the centre panel: second and third storey, round window with its pediment,
            the arcade under the cornice, the side windows with balconies and the wall around it all.
  frame     the flanks, the chamfered corners, the tall base, the string courses and the main cornice.
  stairs    the flight of steps with its stepped side walls, the landing and its balustrades.

Writes <out>/Girona_Front_<part>.blend and <out>/SM_Cathedral_Front_<part>.fbx.
"""
import argparse
import math
import os
import sys

import bpy
import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gothic_kit as gk  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--part", default="doorbay")
ap.add_argument("--renders")
ap.add_argument("--template")
ap.add_argument("--audit", action="store_true", help="look for places where one can see through the stonework")
args = ap.parse_args(argv)

# ---------------------------------------------------------------------------------------------
# Measured on the template
# ---------------------------------------------------------------------------------------------
WALL_Y = -42.2          # face of the front wall
LANDING_Z = 4.59        # top of the template's landing in front of the door
DOOR_W, DOOR_H = 2.72, 3.25

# ---------------------------------------------------------------------------------------------
# Ground tier layout (x along the wall, centred on the door)
# ---------------------------------------------------------------------------------------------
BAY_X = 5.9                             # the sample covers -BAY_X .. BAY_X
FLOOR_Z = LANDING_Z + 0.035             # top of the new paving
PAIRS = (-4.62, -2.42, 2.42, 4.62)      # centres of the column pairs
PAIR_GAP = 0.265                        # each column stands this far from its pair centre
COL_OUT = 0.42                          # column axis in front of the wall
COL_R0, COL_R1 = 0.18, 0.153
PED_W, PED_D, PED_H = 1.04, 0.68, 0.95
SHAFT_H, CAP_H = 2.7, 0.44
NICHES = (-3.52, 3.52)
NICHE_R = 0.44
NICHE_Z0 = FLOOR_Z + PED_H              # niche floor level with the pedestal tops
NICHE_SPRING = NICHE_Z0 + 1.72
FRAME_W = 0.26                          # marble door frame
REVEAL = 0.34                           # the door leaves sit this deep in the wall
RESSAUT_HALF, RESSAUT_OUT, ENT_OUT = 0.46, 0.6, 0.1

MATERIALS = ["Ashlar", "Limestone", "Marble", "Paving", "Bronze", "BronzeRust", "Timber", "Glass", "Iron"]
COLORS = {"Ashlar": (0.72, 0.69, 0.62), "Limestone": (0.78, 0.76, 0.70), "Marble": (0.82, 0.82, 0.80),
          "Paving": (0.42, 0.42, 0.41), "Bronze": (0.23, 0.36, 0.30), "BronzeRust": (0.36, 0.22, 0.14),
          "Timber": (0.30, 0.20, 0.12), "Glass": (0.05, 0.07, 0.12), "Iron": (0.08, 0.08, 0.09)}


def build_doorbay(B):
    B.push(gk.translate(0.0, WALL_Y, 0.0))          # from here on y = 0 is the wall face
    base_h = None
    col_top = None

    # --- column pairs on pedestals ------------------------------------------------------------
    for pc in PAIRS:
        B.push(gk.translate(pc, 0.0, FLOOR_Z))
        gk.pedestal(B, "Limestone", PED_W, PED_D, PED_H)
        B.pop()
        for sx in (-1, 1):
            B.push(gk.translate(pc + sx * PAIR_GAP, -COL_OUT, FLOOR_Z + PED_H))
            base_h = gk.attic_base(B, "Limestone", r=COL_R0, plinth=0.5)
            B.push(gk.translate(0, 0, base_h))
            gk.column_shaft(B, "Limestone", COL_R0, COL_R1, SHAFT_H)
            B.push(gk.translate(0, 0, SHAFT_H))
            gk.corinthian_capital(B, "Limestone", r=COL_R1, height=CAP_H, abacus=0.5)
            B.pop()
            B.pop()
            B.pop()
    col_top = FLOOR_Z + PED_H + base_h + SHAFT_H + CAP_H
    ent_z = col_top
    top_z = ent_z + gk.ENTABLATURE_H

    # --- entablature breaking forward over every pair -----------------------------------------
    path = [(-BAY_X, -ENT_OUT)]
    for pc in PAIRS:
        path += [(pc - RESSAUT_HALF, -ENT_OUT), (pc - RESSAUT_HALF, -RESSAUT_OUT), (pc + RESSAUT_HALF, -RESSAUT_OUT), (pc + RESSAUT_HALF, -ENT_OUT)]
    path += [(BAY_X, -ENT_OUT)]
    gk.entablature(B, "Limestone", path, ent_z)
    for pc in PAIRS:                                  # a flower on the frieze above every column
        for sx in (-1, 1):
            B.push(gk.translate(pc + sx * PAIR_GAP, -RESSAUT_OUT, ent_z + 0.41) @ gk.FACE_OUT)
            gk.rosette(B, "Limestone", 0.085, 0.04, petals=8)
            B.pop()

    # --- shell niches -------------------------------------------------------------------------
    holes = []
    for nc in NICHES:
        B.push(gk.translate(nc, 0.0, 0.0))
        gk.shell_niche(B, "Limestone", NICHE_R, NICHE_Z0, NICHE_SPRING)
        B.push(gk.translate(0.0, 0.03, NICHE_Z0))
        gk.statue_plinth(B, "Limestone", 0.56, 0.2, 0.3)
        B.pop()
        B.pop()
        holes.append((nc - NICHE_R, nc + NICHE_R, NICHE_Z0, NICHE_SPRING + NICHE_R))

    # --- door: opening, marble frame, reveals, leaves -----------------------------------------
    hw = DOOR_W / 2
    z0, z1 = FLOOR_Z, FLOOR_Z + DOOR_H
    holes.append((-hw, hw, LANDING_Z, z1))
    frame = [(-hw, 0.0, z0), (-hw, 0.0, z1), (hw, 0.0, z1), (hw, 0.0, z0)]
    B.sweep(frame, gk.frame_profile(FRAME_W, 0.085), "Marble", b=(0, -1, 0), side=-1)
    B.quad((-hw, 0.0, z0), (-hw, REVEAL, z0), (-hw, REVEAL, z1), (-hw, 0.0, z1), "Marble", want=(1, 0, 0))       # reveals
    B.quad((hw, 0.0, z0), (hw, REVEAL, z0), (hw, REVEAL, z1), (hw, 0.0, z1), "Marble", want=(-1, 0, 0))
    B.quad((-hw, 0.0, z1), (hw, 0.0, z1), (hw, REVEAL, z1), (-hw, REVEAL, z1), "Marble", want=(0, 0, -1))
    B.quad((-hw, 0.0, z0), (hw, 0.0, z0), (hw, REVEAL, z0), (-hw, REVEAL, z0), "Paving", want=(0, 0, 1))         # threshold
    # plinth blocks under the frame and a cornice shelf over it
    for sx in (-1, 1):
        xa, xb = sorted((sx * hw, sx * (hw + FRAME_W + 0.02)))
        B.box(xa, xb, -0.11, 0.0, z0, z0 + 0.34, "Marble", faces="xXyZ")
    zc = z1 + FRAME_W + 0.2
    shelf = [[(0.0, 0.0), (0.0, 0.05)],
             [(0.0, 0.05)] + [(0.05 * math.sin(a), 0.05 + 0.05 * (1 - math.cos(a))) for a in gk.QUARTER],
             [(0.05, 0.1), (0.14, 0.11), (0.14, 0.16)],
             [(0.14, 0.16)] + [(0.14 + 0.05 * i / 5, 0.16 + 0.05 * (0.5 - 0.5 * math.cos(math.pi * i / 5))) for i in range(1, 6)],
             [(0.19, 0.21), (0.2, 0.21), (0.2, 0.235), (0.0, 0.235)]]
    sw = hw + FRAME_W + 0.03
    B.sweep([(-sw, 0.0, zc), (-sw, -0.06, zc), (sw, -0.06, zc), (sw, 0.0, zc)], shelf, "Marble")
    B.quad((-sw, -0.06, zc), (sw, -0.06, zc), (sw, 0.0, zc), (-sw, 0.0, zc), "Marble", want=(0, 0, -1))
    B.box(-hw - FRAME_W, hw + FRAME_W, -0.035, 0.0, z1 + FRAME_W, zc, "Marble", faces="xXy")                    # frieze panel
    leaf_w = hw
    for sx in (-1, 1):
        B.push(gk.translate(-hw if sx < 0 else 0.0, REVEAL, z0))
        gk.bronze_leaf(B, leaf_w, DOOR_H, "Bronze", "BronzeRust", seed=5 + sx)
        B.pop()
    B.box(-0.03, 0.03, REVEAL - 0.03, REVEAL, z0, z1, "Bronze", faces="xXy")                             # meeting stile
    for i in range(18):
        B.push(gk.translate(0.0, REVEAL - 0.03, z0 + 0.1 + i * (DOOR_H - 0.2) / 17) @ gk.FACE_OUT)
        gk.stud(B, "BronzeRust", 0.026, 0.016)
        B.pop()

    # --- the wall of cut blocks, a base course and the paving ----------------------------------
    gk.block_wall(B, -BAY_X - 0.07, BAY_X + 0.07, LANDING_Z, top_z + 0.06, "Ashlar", holes=holes, seed=11)   # laps over the cut in the template
    skirt = [[(0.03, 0.0), (0.03, 0.2)], [(0.03, 0.2)] + [(0.03 * (1 - math.sin(a)), 0.2 + 0.03 * (1 - math.cos(a))) for a in gk.QUARTER]]
    edges = [-BAY_X] + [e for pc in PAIRS for e in (pc - PED_W / 2 - 0.055, pc + PED_W / 2 + 0.055)] + [BAY_X]
    edges[4:6] = [edges[4], -hw - FRAME_W - 0.02, hw + FRAME_W + 0.02, edges[5]]      # break the base course at the door
    for xa, xb in zip(edges[0::2], edges[1::2]):
        B.sweep([(xa, 0.0, FLOOR_Z), (xb, 0.0, FLOOR_Z)], skirt, "Ashlar")
    gk.slab_floor(B, -BAY_X, BAY_X, -1.6, 0.0, FLOOR_Z, "Paving", seed=21)
    B.quad((-BAY_X, -1.6, LANDING_Z - 0.02), (BAY_X, -1.6, LANDING_Z - 0.02), (BAY_X, -1.6, FLOOR_Z - 0.005), (-BAY_X, -1.6, FLOOR_Z - 0.005),
           "Paving", want=(0, -1, 0))
    B.pop()
    return {"top_z": top_z, "ent_z": ent_z}


# ---------------------------------------------------------------------------------------------
# Part "centre": the centre panel of the front above and beside the door bay
# ---------------------------------------------------------------------------------------------
BASE_H = 0.253                          # height of gk.attic_base
T1_TOP = FLOOR_Z + PED_H + BASE_H + SHAFT_H + CAP_H + gk.ENTABLATURE_H      # top of the door bay's cornice
PANEL_X = 9.95                          # the centre panel spans -PANEL_X .. PANEL_X
FLANK_BACK = 0.15                       # the flanks of the front stand this far behind the centre panel
CENTRE_TOP = 30.95                      # the part ends under the upper string course
ROSE_Z, ROSE_R, ROSE_W = 21.6, 2.55, 1.4
ARCADE = [1.6 * k for k in range(-3, 4)]            # centres of the seven arches under the cornice
SIDE_WIN_X = 7.29
SKIRT = [[(0.03, 0.0), (0.03, 0.2)], [(0.03, 0.2)] + [(0.03 * (1 - math.sin(a)), 0.2 + 0.03 * (1 - math.cos(a))) for a in gk.QUARTER]]


def storey(B, z0, ped_h, shaft_h, niches, ent_s=0.85, r0=0.15, r1=0.128, cap_h=0.37, abacus=0.42, gap=0.235, out=0.38):
    """One storey of the frontispiece above the door bay: pedestals, column pairs, shell niches and an
    entablature that breaks forward over each pair. niches: (x, radius, floor z, spring z).
    Returns (holes to leave in the wall, top z)."""
    ped_w, ped_d = 2 * gap + abacus + 0.07, out + abacus / 2 + 0.03
    for pc in PAIRS:
        B.push(gk.translate(pc, 0.0, z0))
        gk.pedestal(B, "Limestone", ped_w, ped_d, ped_h, base_h=0.12, cap_h=0.1)
        B.pop()
        for sx in (-1, 1):
            B.push(gk.translate(pc + sx * gap, -out, z0 + ped_h))
            gk.attic_base(B, "Limestone", r=r0, plinth=abacus)
            B.push(gk.translate(0, 0, BASE_H))
            gk.column_shaft(B, "Limestone", r0, r1, shaft_h)
            B.push(gk.translate(0, 0, shaft_h))
            gk.corinthian_capital(B, "Limestone", r=r1, height=cap_h, abacus=abacus)
            B.pop()
            B.pop()
            B.pop()
    ent_z = z0 + ped_h + BASE_H + shaft_h + cap_h
    half, r_out = gap + abacus / 2 - 0.045, out + r1 + 0.025
    path = [(-BAY_X, -ENT_OUT)]
    for pc in PAIRS:
        path += [(pc - half, -ENT_OUT), (pc - half, -r_out), (pc + half, -r_out), (pc + half, -ENT_OUT)]
    path += [(BAY_X, -ENT_OUT)]
    B.push(gk.translate(0, 0, ent_z) @ np.diag([1.0, ent_s, ent_s, 1.0]))       # a lighter entablature than the door bay's
    gk.entablature(B, "Limestone", [(x, y / ent_s) for x, y in path], 0.0)
    B.pop()
    holes = []
    for x, r, zf, zs in niches:
        B.push(gk.translate(x, 0.0, 0.0))
        gk.shell_niche(B, "Limestone", r, zf, zs, ribs=9 if r < 0.45 else 11)
        B.push(gk.translate(0.0, 0.03, zf))
        gk.statue_plinth(B, "Limestone", r * 1.25, 0.16, r * 0.65)
        B.pop()
        B.pop()
        holes.append((x - r, x + r, zf, zs + r))
    return holes, ent_z + gk.ENTABLATURE_H * ent_s


def build_centre(B):
    B.push(gk.translate(0.0, WALL_Y, 0.0))          # from here on y = 0 is the wall face
    holes = [(-BAY_X - 0.07, BAY_X + 0.07, LANDING_Z - 0.1, T1_TOP + 0.06)]     # the door bay part has its own wall here

    # --- second and third storey of the frontispiece ------------------------------------------
    z2 = T1_TOP
    h2, t2 = storey(B, z2, 0.5, 1.95, [(-3.52, 0.40, z2 + 0.5, z2 + 1.92), (3.52, 0.40, z2 + 0.5, z2 + 1.92), (0.0, 0.52, z2 + 0.5, z2 + 1.96)])
    apex2 = t2 + 0.62
    gk.course(B, "Limestone", [(-2.0, t2), (0.0, apex2), (2.0, t2)], s=0.8)      # pediment over the centre niche
    h3, t3 = storey(B, t2, 0.45, 2.0, [(-3.52, 0.40, t2 + 0.6, t2 + 2.06), (3.52, 0.40, t2 + 0.6, t2 + 2.06), (0.0, 0.5, apex2 + 0.35, apex2 + 1.4)])
    holes += h2 + h3
    for pc in PAIRS:                                  # urns over the outer pairs, balls over the inner ones
        B.push(gk.translate(pc, -0.38, t3))
        if abs(pc) > 3.0:
            gk.urn(B, "Limestone", 0.95)
        else:
            B.box(-0.16, 0.16, -0.16, 0.16, 0.0, 0.22, "Limestone", faces="xXyYZ")
            B.push(gk.translate(0, 0, 0.22))
            gk.ball(B, "Limestone", 0.2)
            B.pop()
        B.pop()

    # --- round window with its scrolls and pediment -------------------------------------------
    holes.append(gk.rose_window(B, 0.0, ROSE_Z, ROSE_R, ROSE_W, "Limestone", "Glass", "Iron"))
    for sx in (-1, 1):
        gk.scroll(B, "Limestone", (sx * 5.05, 19.55), 0.78, (sx * 4.62, 22.5), 0.42, sx=sx, y=-0.16, width=0.32, thick=0.17)
    zb, apex, half = ROSE_Z + ROSE_R + ROSE_W + 0.1, ROSE_Z + ROSE_R + ROSE_W + 2.2, 4.5
    pts = [(-half - 0.3, zb + 0.2), (-half, zb)]
    pts += [(x, zb + (apex - zb) * (1 - abs(x) / half) ** 0.85) for x in np.linspace(-half, half, 13)[1:-1]]
    pts += [(half, zb), (half + 0.3, zb + 0.2)]
    gk.course(B, "Limestone", pts, s=1.25)
    B.push(gk.translate(0.0, -0.04, zb + 0.95) @ gk.FACE_OUT)      # cartouche in the gable
    gk.rosette(B, "Limestone", 0.42, 0.12, petals=10)
    B.pop()
    B.push(gk.translate(0.0, -0.2, apex + 0.22))
    gk.urn(B, "Limestone", 0.8)
    B.pop()

    # --- arcade of seven arches under the cornice ---------------------------------------------
    az0, azs = 28.25, 29.5
    for c in ARCADE:
        holes.append(gk.opening(B, c - 0.525, c + 0.525, az0, azs, "Limestone", "Timber", recess=0.35, frame_w=0.13, proud=0.045))
        for zz in np.arange(az0 + 0.08, azs + 0.4, 0.13):           # louvre slats
            B.box(c - 0.5, c + 0.5, 0.29, 0.35, zz, zz + 0.04, "Timber", faces="yzZ")
    gk.course(B, "Limestone", [(-BAY_X, az0 - 0.2), (BAY_X, az0 - 0.2)], s=0.55)

    # --- side windows with balconies, on a tall panel -----------------------------------------
    for sx in (-1, 1):
        cx = sx * SIDE_WIN_X
        wz0, wz1 = 14.6, 16.7
        holes.append(gk.opening(B, cx - 0.45, cx + 0.45, wz0, wz1, "Limestone", "Timber", arched=False, recess=0.3, frame_w=0.17, proud=0.06))
        for lx in (-0.225, 0.225):                                  # shutters: rails on two leaves
            for zr in (wz0 + 0.05, wz0 + 0.95, wz1 - 0.13):
                B.box(cx + lx - 0.2, cx + lx + 0.2, 0.27, 0.3, zr, zr + 0.08, "Timber", faces="xXyzZ")
        B.box(cx - 0.025, cx + 0.025, 0.26, 0.3, wz0, wz1, "Timber", faces="xXy")
        B.box(cx - 0.68, cx + 0.68, -0.07, 0.0, wz1 + 0.17, wz1 + 0.42, "Limestone", faces="xXyzZ")
        gk.course(B, "Limestone", [(cx - 0.85, wz1 + 0.42), (cx, wz1 + 0.95), (cx + 0.85, wz1 + 0.42)], s=0.6)
        B.push(gk.translate(cx, -0.12, wz1 + 1.15))
        gk.obelisk(B, "Limestone", 2.3, half=0.17)
        B.pop()
        bz, bx0, bx1 = wz0 - 0.2, cx - 1.4, cx + 1.4                 # balcony: slab on stepped corbels
        B.box(bx0, bx1, -0.62, 0.0, bz - 0.14, bz, "Limestone", faces="xXyzZ")
        for bxk in (bx0 + 0.22, cx, bx1 - 0.22):
            B.box(bxk - 0.08, bxk + 0.08, -0.46, 0.0, bz - 0.46, bz - 0.14, "Limestone", faces="xXyz")
            B.box(bxk - 0.08, bxk + 0.08, -0.24, 0.0, bz - 0.74, bz - 0.46, "Limestone", faces="xXyz")
        gk.balustrade(B, "Limestone", (bx0 + 0.11, -0.51), (bx1 - 0.11, -0.51), bz, h=0.85)
        for bxs in (bx0 + 0.11, bx1 - 0.11):
            gk.balustrade(B, "Limestone", (bxs, -0.40), (bxs, 0.0), bz, h=0.85, posts=(False, False))
        px0, px1, pz1 = cx - 1.05, cx + 1.05, bz - 0.78              # tall panel under the balcony
        gk.block_wall(B, px0, px1, LANDING_Z, pz1, "Ashlar", y=-0.07, seed=31 + sx)
        B.box(px0, px1, -0.07, 0.0, LANDING_Z, pz1, "Ashlar", faces="xX")
        gk.course(B, "Limestone", [(px0 - 0.04, pz1), (px1 + 0.04, pz1)], s=0.5)

    # --- the wall of the centre panel, its returns to the flanks, base course and paving -------
    gk.block_wall(B, -PANEL_X, PANEL_X, LANDING_Z, CENTRE_TOP, "Ashlar", holes=holes, seed=17)
    for sx in (-1, 1):
        B.quad((sx * PANEL_X, 0.0, LANDING_Z), (sx * PANEL_X, FLANK_BACK + 0.05, LANDING_Z), (sx * PANEL_X, FLANK_BACK + 0.05, CENTRE_TOP),
               (sx * PANEL_X, 0.0, CENTRE_TOP), "Ashlar", want=(sx, 0, 0))
        xa, xb = sorted((sx * (BAY_X + 0.07), sx * PANEL_X))
        B.sweep([(xa, 0.0, FLOOR_Z), (xb, 0.0, FLOOR_Z)], SKIRT, "Ashlar")          # base course; runs on behind the tall panel
        px = sx * SIDE_WIN_X
        B.sweep([(px - 1.05, 0.0, FLOOR_Z), (px - 1.05, -0.07, FLOOR_Z), (px + 1.05, -0.07, FLOOR_Z), (px + 1.05, 0.0, FLOOR_Z)], SKIRT, "Ashlar")
        fa, fb = sorted((sx * BAY_X, sx * PANEL_X))                                  # the paving starts where the door bay's ends
        gk.slab_floor(B, fa, fb, -1.6, 0.0, FLOOR_Z, "Paving", seed=23 + sx)
        B.quad((fa, -1.6, LANDING_Z - 0.02), (fb, -1.6, LANDING_Z - 0.02), (fb, -1.6, FLOOR_Z - 0.005), (fa, -1.6, FLOOR_Z - 0.005), "Paving", want=(0, -1, 0))
        xe = sx * PANEL_X
        B.quad((xe, -1.6, LANDING_Z - 0.02), (xe, 0.0, LANDING_Z - 0.02), (xe, 0.0, FLOOR_Z - 0.005), (xe, -1.6, FLOOR_Z - 0.005), "Paving", want=(sx, 0, 0))
    B.pop()
    return {"top_z": CENTRE_TOP, "t1": round(T1_TOP, 3), "t2": round(t2, 3), "t3": round(t3, 3)}


# ---------------------------------------------------------------------------------------------
# Part "frame": the flanks of the front, its chamfered corners, base, string courses and the crown
# ---------------------------------------------------------------------------------------------
FLANK_Y = 0.2                           # the flanks stand this far behind the centre panel
X_CH = 13.2                             # the flat flank ends here and the 45 degree chamfer begins
CORNER = (16.05, 3.05)                  # (x, y) where the chamfer meets the side wall of the nave
TOP_Z = 32.95                           # top of the parapet
CORNICE_Z, CORNICE_S = 31.0, 1.3        # main cornice: its foot and its scale against gk.entablature_profile
LAND_X = 11.2                           # the landing in front of the door reaches this far to each side
STUB = 0.03                             # mouldings carry on this far round the outer corners, for the next zone to join
PLINTH_H, PLINTH_OUT = 2.75, 0.22
STRING = [[(0.0, 0.0), (0.05, 0.03), (0.07, 0.07)], [(0.07, 0.07), (0.07, 0.13)], [(0.07, 0.13), (0.03, 0.16), (0.0, 0.16)]]
PLINTH_TOP = [[(PLINTH_OUT, 0.0), (PLINTH_OUT, 0.06)], [(PLINTH_OUT, 0.06), (PLINTH_OUT - 0.03, 0.10), (0.11, 0.2), (0.09, 0.26)],
              [(0.09, 0.26), (0.09, 0.30), (0.0, 0.34)]]


def frame_faces():
    """The front's wall faces beside the centre panel as (name, matrix, x0, x1): two flat flanks and two chamfers."""
    L = math.hypot(CORNER[0] - X_CH, CORNER[1] - FLANK_Y)
    return [("left flank", gk.translate(0.0, FLANK_Y, 0.0), -X_CH, -PANEL_X), ("right flank", gk.translate(0.0, FLANK_Y, 0.0), PANEL_X, X_CH),
            ("left chamfer", gk.translate(-CORNER[0], CORNER[1], 0.0) @ gk.rot_z(-45.0), 0.0, L),
            ("right chamfer", gk.translate(X_CH, FLANK_Y, 0.0) @ gk.rot_z(45.0), 0.0, L)]


def build_frame(B):
    B.push(gk.translate(0.0, WALL_Y, 0.0))          # from here on y = 0 is the face of the centre panel
    k = PLINTH_OUT * math.tan(math.radians(22.5))    # how far a face 0.22 m proud runs on to reach the corner
    r2 = math.sqrt(0.5)
    for i, (name, m, x0, x1) in enumerate(frame_faces()):
        B.push(m)
        gk.block_wall(B, x0, x1, -0.1, TOP_Z, "Ashlar", seed=41 + i)
        B.quad((x0, 0.0, TOP_Z), (x1, 0.0, TOP_Z), (x1, 0.6, TOP_Z), (x0, 0.6, TOP_Z), "Ashlar", want=(0, 0, 1))      # top of the parapet
        # tall base of larger blocks, where the wall comes down to the ground
        if name == "left flank":
            px0, px1 = x0 - k, -LAND_X
        elif name == "right flank":
            px0, px1 = LAND_X, x1 + k
        else:
            px0, px1 = x0 - k, x1 + k
        gk.block_wall(B, px0, px1, -0.1, PLINTH_H, "Ashlar", y=-PLINTH_OUT, course=0.46, length=(0.8, 1.3), seed=51 + i)
        B.pop()
    # the centre panel above the "centre" part, and its returns
    gk.block_wall(B, -PANEL_X, PANEL_X, CENTRE_TOP, TOP_Z, "Ashlar", seed=47)
    B.quad((-PANEL_X, 0.0, TOP_Z), (PANEL_X, 0.0, TOP_Z), (PANEL_X, 0.6, TOP_Z), (-PANEL_X, 0.6, TOP_Z), "Ashlar", want=(0, 0, 1))
    for sx in (-1, 1):
        B.quad((sx * PANEL_X, 0.0, CENTRE_TOP), (sx * PANEL_X, FLANK_Y + 0.05, CENTRE_TOP), (sx * PANEL_X, FLANK_Y + 0.05, TOP_Z),
               (sx * PANEL_X, 0.0, TOP_Z), "Ashlar", want=(sx, 0, 0))
        # corner strips where flank and chamfer meet: on the wall and on the base
        a, na, b, nb = (-sx, 0.0), (0.0, -1.0), (sx * r2, r2), (sx * r2, -r2)
        gk.corner_post(B, "Ashlar", (sx * X_CH, FLANK_Y), a, na, b, nb, PLINTH_H, TOP_Z)
        gk.corner_post(B, "Ashlar", (sx * (X_CH + k), FLANK_Y - PLINTH_OUT), a, na, b, nb, -0.1, PLINTH_H, leg=0.2, proud=0.025)
        B.quad((sx * LAND_X, FLANK_Y - PLINTH_OUT, -0.1), (sx * LAND_X, FLANK_Y, -0.1), (sx * LAND_X, FLANK_Y, PLINTH_H),
               (sx * LAND_X, FLANK_Y - PLINTH_OUT, PLINTH_H), "Ashlar", want=(-sx, 0, 0))                    # end of the base at the landing
        # mouldings that run round the flank and the chamfer: top of the base and two string courses
        run = [(sx * LAND_X, FLANK_Y), (sx * X_CH, FLANK_Y), (sx * CORNER[0], CORNER[1]), (sx * CORNER[0], CORNER[1] + STUB)]
        full = [(sx * PANEL_X, FLANK_Y)] + run[1:]
        if sx < 0:
            run, full = run[::-1], full[::-1]                          # sweeps run left to right
        B.sweep([(x, y, PLINTH_H) for x, y in run], PLINTH_TOP, "Limestone")
        for zs in (10.0, 12.95):
            B.sweep([(x, y, zs) for x, y in full], STRING, "Limestone")

    # --- the crown: main cornice round the whole front, mitred at every corner ------------------
    s = CORNICE_S
    prof = [[(u * s, v * s) for u, v in run_] for run_ in gk.entablature_profile()]
    prof.append([(gk.CORNICE_OUT * s, gk.ENTABLATURE_H * s), (0.0, gk.ENTABLATURE_H * s)])     # its top, back to the wall
    path = [(-CORNER[0], CORNER[1] + STUB), (-CORNER[0], CORNER[1]), (-X_CH, FLANK_Y), (-PANEL_X, FLANK_Y), (-PANEL_X, 0.0), (PANEL_X, 0.0),
            (PANEL_X, FLANK_Y), (X_CH, FLANK_Y), CORNER, (CORNER[0], CORNER[1] + STUB)]
    B.sweep([(x, y, CORNICE_Z) for x, y in path], prof, "Limestone")
    chain = [p for run_ in prof for p in run_]
    for (xe, ye), sx in ((path[0], -1), (path[-1], 1)):               # flat ends of the cornice at the side walls
        out = np.array([float(sx), 0.0, 0.0])                          # the side wall's outward normal
        along = np.array([0.0, 1.0, 0.0])                              # along the side wall, away from the front
        base = np.array([xe, ye, CORNICE_Z])
        for (u1, v1), (u2, v2) in zip(chain, chain[1:]):
            if v2 - v1 > 1e-9:
                B.quad(base + [0, 0, v1], base + out * u1 + [0, 0, v1], base + out * u2 + [0, 0, v2], base + [0, 0, v2], "Limestone", want=along)
    B.pop()
    return {"top_z": TOP_Z}


# ---------------------------------------------------------------------------------------------
# Part "stairs": the flight up to the door, its stepped side walls, the landing and its balustrades
# ---------------------------------------------------------------------------------------------
STEPS, GOING = 30, 0.25
STAIR_X = 7.0                           # the flight spans -STAIR_X .. STAIR_X
CHEEK = 0.9                             # thickness of the side walls of the flight
LAND_Y = -4.8                           # front edge of the landing (y is measured from the wall face)
FOOT_Y = LAND_Y - STEPS * GOING         # where the first step starts
BAST_D, BAST_Z = 3.5, 2.5               # the bastions beside the foot of the flight: depth and height of their terrace
BAST_X = 10.2                           # how far they reach to each side: keeps the plaza's corner house 5 m away


def side_wall(B, x, facing, y0, y1, z0, z1, seed, mat="Ashlar"):
    """Block wall in the plane x = const between y0 and y1, facing -x (facing = -1) or +x (facing = 1)."""
    if facing < 0:
        B.push(gk.translate(x, 0.0, 0.0) @ gk.rot_z(-90.0))            # local x runs towards -y
        gk.block_wall(B, -y1, -y0, z0, z1, mat, seed=seed)
    else:
        B.push(gk.translate(x, 0.0, 0.0) @ gk.rot_z(90.0))             # local x runs towards +y
        gk.block_wall(B, y0, y1, z0, z1, mat, seed=seed)
    B.pop()


def build_stairs(B):
    B.push(gk.translate(0.0, WALL_Y, 0.0))
    rise = FLOOR_Z / STEPS
    rng = np.random.default_rng(5)
    # --- the steps: stone slabs with a small nosing, each slab its own tone ---------------------
    for i in range(STEPS):
        front, top = FOOT_Y + i * GOING, (i + 1) * rise
        cuts = [-STAIR_X - 0.012]                                      # the ends run in behind the side walls
        while cuts[-1] < STAIR_X - 2.2:
            cuts.append(cuts[-1] + rng.uniform(1.2, 2.0))
        cuts.append(STAIR_X + 0.012)
        for xa, xb in zip(cuts, cuts[1:]):
            tone = float(np.clip(rng.normal(0.5, 0.12), 0.0, 1.0))
            B.box(xa, xb, front - 0.02, front + GOING, top - 0.035, top, "Paving", var=tone, faces="yzZ")
            B.quad((xa, front, top - rise), (xb, front, top - rise), (xb, front, top - 0.035), (xa, front, top - 0.035), "Paving", want=(0, -1, 0), var=tone)
    # --- side walls of the flight, stepping up in five stages, with coping slabs and end pedestals
    stages = 5
    seg = STEPS * GOING / stages
    for sx in (-1, 1):
        xi, xo = sx * STAIR_X, sx * (STAIR_X + CHEEK)
        prev_top = None
        for k_ in range(stages):
            ya, yb = FOOT_Y + k_ * seg, FOOT_Y + (k_ + 1) * seg
            top = (k_ + 1) * seg / GOING * rise + 0.55                 # 0.55 m above the highest step it passes
            side_wall(B, xi, -sx, ya, yb, -0.1, top, seed=60 + k_)
            side_wall(B, xo, sx, ya, yb, -0.1, top, seed=70 + k_)
            x0, x1 = sorted((xi, xo))
            B.box(x0 - 0.04, x1 + 0.04, ya - 0.04, yb + (0.04 if k_ == stages - 1 else 0.0), top, top + 0.1, "Limestone", faces="xXyYzZ")   # coping
            B.push(gk.translate(0.0, ya, 0.0))                         # the face that looks down the stairs
            gk.block_wall(B, x0, x1, -0.1 if prev_top is None else prev_top, top, "Ashlar", seed=80 + k_)
            B.pop()
            prev_top = top
        B.quad((min(xi, xo), LAND_Y, FLOOR_Z), (max(xi, xo), LAND_Y, FLOOR_Z), (max(xi, xo), LAND_Y, prev_top), (min(xi, xo), LAND_Y, prev_top),
               "Ashlar", want=(0, 1, 0))                                # its back, seen from the landing
        # pedestal with a ball at the foot
        cxp = sx * (STAIR_X + CHEEK / 2)
        B.box(cxp - 0.62, cxp + 0.62, FOOT_Y - 1.3, FOOT_Y - 0.02, -0.1, 1.5, "Limestone", faces="xXyY")
        B.sweep([(cxp - 0.62, FOOT_Y - 0.02, 1.5), (cxp - 0.62, FOOT_Y - 1.3, 1.5), (cxp + 0.62, FOOT_Y - 1.3, 1.5), (cxp + 0.62, FOOT_Y - 0.02, 1.5)], STRING, "Limestone")
        B.quad((cxp - 0.7, FOOT_Y - 1.38, 1.66), (cxp + 0.7, FOOT_Y - 1.38, 1.66), (cxp + 0.7, FOOT_Y, 1.66), (cxp - 0.7, FOOT_Y, 1.66), "Limestone", want=(0, 0, 1))
        B.push(gk.translate(cxp, FOOT_Y - 0.66, 1.66))
        gk.ball(B, "Limestone", 0.34)
        B.pop()
        # --- bastion beside the foot of the flight: a raised terrace with its own balustrade, so the
        # balustrades step up towards the door in two tiers, as on the photos of the great stair
        xb = sx * BAST_X
        bx0, bx1 = sorted((xo, xb))
        by0, by1 = FOOT_Y, FOOT_Y + BAST_D
        B.push(gk.translate(0.0, by0, 0.0))                           # its front
        gk.block_wall(B, bx0, bx1, -0.1, BAST_Z, "Ashlar", seed=100 + sx)
        B.pop()
        side_wall(B, xb, sx, by0, by1, -0.1, BAST_Z, seed=102 + sx)   # its outer side
        B.push(gk.translate(0.0, by1, 0.0) @ gk.rot_z(180.0))         # its back, seen from the landing
        gk.block_wall(B, -bx1, -bx0, -0.1, BAST_Z, "Ashlar", seed=104 + sx)
        B.pop()
        side_wall(B, xo, -sx, by0, by0 + seg, seg / GOING * rise + 0.55, BAST_Z, seed=106 + sx)   # above the first stage of the side wall
        cap = [(xo, by0), (xb, by0), (xb, by1)]                       # moulding under the terrace floor
        B.sweep([(x, y, BAST_Z - 0.16) for x, y in (cap if sx > 0 else cap[::-1])], STRING, "Limestone")
        gk.slab_floor(B, bx0, bx1, by0, by1, BAST_Z, "Paving", size=(1.0, 0.7), seed=108 + sx)
        xin, xout = xo + sx * 0.13, xb - sx * 0.13
        gk.balustrade(B, "Limestone", (xin, by0 + 0.13), (xout, by0 + 0.13), BAST_Z, h=0.95)
        gk.balustrade(B, "Limestone", (xout, by0 + 0.13), (xout, by1 - 0.13), BAST_Z, h=0.95, posts=(False, True))
        gk.balustrade(B, "Limestone", (xout, by1 - 0.13), (xo, by1 - 0.13), BAST_Z, h=0.95, posts=(False, False))
        gk.balustrade(B, "Limestone", (xin, by0 + 0.13), (xin, by0 + 2 * seg), BAST_Z, h=0.95, posts=(False, False))
        # inside corners of block walls: side wall / landing front, bastion back / side wall, bastion / second stage
        gk.seal_inside_corner(B, "Ashlar", (xo, LAND_Y), (sx, 0.0), (0.0, -1.0), -0.1, FLOOR_Z - 0.03)
        gk.seal_inside_corner(B, "Ashlar", (xo, by1), (0.0, 1.0), (sx, 0.0), -0.1, BAST_Z - 0.03)
        gk.seal_inside_corner(B, "Ashlar", (xo, by0 + seg), (-sx, 0.0), (0.0, -1.0), seg / GOING * rise + 0.55, 2 * seg / GOING * rise + 0.55)
        # --- landing: retaining walls, paving and balustrades -----------------------------------
        xl = sx * LAND_X
        B.push(gk.translate(0.0, LAND_Y, 0.0))                         # front wall of the landing beside the flight
        gk.block_wall(B, *sorted((xo, xl)), -0.1, FLOOR_Z, "Ashlar", seed=90 + sx)
        B.pop()
        side_wall(B, xl, sx, LAND_Y, FLANK_Y, -0.1, FLOOR_Z, seed=92 + sx)
        x0, x1 = sorted((xo, xl))
        B.box(x0 - (0.05 if sx < 0 else 0.0), x1 + (0.05 if sx > 0 else 0.0), LAND_Y - 0.05, LAND_Y + 0.3, FLOOR_Z - 0.02, FLOOR_Z + 0.1, "Limestone", faces="xXyYZ")  # kerb
        ya_ = LAND_Y + 0.3
        B.box(*sorted((xl - sx * 0.3, xl + sx * 0.05)), ya_, FLANK_Y, FLOOR_Z - 0.02, FLOOR_Z + 0.1, "Limestone", faces="xXyZ")
        gk.balustrade(B, "Limestone", (x0 + 0.13 if sx > 0 else x0 + 0.17, LAND_Y + 0.13), (x1 - 0.17 if sx > 0 else x1 - 0.13, LAND_Y + 0.13), FLOOR_Z + 0.1, h=0.95)
        gk.balustrade(B, "Limestone", (xl - sx * 0.14, LAND_Y + 0.42), (xl - sx * 0.14, FLANK_Y - 0.05), FLOOR_Z + 0.1, h=0.95, posts=(False, True))
    gk.slab_floor(B, -LAND_X, LAND_X, LAND_Y, -1.6, FLOOR_Z, "Paving", size=(1.1, 0.8), seed=95)
    for sx in (-1, 1):
        gk.slab_floor(B, *sorted((sx * PANEL_X, sx * LAND_X)), -1.6, FLANK_Y, FLOOR_Z, "Paving", size=(1.1, 0.8), seed=96 + sx)
    B.pop()
    return {"top_z": FLOOR_Z, "foot_y": round(WALL_Y + FOOT_Y, 2), "rise": round(rise, 4)}



# ---------------------------------------------------------------------------------------------
PARTS = {"doorbay": build_doorbay, "centre": build_centre, "frame": build_frame, "stairs": build_stairs}
EYE_Z = LANDING_Z + 1.7
# Per part: the wall region to check (x0, x1, z0, z1), the ray spacing, and the eye points of the see-through check.
AUDITS = {
    "doorbay": ((-BAY_X + 0.02, BAY_X - 0.02, FLOOR_Z + 0.01, T1_TOP - 0.02), 0.015,
                [("front", (0.0, WALL_Y - 9.0, EYE_Z)), ("left", (-8.0, WALL_Y - 4.5, EYE_Z)), ("right", (8.0, WALL_Y - 4.5, EYE_Z)),
                 ("close left", (-3.2, WALL_Y - 2.0, EYE_Z)), ("close right", (3.2, WALL_Y - 2.0, EYE_Z)), ("low", (0.5, WALL_Y - 2.5, LANDING_Z + 0.9))]),
    "centre": ((-PANEL_X + 0.03, PANEL_X - 0.03, FLOOR_Z + 0.01, CENTRE_TOP - 0.03), 0.025,
               [("plaza", (0.0, WALL_Y - 32.0, 2.0)), ("landing", (0.0, WALL_Y - 9.0, EYE_Z)), ("left", (-12.0, WALL_Y - 8.0, EYE_Z)),
                ("right", (12.0, WALL_Y - 8.0, EYE_Z)), ("under", (2.0, WALL_Y - 3.0, EYE_Z))]),
    "frame": ((-12.5, 12.5, 0.05, 32.5), 0.03,
              [("plaza", (0.0, WALL_Y - 34.0, 1.7)), ("left", (-12.0, WALL_Y - 9.0, EYE_Z)), ("right", (12.0, WALL_Y - 9.0, EYE_Z)),
               ("foot", (3.0, WALL_Y - 14.0, 1.7))]),
    "stairs": ((-12.5, 12.5, 0.05, 6.4), 0.02,
               [("plaza", (0.0, WALL_Y - 34.0, 1.7)), ("foot left", (-10.0, WALL_Y - 16.0, 1.7)), ("foot right", (10.0, WALL_Y - 16.0, 1.7)),
                ("on the steps", (2.0, WALL_Y - 9.0, 3.4))]),
}
# Per part: preview shots (tag, camera position, look-at point, lens, width, height).
SHOTS = {
    "doorbay": [("wide", (0.0, WALL_Y - 9.5, EYE_Z), (0.0, WALL_Y, LANDING_Z + 2.9), 24, 1500, 1000),
                ("oblique", (6.5, WALL_Y - 5.0, EYE_Z), (1.0, WALL_Y, LANDING_Z + 2.8), 26, 1500, 1000),
                ("door", (0.0, WALL_Y - 3.2, EYE_Z), (0.0, WALL_Y, LANDING_Z + 1.9), 28, 1100, 1300),
                ("capital", (3.4, WALL_Y - 2.4, LANDING_Z + 3.4), (2.5, WALL_Y - 0.4, LANDING_Z + 4.5), 60, 1500, 1000),
                ("niche", (3.5, WALL_Y - 2.6, EYE_Z), (3.52, WALL_Y, LANDING_Z + 2.6), 35, 1000, 1300)],
    "centre": [("wide", (0.0, WALL_Y - 44.0, 8.0), (0.0, WALL_Y, 17.6), 30, 1150, 1500),
               ("upper", (4.0, WALL_Y - 17.0, 15.0), (0.0, WALL_Y, 21.0), 30, 1300, 1300),
               ("rose", (3.0, WALL_Y - 9.0, 20.5), (0.0, WALL_Y, 22.0), 30, 1400, 1100),
               ("storeys", (7.0, WALL_Y - 9.0, 12.5), (0.5, WALL_Y, 13.8), 28, 1500, 1100),
               ("window", (10.5, WALL_Y - 6.5, 15.2), (7.3, WALL_Y, 15.6), 32, 1100, 1300),
               ("arcade", (3.0, WALL_Y - 11.0, 27.0), (0.0, WALL_Y, 28.9), 32, 1500, 900)],
    "frame": [("wide", (0.0, WALL_Y - 52.0, 1.7), (0.0, WALL_Y, 16.0), 24, 1300, 1500),
              ("corner", (24.0, WALL_Y - 16.0, 1.7), (13.0, WALL_Y, 12.0), 24, 1100, 1500),
              ("crown", (9.0, WALL_Y - 12.0, 26.0), (12.0, WALL_Y, 31.6), 35, 1500, 1000),
              ("base", (15.5, WALL_Y - 6.0, 1.7), (13.0, WALL_Y, 2.2), 30, 1500, 1000)],
    "stairs": [("wide", (0.0, WALL_Y - 26.0, 1.7), (0.0, WALL_Y - 6.0, 3.6), 24, 1500, 1000),
               ("side", (13.0, WALL_Y - 19.0, 1.7), (5.0, WALL_Y - 7.0, 3.2), 26, 1500, 1000),
               ("landing", (9.0, WALL_Y - 1.5, EYE_Z + 0.2), (10.0, WALL_Y - 5.0, FLOOR_Z + 0.5), 22, 1500, 1000),
               ("steps", (1.5, WALL_Y - 13.2, 1.2), (0.0, WALL_Y - 10.0, 1.6), 30, 1500, 1000)],
}

bpy.ops.wm.read_factory_settings(use_empty=True)
B = gk.Builder(MATERIALS)
info = PARTS[args.part](B)
name = f"SM_Cathedral_Front_{args.part}"
obj = B.to_object(name, COLORS)
print(f"CAVITY mean {gk.bake_cavity(obj):.3f}")
print(f"WEATHER streaks (mean, share of upright stone) {gk.bake_weather(obj)}")
context = None                  # the other parts of the front, only for the see-through check and the previews
if args.audit or args.renders:
    context = gk.Builder(MATERIALS)
    for other, build in PARTS.items():
        if other != args.part:
            build(context)
if args.audit:
    region, step, eyes = AUDITS[args.part]
    target = obj
    if context is not None:
        both = gk.Builder(MATERIALS)
        both.chunks = B.chunks + context.chunks
        target = both.to_object("audit", COLORS)
    holes = gk.audit_see_through(target, WALL_Y, *region, eyes, step=step)
    if target is not obj:
        bpy.data.objects.remove(target, do_unlink=True)
    print(f"AUDIT total see-through rays: {holes}")
co = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
obj.data.vertices.foreach_get("co", co)
co = co.reshape(-1, 3)
print(f"BUILT {name}: {B.triangles()} triangles, {len(B.chunks)} chunks, bounds {co.min(0).round(3)} .. {co.max(0).round(3)}, info {info}")

os.makedirs(args.out, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(args.out, f"Girona_Front_{args.part}.blend"))
bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj
bpy.ops.export_scene.fbx(
    filepath=os.path.join(args.out, name + ".fbx"), use_selection=True, object_types={'MESH'},
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y',
    mesh_smooth_type='FACE', colors_type='SRGB', use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
print("EXPORTED")

# ---------------------------------------------------------------------------------------------
# Optional preview renders: clay shading with cavity, back faces culled so a flipped face shows as a hole
# ---------------------------------------------------------------------------------------------
if args.renders:
    from mathutils import Vector
    os.makedirs(args.renders, exist_ok=True)
    scene = bpy.context.scene
    if context is not None:
        context.to_object("context", COLORS)
    if args.template:
        with bpy.data.libraries.load(args.template, link=False) as (src, dst):
            dst.objects = list(src.objects)
        for o in dst.objects:
            if o is not None and o.type == 'MESH':
                scene.collection.objects.link(o)
    scene.render.engine = 'BLENDER_WORKBENCH'
    sh = scene.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'MATERIAL'
    sh.show_cavity = True
    sh.cavity_type = 'BOTH'
    sh.cavity_ridge_factor = 1.2
    sh.cavity_valley_factor = 1.4
    sh.show_shadows = False
    sh.show_backface_culling = True
    sh.show_object_outline = False
    scene.world = bpy.data.worlds.new("w")
    cd = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cd.clip_start, cd.clip_end = 0.05, 500
    for tag, pos, at, lens, rx, ry in SHOTS[args.part]:
        cam.location = Vector(pos)
        cam.rotation_euler = (Vector(at) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()
        cd.lens = lens
        scene.render.resolution_x, scene.render.resolution_y = rx, ry
        scene.render.filepath = os.path.join(args.renders, f"{args.part}_{tag}.png")
        bpy.ops.render.render(write_still=True)
    print("RENDERED")
