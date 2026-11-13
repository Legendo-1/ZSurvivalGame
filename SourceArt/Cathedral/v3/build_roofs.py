"""Builds the roofscape of the Girona cathedral: everything that closes the building from above once the
Meshy template is retired.

Coordinates are those of the Meshy model (front = -Y, bell tower = +X, metres), as in build_front.py.

Part:
  main    the tiled nave roof with its ridge, the lean-to roofs of the chapels, the paved terrace over the
          east end, the nave's east gable with a round window, the neck that joins nave and apse, the back
          of the front's parapet and the foot of the bell storey where it rises out of the roof.

Run: see build_common.py. Writes Girona_Roof_main.blend and SM_Cathedral_Roof_main.fbx.
"""
import math
import os
import sys

import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gothic_kit as gk  # noqa: E402
import build_common  # noqa: E402

A_NAVE, EAVE, RIDGE = 10.85, 29.95, 33.1        # nave wall face, its eave and the ridge of its roof
Y_FRONT, Y_EAST = -41.6, 20.5                   # back of the front's parapet, the nave's east wall
A_CH, CH_TOP, LEAN_TOP = 17.0, 10.85, 11.9      # chapel wall face, its top, where the lean-to meets the nave wall
CH_Y0, EAST_Y = -23.4, 26.4                     # the lean-to roofs start inside the first buttress; east wall of the chapels
TOP_Z = 32.95                                   # top of the front's parapet
C_TOP, A_TOP = (11.184, -36.9), 4.6             # the bell storey (build_bell_tower.py)
NECK_X, NECK_Y1, NECK_TOP = 2.1, 29.5, 23.2     # the neck between the nave and the apse's upper tier
STRING = [[(0.0, 0.0), (0.05, 0.03), (0.07, 0.07)], [(0.07, 0.07), (0.07, 0.13)], [(0.07, 0.13), (0.03, 0.16), (0.0, 0.16)]]


def build_main(B):
    # --- nave roof: two tiled slopes and a ridge roll -------------------------------------------------
    ae = A_NAVE - 0.35
    for sx in (-1, 1):
        gk.tile_facet(B, "Tile", (sx * ae, Y_FRONT, EAVE + 0.05), (sx * ae, Y_EAST, EAVE + 0.05), (0.0, Y_EAST, RIDGE), (0.0, Y_FRONT, RIDGE),
                      pitch=0.45, seed=1001 + sx)
    B.tube([(0.0, Y_FRONT, RIDGE + 0.05), (0.0, (Y_FRONT + Y_EAST) / 2, RIDGE + 0.05), (0.0, Y_EAST, RIDGE + 0.05)], 0.13, "Tile", sides=8, var=0.35)
    # --- chapel lean-to roofs, with a closed end towards the east terrace -----------------------------------
    for sx in (-1, 1):
        lo, hi = sx * (A_CH - 0.45), sx * (A_NAVE + 0.02)
        gk.tile_facet(B, "Tile", (lo, CH_Y0, CH_TOP + 0.07), (lo, Y_EAST, CH_TOP + 0.07), (hi, Y_EAST, LEAN_TOP), (hi, CH_Y0, LEAN_TOP), seed=1010 + sx)
        B.fan([(lo, Y_EAST, CH_TOP + 0.04), (hi, Y_EAST, CH_TOP + 0.04), (hi, Y_EAST, LEAN_TOP + 0.03)], "Ashlar", (0, 1, 0))
    # --- paved terrace over the east end, between the nave, the chapels' east wall and the apse ----------------
    gk.slab_floor(B, -(A_CH - 0.4), A_CH - 0.4, Y_EAST, EAST_Y - 0.3, CH_TOP + 0.04, "Paving", size=(1.1, 0.8), seed=1020)
    gk.slab_floor(B, -6.8, 6.8, EAST_Y - 0.3, EAST_Y + 1.5, CH_TOP + 0.04, "Paving", size=(1.1, 0.8), seed=1021)
    # --- the nave's east gable: wall, round window, raking coping ------------------------------------------------
    m, length = gk.wall_frame((A_NAVE, Y_EAST), (-A_NAVE, Y_EAST))          # seen from the apse side
    gable_top = RIDGE + 0.4
    B.push(m)
    holes = [gk.rose_window(B, length / 2, 27.3, 1.25, 0.95, "Limestone", "Glass", "Iron", recess=0.4, bosses=12)]
    z = EAVE
    while z < gable_top - 1e-6:                              # cut the corners off above the eaves, course by course
        zt = min(z + 0.31, gable_top)
        cut = (z - EAVE) / (gable_top - EAVE) * (length / 2)
        if cut > 0.02:
            holes += [(-0.01, cut, z, zt, "tb"), (length - cut, length + 0.01, z, zt, "tb")]
        z = zt
    gk.block_wall(B, 0.0, length, CH_TOP, gable_top, "Ashlar", holes=holes, seed=1030)
    gk.course(B, "Limestone", [(-0.2, EAVE + 0.2), (length / 2, gable_top + 0.22), (length + 0.2, EAVE + 0.2)], s=0.8)
    B.pop()
    m, length = gk.wall_frame((-A_NAVE, Y_EAST - 0.45), (A_NAVE, Y_EAST - 0.45))  # its back, above the roof
    B.push(m)
    z, holes = EAVE, []
    while z < gable_top - 1e-6:
        zt = min(z + 0.31, gable_top)
        cut = (z - EAVE) / (gable_top - EAVE) * (length / 2)
        if cut > 0.02:
            holes += [(-0.01, cut, z, zt, "tb"), (length - cut, length + 0.01, z, zt, "tb")]
        z = zt
    gk.block_wall(B, 0.0, length, EAVE - 0.3, gable_top, "Ashlar", holes=holes, seed=1031)
    B.pop()
    # --- the neck: two walls, a cornice and a small tiled roof -------------------------------------------------
    for sx in (-1, 1):
        frame = gk.wall_frame((NECK_X, Y_EAST), (NECK_X, NECK_Y1)) if sx > 0 else gk.wall_frame((-NECK_X, NECK_Y1), (-NECK_X, Y_EAST))
        m, length = frame
        B.push(m)
        gk.block_wall(B, 0.0, length, CH_TOP, NECK_TOP, "Ashlar", seed=1040 + sx)
        gk.course(B, "Limestone", [(0.0, NECK_TOP - 0.2), (length, NECK_TOP - 0.2)], s=0.66, caps=False)
        B.pop()
        gk.tile_facet(B, "Tile", (sx * (NECK_X + 0.2), Y_EAST, NECK_TOP + 0.02), (sx * (NECK_X + 0.2), NECK_Y1, NECK_TOP + 0.02),
                      (0.0, NECK_Y1, NECK_TOP + 0.85), (0.0, Y_EAST, NECK_TOP + 0.85), seed=1044 + sx)
    B.tube([(0.0, Y_EAST, NECK_TOP + 0.88), (0.0, NECK_Y1, NECK_TOP + 0.88)], 0.1, "Tile", sides=8, var=0.35)
    # --- the back of the front's parapet, from the left corner block to the bell tower --------------------------
    m, length = gk.wall_frame((6.5, Y_FRONT), (-A_NAVE - 0.1, Y_FRONT))
    B.push(m)
    gk.block_wall(B, 0.0, length, EAVE - 0.6, TOP_Z, "Ashlar", seed=1050)
    B.pop()
    # --- the foot of the bell storey, where it rises out of the roof -----------------------------------------
    hb = A_TOP * gk.T8
    for k in range(8):
        B.push(gk.face_matrix(C_TOP, A_TOP, 45.0 * k))
        gk.block_wall(B, -hb, hb, EAVE - 0.6, 32.25, "Ashlar", seed=1060 + k)
        B.pop()
    return {"ridge": RIDGE, "eave": EAVE, "gable top": gable_top}


PARTS = {"main": build_main}
SHOTS = {"main": [("air", (60.0, -70.0, 60.0), (0.0, -10.0, 22.0), 35, 1500, 1100),
                  ("east", (30.0, 44.0, 44.0), (0.0, 22.0, 20.0), 35, 1500, 1100),
                  ("front", (-22.0, -30.0, 46.0), (0.0, -38.0, 31.0), 35, 1500, 1100)]}
build_common.run("Roof", PARTS, {}, SHOTS)
