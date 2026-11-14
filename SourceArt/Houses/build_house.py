"""Builds one house from its spec: SM_House_<Folder>_Ext (outside, with UCX_ collision) and SM_House_<Folder>_Int
(floors, ceilings, inner wall faces, stair, guards), then bakes, exports and checks them.

Run (Blender 5.2, no UI), from the project folder:
  blender -b --factory-startup --python SourceArt/Houses/build_house.py -- --spec SourceArt/Houses/specs/<Folder>.json
      [--out SourceArt/Houses/build/<Folder>] [--audit] [--renders <folder>] [--no-bake]
Writes <out>/<Folder>.blend, <out>/SM_House_<Folder>_Ext.fbx and <out>/SM_House_<Folder>_Int.fbx.
  --audit    the see-through check (gothic_kit.audit_plane) from street and room eye points; must print 0
  --renders  clay renders (Workbench, cavity, back faces culled so a flipped face shows as a hole) and, when the spec
             names the Meshy measuring picture, compare_front.png: the Meshy front | the new front, same scale

The spec's "type" picks the builder. "townhouse_flat": a flat-roofed terraced house, walls in horizontal zones
(coursed blocks or flat brick), string courses, a crowning cornice and parapet, windows and doors per facade, wall
features (clock), chimneys, floors and a dog-leg stair against the back wall. Other types get added by later houses.
"""
import argparse
import json
import math
import os
import sys

import bpy
import numpy as np

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import house_kit as hk  # noqa: E402
import gothic_kit as gk  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--spec", required=True)
ap.add_argument("--out")
ap.add_argument("--audit", action="store_true")
ap.add_argument("--renders")
ap.add_argument("--no-bake", action="store_true")
args = ap.parse_args(argv)
spec = json.load(open(args.spec, encoding="utf-8"))
FOLDER = spec["folder"]
OUT = args.out or os.path.join(HERE, "build", FOLDER)


def xf(B, pts):
    """Points from the Builder's current frame (e.g. wall coordinates) to house space."""
    m = B.stack[-1]
    p = np.asarray(pts, dtype=np.float64)
    return (p @ m[:3, :3].T + m[:3, 3]).tolist()


# ---------------------------------------------------------------------------------------------
# townhouse_flat
# ---------------------------------------------------------------------------------------------
def zone_at(zones, z):
    for zn in zones:
        if zn["z0"] <= z < zn["z1"]:
            return zn
    return zones[-1]


DOORS = []      # separate door leaves ("leaf": {"separate": true}), filled by opening(), written out by main()


def opening(E, I, op, t, zones, floor0, hulls):
    """One window or door in wall coordinates. Returns the hole to leave in the wall faces."""
    x0, x1, z0, z1 = op["x0"], op["x1"], op["z0"], op["z1"]
    rec = op.get("recess", 0.12)
    depth = 0.08
    zone_mat = zone_at(zones, (z0 + z1) / 2)["mat"]
    if op["kind"] == "window":
        c, r = op.get("panes", [2, 3])
        hk.sash_window(E, x0, x1, z0, z1, "Metal", "Glass", c, r, y=rec, depth=depth, meeting=op.get("sash", True))
        hk.reveal(E, (x0, x1, z0, z1), 0.0, rec, zone_mat, sides="lrt" + ("" if op.get("sill") else "b"))
        hk.reveal(I, (x0, x1, z0, z1), rec, t, "Plaster")         # from the frame's front: no gap behind the frame
        if op.get("sill"):
            s = op["sill"]
            hk.sill(E, x0 - s.get("ext", 0.08), x1 + s.get("ext", 0.08), z0, "Trim", h=s.get("h", 0.17), proud=s.get("proud", 0.07), back=rec)
        head = op.get("head")
        if head:
            hx0, hx1 = x0 - head.get("ext", 0.0), x1 + head.get("ext", 0.0)
            if head["kind"] == "lintel":
                hk.lintel(E, hx0, hx1, head["z0"], head["z1"], "Trim", proud=head.get("proud", 0.035))
            elif head["kind"] == "arch":
                hk.gauged_arch(E, hx0, hx1, head["z0"], head["z1"], "Gauged", n=head.get("n", 9), splay=head.get("splay", 0.1))
        return (x0, x1, z0, z1)
    # door: opening from the street up, threshold step, transom light, open leaf, stone surround
    tr = op.get("transom", 0.0)
    ztr = z1 - tr
    hk.reveal(E, (x0, x1, 0.0, z1), 0.0, rec, "Trim", sides="lrt")
    hk.reveal(I, (x0, x1, 0.0, z1), rec, t, "Plaster", sides="lrt")
    hk.block(E, x0, x1, -op.get("step_proud", 0.3), t, 0.0, floor0, "Trim", faces="xXyZ")
    if tr > 0:
        hk.sash_window(E, x0, x1, ztr, z1, "Metal", "Glass", op.get("transom_panes", 3), 1, y=rec, depth=depth, meeting=False)
    sr = op.get("surround")
    if sr:
        w, p = sr["w"], sr.get("proud", 0.05)
        hk.block(E, x0 - w, x0, -p, 0.0, 0.0, sr["cap"][0], "Trim", faces="xXyZ")
        hk.block(E, x1, x1 + w, -p, 0.0, 0.0, sr["cap"][0], "Trim", faces="xXyZ")
        if sr["cap"][0] > z1 + 1e-3:
            hk.block(E, x0, x1, -p, 0.0, z1, sr["cap"][0], "Trim", faces="yz")
        cp = sr.get("cap_proud", 0.09)
        hk.block(E, x0 - w - 0.03, x1 + w + 0.03, -cp, 0.0, sr["cap"][0], sr["cap"][1], "Trim", faces="xXyzZ")
    lf = op.get("leaf")
    if lf and lf.get("separate"):
        # Closed leaf as its own mesh SM_House_<Folder>_Door, in hinge space: origin on the hinge axis at the foot of
        # the leaf, the leaf along +x (left hinge) or -x (right hinge), its 6 cm thickness on -y, so it fills the frame
        # at y rec..rec+0.06 when closed and swings into the doorway (not into the reveal) when it opens later.
        w = x1 - x0 - 0.01
        h = ztr - floor0 - 0.01 if tr > 0 else z1 - floor0 - 0.01
        left = lf.get("hinge", "left") == "left"
        hinge = (x0 if left else x1, rec + 0.06, floor0)
        shift = gk.translate(0.0 if left else -w, -0.06, 0.0)
        D, P = hk.Builder(hk.MATERIALS, seed=31 + len(DOORS)), hk.Builder(hk.MATERIALS, seed=31 + len(DOORS))
        P.stack = [E.stack[-1] @ gk.translate(*hinge)]           # the same leaf in house space, for the audit and renders
        for B in (D, P):
            B.push(shift)
            hk.panel_leaf(B, w, h, "Door", t=0.06, cols=lf.get("cols", 2), rows=lf.get("rows", 3))
            B.pop()
        m = E.stack[-1]
        DOORS.append({"builder": D, "preview": P, "hull": hk.box_pts(0.0 if left else -w, w if left else 0.0, -0.06, 0.0, 0.0, h),
                      "hinge": xf(E, [hinge])[0], "facade_yaw": math.degrees(math.atan2(m[1, 0], m[0, 0])), "left": left})
        return (x0, x1, 0.0, z1)
    if lf:
        w = x1 - x0
        h = ztr - floor0 - 0.01 if tr > 0 else z1 - floor0 - 0.01
        a = lf.get("open_deg", 95.0)
        if lf.get("hinge", "left") == "left":
            m = gk.translate(x0, t, floor0) @ gk.rot_z(a)
        else:
            m = gk.translate(x1, t, floor0) @ gk.rot_z(180.0 - a) @ np.diag([1.0, -1.0, 1.0, 1.0])
        E.push(m)
        hk.panel_leaf(E, w - 0.01, h, "Door", t=0.06, cols=lf.get("cols", 2), rows=lf.get("rows", 3))
        hulls.append(xf(E, hk.box_pts(0.0, w - 0.01, 0.0, 0.06, 0.0, h)))
        E.pop()
    return (x0, x1, 0.0, z1)


def build_townhouse_flat(spec):
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    floors, T = spec["floors"], spec["slab"]
    ceil, deck = spec["ceiling"], spec["roof"]["deck"]
    zones = spec["zones"]
    zc = spec["cornice"]["z"]
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    for side in ("front", "right", "back", "left"):
        fac = spec["facades"].get(side, {})
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        holes, doors = [], []
        for op in fac.get("openings", []):
            h = opening(E, I, op, t, zones, floors[0], hulls)
            holes.append(h)
            if op["kind"] == "door":
                doors.append((op["x0"], op["x1"], op["z1"] - op.get("transom", 0.0)))
        for ft in fac.get("features", []):
            if ft["kind"] == "clock":
                hk.clock_dial(E, ft["cx"], ft["cz"], ft["half"], ft["r"], ft.get("plate", "Bronze"), "Brass", "Dial", "Metal", brass="Brass")
        for k, zn in enumerate(zones):
            zh = [hh for hh in holes if hh[2] < zn["z1"] and hh[3] > zn["z0"]]
            if zn["kind"] == "blocks":
                hk.rubble_face(E, 0.0, L, zn["z0"], zn["z1"], zn["mat"], holes=zh, course=zn.get("course", 0.39),
                               length=tuple(zn.get("length", (0.45, 1.0))), chamfer=zn.get("chamfer", 0.03),
                               jitter=zn.get("jitter", 0.012), seed=7 + 13 * k + 101 * ("front", "right", "back", "left").index(side))
            else:
                hk.flat_face(E, 0.0, L, zn["z0"], zn["z1"], zn["mat"], holes=zh, cell=zn.get("cell", 0.6))
        hk.flat_face(I, t, L - t, floors[0], ceil, "Plaster", holes=holes, y=t, inward=True)
        hk.wall_core(E, L, 0.0, zc + gk.ENTABLATURE_H, t, holes=holes)
        hk.flat_face(E, t, L - t, deck, zc + gk.ENTABLATURE_H, "Trim", y=t, inward=True)          # parapet, inside
        # collision: the wall as boxes, open at the doors (up to the transom), windows closed
        top = zc + gk.ENTABLATURE_H
        cuts = sorted(doors)
        xa = 0.0
        for dx0, dx1, dz in cuts:
            hulls.append(xf(E, hk.box_pts(xa, dx0, 0.0, t, 0.0, top)))
            hulls.append(xf(E, hk.box_pts(dx0, dx1, 0.0, t, dz, top)))
            hulls.append(xf(E, hk.box_pts(dx0, dx1, -0.3, t, -0.2, floors[0])))           # threshold step
            xa = dx1
        hulls.append(xf(E, hk.box_pts(xa, L, 0.0, t, 0.0, top)))
        E.pop()
        I.pop()
    for b in spec.get("bands", []):
        hk.band_loop(E, W, D, b["z0"], b["z1"], b["proud"], b.get("mat", "Trim"), sides=b.get("sides"))
    hk.cornice_loop(E, W, D, zc, spec["cornice"].get("mat", "Trim"), wall=t, dentils=spec["cornice"].get("dentils", True))
    hk.rect(E, (t, t, deck), (W - t, t, deck), (W - t, D - t, deck), (t, D - t, deck), spec["roof"].get("mat", "Roof"), (0, 0, 1))
    hk.rect(I, (t, t, ceil), (W - t, t, ceil), (W - t, D - t, ceil), (t, D - t, ceil), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, ceil, deck))
    for c in spec.get("chimneys", []):
        hulls += hk.chimney(E, c["x0"], c["x1"], c["y0"], c["y1"], deck, c["z1"], c.get("mat", "Brick"), c.get("cap_mat", "Gauged"), "Clay",
                            pots=c.get("pots", 2), cap_h=c.get("cap_h", 0.26), pot_h=c.get("pot_h", 0.73))
    # floors and the stair
    st = spec["stair"]
    well, sh = hk.dogleg_stair(I, None, floors, st["risers"], t, st["x_half"], st["x_land"], W - t, D - t, fw=st.get("flight_width", 1.2),
                               spine=st.get("spine", 0.12), thick=T, ceiling=ceil, guard=st.get("guard", 1.0))
    hulls += sh
    wx0, wx1, wy0, wy1 = well
    hk.rect(I, (t, t, floors[0]), (W - t, t, floors[0]), (W - t, D - t, floors[0]), (t, D - t, floors[0]), "Timber", (0, 0, 1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, floors[0]))
    for k, F in enumerate(floors[1:], start=1):
        rects = [(t, W - t, t, wy0), (wx1, W - t, wy0, D - t)]
        hk.slab_rects(I, rects, F, T, "Timber", "Plaster")
        hk.rect(I, (wx0, wy0, F - T), (wx1, wy0, F - T), (wx1, wy0, F), (wx0, wy0, F), "Plaster", (0, 1, 0))
        if k == len(floors) - 1:              # top floor: the slab edge over the back row, where no flight starts
            ysp = wy1 - st.get("flight_width", 1.2)
            hk.rect(I, (wx1, ysp, F - T), (wx1, wy1, F - T), (wx1, wy1, F), (wx1, ysp, F), "Plaster", (-1, 0, 0))
        for x0, x1, y0, y1 in rects:
            hulls.append(hk.box_pts(x0, x1, y0, y1, F - T, F))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_gable (house #2 on): a terraced house with a gable roof between two coped gable walls, a facade of
# zones at their own depths (a recessed rusticated ground floor), swept mouldings, architrave windows with sills,
# brackets and hoods, a grand double door, wall lanterns and an empty shell inside (spec "interior": "shell")
# ---------------------------------------------------------------------------------------------
def separate_leaf(E, hinge_x, left, w, h, y_front, z0, t, make, seed):
    """One closed door leaf as its own mesh (DOORS), in hinge space: origin on the hinge axis at the foot, the leaf
    along +x (left hinge) or -x (right hinge), its thickness t on -y, so that it fills y_front..y_front + t when
    closed. make(B) builds the leaf in leaf space (x 0..w, y 0..t, z 0..h)."""
    hinge = (hinge_x, y_front + t, z0)
    shift = gk.translate(0.0 if left else -w, -t, 0.0)
    D, P = hk.Builder(hk.MATERIALS, seed=seed), hk.Builder(hk.MATERIALS, seed=seed)
    P.stack = [E.stack[-1] @ gk.translate(*hinge)]
    for B in (D, P):
        B.push(shift)
        make(B)
        B.pop()
    m = E.stack[-1]
    DOORS.append({"builder": D, "preview": P, "hull": hk.box_pts(0.0 if left else -w, w if left else 0.0, -t, 0.0, 0.0, h),
                  "hinge": xf(E, [hinge])[0], "facade_yaw": math.degrees(math.atan2(m[1, 0], m[0, 0])), "left": left})


def gable_outline(spec):
    """The roof outline over the side walls, [(y, z), ...] front to back: cornice top, gutter, front slope, the
    upright step to the ridge, back slope down to the back eave."""
    D, rf = spec["depth"], spec["roof"]
    top = spec["front"]["top"]
    yt, zt = rf["front_top"]
    eave = rf["ridge"] - rf["back_pitch"] * (D - yt)
    return [(0.0, top), (rf["gutter_y"], top), (yt, zt), (yt, rf["ridge"]), (D, eave)], eave


def build_townhouse_gable(spec):
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    F0, CEIL = spec["floor"], spec["ceiling"]
    fr, rf = spec["front"], spec["roof"]
    TOP = fr["top"]
    outline, eave = gable_outline(spec)
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    dr = fr["door"]
    door_hole = (dr["x0"], dr["x1"], 0.0, dr["z_top"])
    ws = fr["window_style"]
    win_holes = [(w["x0"], w["x1"], w["z0"], w["z1"]) for w in fr["windows"]]

    # --- front (wall coordinates = house coordinates) -----------------------------------------
    for k, zn in enumerate(fr["zones"]):
        holes = [h for h in [door_hole] + win_holes if h[2] < zn["z1"] and h[3] > zn["z0"]]
        if zn["kind"] == "blocks":
            E.push(gk.translate(0.0, zn["y"], 0.0))
            hk.rubble_face(E, 0.0, W, zn["z0"], zn["z1"], zn["mat"], holes=holes, course=zn["course"], length=tuple(zn["length"]),
                           chamfer=zn["chamfer"], jitter=zn["jitter"], seed=7 + 13 * k)
            E.pop()
        else:
            hk.flat_face(E, 0.0, W, zn["z0"], zn["z1"], zn["mat"], holes=holes, y=zn["y"], cell=0.6)
        q = zn.get("quoins")
        if q:                                   # corner blocks standing proud of a recessed zone, alternating length
            n = max(1, round((zn["z1"] - zn["z0"]) / zn["course"]))
            hc = (zn["z1"] - zn["z0"]) / n
            for i in range(n):
                za, zb = zn["z0"] + i * hc + 0.006, zn["z0"] + (i + 1) * hc - 0.006
                ln = q["length"][i % 2]
                hk.block(E, 0.0, ln, q["y"], zn["y"] + 0.02, za, zb, zn["mat"], faces="yXzZ")
                hk.block(E, W - ln, W, q["y"], zn["y"] + 0.02, za, zb, zn["mat"], faces="yxzZ")
    for mo in fr["mouldings"]:
        pieces = [(0.0, W, 0.0, 1.0)]
        for bx0, bx1 in mo.get("breaks", []):          # e.g. a plinth cap stopping at the door's pedestals
            pieces = hk._minus(pieces, (bx0, bx1, -1.0, 2.0))
        for px0, px1, _a, _b in pieces:
            hk.moulding_run(E, px0, px1, mo["z"], mo["profile"], mo["mat"], y=mo["y"], hub_v=mo.get("hub_v"))
    # windows: frame and glass, reveals, architrave, sill on brackets, hood
    for w in fr["windows"]:
        x0, x1, z0, z1 = w["x0"], w["x1"], w["z0"], w["z1"]
        rec = ws["rec"]
        c, r = ws["panes"]
        hk.sash_window(E, x0, x1, z0, z1, ws["frame"], ws["glass"], c, r, y=rec, depth=ws["depth"])
        hk.reveal(E, (x0, x1, z0, z1), 0.0, rec, ws["reveal_mat"], sides="lrt")
        hk.reveal(I, (x0, x1, z0, z1), rec, t, "Plaster")
        ar = ws["architrave"]
        hk.frame_run(E, x0, x1, z0, z1, ar["profile"], ar["mat"])
        aw = max(p[0] for p in ar["profile"])
        s = ws["sill"]
        zs = z0 - s["drop"]
        hk.sill(E, x0 - s["ext"], x1 + s["ext"], zs, s["mat"], h=s["h"], proud=s["proud"], back=rec)
        b = ws["brackets"]
        zb1 = zs - s["h"] - b["gap"]
        for bx0 in (x0 - aw, x1 + aw - b["w"]):
            hk.block(E, bx0, bx0 + b["w"], -b["proud"], 0.0, zb1 - b["h"], zb1, b["mat"], faces="xXyzZ")
        hd = ws["hood"]
        hk.moulding_run(E, x0 - hd["ext"], x1 + hd["ext"], z1 + aw + hd["gap"] - 0.02, hd["profile"], hd["mat"])
    # door: steps, pedestals, pilasters with sunk panels, consoles, architrave, hood, reveals, transom, two leaves
    for st in dr["steps"]:
        hk.block(E, st["x0"], st["x1"], st["y0"], t, st["z0"], st["z1"], st["mat"], faces="xXyZ")
        hulls.append(hk.box_pts(st["x0"], st["x1"], st["y0"], t, st["z0"], st["z1"]))
    pd = dr["pedestals"]
    yg = fr["zones"][1]["y"]                  # the plane of the recessed ground floor
    for px0, px1 in pd["x"]:
        hk.block(E, px0, px1, pd["front"], yg, 0.0, pd["z1"], pd["mat"], faces="xXyZ")
        hk.block(E, px0, px1, pd["cap_front"], yg, pd["z1"], pd["cap"], pd["mat"], faces="xXyZ")
        hulls.append(hk.box_pts(px0, px1, pd["front"], yg, 0.0, pd["cap"]))
    pl = dr["pilasters"]
    for px0, px1 in pl["x"]:
        m, pz = pl["panel_margin"], pl["panel_z"]
        panel = (px0 + m, px1 - m, pz[0], pz[1])
        hk.flat_face(E, px0, px1, pl["z0"], pl["z1"], pl["mat"], holes=[panel], y=pl["front"])
        hk.flat_face(E, *panel, pl["mat"], y=pl["panel_front"])
        hk.reveal(E, panel, pl["front"], pl["panel_front"], pl["mat"])
        hk.block(E, px0, px1, pl["front"], yg, pl["z0"], pl["z1"], pl["mat"], faces="xX")
        hulls.append(hk.box_pts(px0, px1, pl["front"], yg, pl["z0"], pl["z1"]))
    cs = dr["consoles"]
    for cx in cs["x"]:
        hk.console(E, cx, cs["y_wall"], cs["z_top"], cs["mat"], out=cs["out"], height=cs["height"], width=cs["width"])
    ar = dr["architrave"]
    hk.frame_run(E, dr["x0"], dr["x1"], ar["z0"], dr["z_top"], ar["profile"], ar["mat"], y=ar["y"])
    hd = dr["hood"]
    hk.moulding_run(E, hd["x0"], hd["x1"], hd["z"], hd["profile"], hd["mat"], y=hd["y"], hub_v=hd.get("hub_v"))
    x0, x1, zf, ztop = dr["x0"], dr["x1"], dr["z_floor"], dr["z_top"]
    ly, lt, ltop = dr["leaf_y"], dr["leaf_t"], dr["leaf_top"]
    tr = dr["transom"]
    hk.reveal(E, (x0, x1, zf, ztop), yg, ly, dr["reveal_mat"], sides="lrt")
    hk.reveal(E, (x0, x1, ltop, ztop), ly, tr["back_y"], dr["reveal_mat"], sides="lrt")
    hk.reveal(I, (x0, x1, zf, ltop), ly + lt, t, "Plaster", sides="lr")
    hk.reveal(I, (x0, x1, zf, ztop), tr["back_y"], t, "Plaster", sides="t")
    hk.reveal(I, (x0, x1, ltop, ztop), tr["back_y"], t, "Plaster", sides="lr")
    # transom: a studded rail, then two lattice lights either side of a mullion, in an iron frame
    fm = dr["frame_mat"]
    xm = (x0 + x1) / 2
    lights = [(x0 + tr["side"], xm - tr["mullion"] / 2, tr["rail_top"], tr["grille_top"]),
              (xm + tr["mullion"] / 2, x1 - tr["side"], tr["rail_top"], tr["grille_top"])]
    hk.flat_face(E, x0, x1, ltop, ztop, fm, holes=lights, y=ly)
    hk.flat_face(E, x0, x1, ltop, ztop, fm, holes=lights, y=tr["back_y"], inward=True)
    hk.rect(E, (x0, ly, ltop), (x1, ly, ltop), (x1, tr["back_y"], ltop), (x0, tr["back_y"], ltop), fm, (0, 0, -1))
    for lx0, lx1, lz0, lz1 in lights:
        hk.reveal(E, (lx0, lx1, lz0, lz1), ly, tr["back_y"], fm)
        for yy, want in ((tr["glass_y"] - 0.001, (0, -1, 0)), (tr["glass_y"] + 0.001, (0, 1, 0))):
            hk.rect(E, (lx0, yy, lz0), (lx1, yy, lz0), (lx1, yy, lz1), (lx0, yy, lz1), "Glass", want)
        hk.lattice_bars(E, lx0, lx1, lz0, lz1, tr["lattice"], 0.025, tr["glass_y"] - 0.003, tr["glass_y"] - 0.025, fm)
    zr = (ltop + tr["rail_top"]) / 2
    n = int((x1 - x0 - 0.2) / 0.12)
    for j in range(n + 1):
        sx = x0 + 0.1 + (x1 - x0 - 0.2) * j / n
        hk.block(E, sx - 0.0175, sx + 0.0175, ly - 0.0175, ly, zr - 0.0175, zr + 0.0175, fm, faces="xXyzZ")
    # the two leaves, each its own mesh hinged at the jamb
    wl = (x1 - x0 - 0.01) / 2
    hl = ltop - zf - 0.005
    rows = [tuple(r) for r in dr["panel_rows"]]
    sh, sm = dr["stile"]["hinge"], dr["stile"]["meeting"]
    rg = dr["ring"]
    for k, left in enumerate((True, False)):
        stile = (sh, sm) if left else (sm, sh)
        rx = rg["from_hinge"] if left else wl - rg["from_hinge"]

        def make(B, stile=stile, rx=rx):
            hk.studded_leaf(B, wl, hl, fm, dr["panel_mat"], rows, t=lt, stile=stile, ring=(rx, rg["z"], rg["r"]))
        separate_leaf(E, (x0 + 0.005) if left else (x1 - 0.005), left, wl, hl, ly, zf, lt, make, seed=31 + k)
    # lanterns
    for ln in fr["lanterns"]:
        hk.wall_lantern(E, ln["x"], fr["lantern_wall_y"], ln["z"], ln["out"], fm, "Lamp")
    holes = [door_hole] + win_holes
    hk.wall_core(E, W, 0.0, TOP, t, holes=holes)
    hk.flat_face(I, t, W - t, F0, CEIL, "Plaster", holes=[(x0, x1, zf, ztop)] + win_holes, y=t, inward=True)
    hulls += [hk.box_pts(0.0, x0, 0.0, t, 0.0, TOP), hk.box_pts(x1, W, 0.0, t, 0.0, TOP), hk.box_pts(x0, x1, 0.0, t, ztop, TOP)]

    # --- sides: zones from the front zones' heights, each starting behind the front at its own depth; the gable above
    sd = spec["sides"]
    bands = sorted([(z["z0"], z["z1"], z["side_y"], z) for z in fr["zones"]] + [(g["z0"], g["z1"], g["side_y"], None) for g in fr["gaps"]])
    for side in ("right", "left"):
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        for k, (za, zb, sy, zn) in enumerate(bands):
            xa, xb = (sy, L) if side == "right" else (0.0, L - sy)
            if zn is not None and zn["kind"] == "blocks":
                hk.rubble_face(E, xa, xb, za, zb, sd["mat"], course=zn["course"], length=tuple(sd["length"]), chamfer=sd["chamfer"],
                               jitter=sd["jitter"], seed=101 + 7 * k + (0 if side == "right" else 50))
            else:
                hk.flat_face(E, xa, xb, za, zb, zn["mat"] if zn is not None else sd["mat"], cell=0.6)
        hk.wall_core(E, L, 0.0, TOP, t)
        hk.flat_face(I, t, L - t, F0, CEIL, "Plaster", y=t, inward=True)
        E.pop()
        I.pop()
    for X, nx in ((0.0, -1.0), (W, 1.0)):
        for (ya, za), (yb, zb) in zip(outline, outline[1:]):
            if yb - ya > 1e-6:
                hk.rect(E, (X, ya, TOP), (X, yb, TOP), (X, yb, zb + rf["coping"]["h"]), (X, ya, za + rf["coping"]["h"]), sd["gable_mat"], (nx, 0, 0))
        hulls.append(hk.box_pts(min(X, X - nx * t), max(X, X - nx * t), 0.0, D, 0.0, TOP))
    cp = rf["coping"]
    hk.gable_coping(E, 0.0, cp["w"], outline, cp["h"], cp["mat"], (1, 0, 0))
    hk.gable_coping(E, W - cp["w"], W, outline, cp["h"], cp["mat"], (-1, 0, 0))

    # --- back ------------------------------------------------------------------------------
    bk = spec["back"]
    m, L = hk.facade_frame("back", W, D)
    E.push(m)
    I.push(m)
    hk.rubble_face(E, 0.0, L, 0.0, eave, bk["mat"], course=bk["course"], length=tuple(bk["length"]), chamfer=bk["chamfer"],
                   jitter=bk["jitter"], seed=301)
    hk.wall_core(E, L, 0.0, eave, t)
    hk.flat_face(I, t, L - t, F0, CEIL, "Plaster", y=t, inward=True)
    E.pop()
    I.pop()
    fa = bk["fascia"]
    hk.block(E, 0.0, W, D, D + fa["proud"], eave - fa["drop"], eave + fa["rise"], fa["mat"], faces="xXYzZ")
    hulls.append(hk.box_pts(0.0, W, D - t, D, 0.0, eave))

    # --- roof between the copings ----------------------------------------------------------
    c = cp["w"]
    gy = rf["gutter_y"]
    yt, zt = rf["front_top"]
    hk.rect(E, (c, 0.0, TOP), (W - c, 0.0, TOP), (W - c, gy, TOP), (c, gy, TOP), rf["mat"], (0, 0, 1))
    hk.rect(E, (c, gy, TOP), (W - c, gy, TOP), (W - c, yt, zt), (c, yt, zt), rf["mat"], (0, -(zt - TOP), yt - gy))
    hk.rect(E, (c, yt, zt), (W - c, yt, zt), (W - c, yt, rf["ridge"]), (c, yt, rf["ridge"]), rf["step_mat"], (0, -1, 0))
    hk.rect(E, (c, yt, rf["ridge"]), (W - c, yt, rf["ridge"]), (W - c, D, eave), (c, D, eave), rf["mat"], (0, rf["ridge"] - eave, D - yt))
    lp = rf["lip"]
    hk.block(E, c, W - c, yt - lp["proud"], yt, lp["z0"], rf["ridge"], rf["step_mat"], faces="xXyzZ")
    fl = rf["flashing"]
    hk.block(E, c, W - c, fl["y0"], fl["y1"], TOP, TOP + fl["h"], fl["mat"], faces="yYZ")
    rb = rf["ribs"]
    for xc in rb["x"]:
        hk.slope_rib(E, xc, rb["w"], (gy, TOP), (yt, zt), rb["h"], rb["mat"])
    hulls.append([(x, y, z) for x in (0.0, W) for y, z in ((0.0, CEIL), (0.0, TOP), (gy, TOP), (yt, zt), (yt, CEIL))])
    hulls.append([(x, y, z) for x in (0.0, W) for y, z in ((yt, CEIL), (yt, rf["ridge"]), (D, eave), (D, CEIL))])
    for ch in spec.get("chimneys", []):
        hulls += hk.chimney(E, ch["x0"], ch["x1"], ch["y0"], ch["y1"], ch["z0"], ch["z1"], ch["mat"], ch["cap_mat"], "Clay",
                            pots=ch["pots"], cap_h=ch["cap_h"], pot_h=ch["pot_h"], pot_r=ch["pot_r"])

    # --- inside: one empty room (floor at the threshold, ceiling under the roof) ----------------
    hk.rect(I, (t, t, F0), (W - t, t, F0), (W - t, D - t, F0), (t, D - t, F0), "Timber", (0, 0, 1))
    hk.rect(I, (t, t, CEIL), (W - t, t, CEIL), (W - t, D - t, CEIL), (t, D - t, CEIL), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, F0))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_terrace (house #3 on): a flat-roofed house behind a cornice, a roof railing round the terrace and an
# optional clock cupola on it; a rusticated ground floor (plinth, shop windows, recessed door, fascia, corbels)
# standing a little proud of ashlar upper floors with corner pilasters, bands, windows with architraves, sills and
# hoods on every side; an optional two-storey cast-iron balcony across the front. Inside: one empty shell.
# ---------------------------------------------------------------------------------------------
def terrace_ground(E, I, fac, spec, L, main, hulls):
    """Plinth, plinth cap, rusticated blocks and frieze of one side's ground floor, with its shop windows and door
    (main sides). Returns the holes to leave in the wall core and the inner face, and the door cut for collision."""
    t, F0 = spec["wall"], spec["floor"]
    pl, gr = spec["plinth"], spec["ground"]
    gp = gr["proud"] if main else 0.0
    pp = pl["proud"] if main else pl["side_proud"]
    pext = pl["side_proud"] if main else pl["proud"]            # past the corner, to the neighbouring side's faces
    gext = 0.0 if main else gr["proud"]
    holes_ext, holes_core, cut = [], [], None
    plinth_holes, cap_breaks = [], []
    sh = fac.get("shop")
    for x0, x1 in (sh["windows"] if sh else []):
        zs, zg, z1, ztr = sh["z_sill"], sh["z_glass"], sh["z1"], sh["transom"]
        yr, fr = sh["frame_y"], sh["frame_depth"]
        plinth_holes.append((x0, x1, 0.0, pl["z1"]))
        cap_breaks.append((x0 - 0.04, x1 + 0.04))
        holes_ext.append((x0, x1, 0.0, z1))
        holes_core.append((x0, x1, zg, z1))
        hk.flat_face(E, x0, x1, 0.0, zs, pl["mat"], y=sh["riser_y"])                          # stallriser
        hk.reveal(E, (x0, x1, 0.0, zs), -pp, sh["riser_y"], pl["mat"], sides="lr")
        hk.block(E, x0 - 0.04, x1 + 0.04, -pp - 0.02, yr, zs, zg, sh["sill_mat"], faces="xXyzZ")
        hk.sash_window(E, x0, x1, zg, ztr, "Metal", "Glass", sh["panes"][0], sh["panes"][1], y=yr, depth=fr, fw=sh["fw"], meeting=False)
        hk.sash_window(E, x0, x1, ztr, z1, "Metal", "Glass", sh["transom_panes"], 1, y=yr, depth=fr, fw=sh["fw"], meeting=False)
        hk.reveal(E, (x0, x1, zg, z1), -gp, yr, gr["mat"], sides="lrt")
        hk.reveal(I, (x0, x1, zg, z1), yr + fr, t, "Plaster")
    dr = fac.get("door")
    if dr:
        x0, x1, z1 = dr["x0"], dr["x1"], dr["z1"]
        fw, ly, lt, ltop = dr["frame"], dr["leaf_y"], dr["leaf_t"], dr["leaf_top"]
        pi = dr["pilasters"]
        px0, px1 = x0 - pi["w"], x1 + pi["w"]
        plinth_holes.append((px0, px1, 0.0, pl["z1"]))
        cap_breaks.append((px0, px1))
        holes_ext.append((x0, x1, 0.0, z1))
        holes_core.append((x0, x1, 0.0, z1))
        cut = (x0, x1, z1)
        hk.block(E, px0, x0, -pp, -gp, 0.0, pi["z1"], pi["mat"], faces="xyZ")
        hk.block(E, x1, px1, -pp, -gp, 0.0, pi["z1"], pi["mat"], faces="XyZ")
        hk.reveal(E, (x0, x1, 0.0, z1), -pp, ly - 0.04, dr["reveal_mat"], sides="lr")
        hk.reveal(E, (x0, x1, 0.0, z1), -gp, ly - 0.04, dr["reveal_mat"], sides="t")
        hk.reveal(I, (x0, x1, F0, z1), ly + lt, t, "Plaster", sides="lrt")
        for st in dr["steps"]:
            hk.block(E, x0, x1, st["y0"], ly + lt, st["z0"], st["z1"], st["mat"], faces=("xXyZ" if st["y0"] < -pp else "yZ"))
            hulls.append(hk.box_pts(x0, x1, st["y0"], ly + lt, st["z0"] - 0.1, st["z1"]))
        for a, b in ((x0, x0 + fw), (x1 - fw, x1)):                                              # jambs
            hk.block(E, a, b, ly - 0.04, ly + lt, F0, ltop, dr["frame_mat"], faces="xXy")
        hk.sash_window(E, x0, x1, ltop, z1, "Metal", "Glass", dr["transom_panes"], 1, y=ly - 0.04, depth=lt + 0.04, fw=fw, meeting=False)
        w, h = x1 - x0 - 2 * fw - 0.01, ltop - F0 - 0.005
        left = dr.get("hinge", "left") == "left"

        def make(B, w=w, h=h):
            hk.panel_leaf(B, w, h, "Door", t=lt, cols=dr["cols"], rows=dr["rows"])
        separate_leaf(E, (x0 + fw + 0.005) if left else (x1 - fw - 0.005), left, w, h, ly, F0, lt, make, seed=31 + len(DOORS))
    fa = fac.get("fascia")
    if fa:
        hk.block(E, fa["x0"], fa["x1"], -gp - fa["proud"], -gp, fa["z0"], fa["z1"], fa["mat"], faces="xXyzZ")
    co = fac.get("corbels")
    if co:
        for cx in co["x"]:
            hk.console(E, cx, -gp, co["z_top"], co["mat"], out=co["out"], height=co["height"], width=co["width"])
    # plinth, its cap, the rusticated blocks and the frieze
    hk.flat_face(E, -pext, L + pext, 0.0, pl["z1"], pl["mat"], holes=plinth_holes, y=-pp, cell=0.6)
    pieces = [(-pext, L + pext, 0.0, 1.0)]
    for bx0, bx1 in cap_breaks:
        pieces = hk._minus(pieces, (bx0, bx1, -1.0, 2.0))
    for a, b, _c, _d in pieces:
        hk.moulding_run(E, a, b, pl["z1"], [[0, 0], [pp - gp, 0], [0, pl["cap_h"]]], pl["mat"], y=-gp, hub_v=0.02)
    E.push(gk.translate(0.0, -gp, 0.0))
    hk.rubble_face(E, -gext, L + gext, gr["z0"], gr["z1"], gr["mat"], holes=holes_ext, course=gr["course"], length=tuple(gr["length"]),
                   chamfer=gr["chamfer"], jitter=gr["jitter"], seed=7 + int(L * 10) + (0 if main else 3))
    E.pop()
    fz = gr["frieze"]
    hk.flat_face(E, -gext, L + gext, gr["z1"], fz["z1"], fz["mat"], holes=holes_ext, y=-gp, cell=0.6)
    return holes_core, cut


def terrace_windows(E, I, fac, spec):
    """The upper windows of one side: frame and glass, reveals, architrave, sill, hood. Returns their holes."""
    ws, t = spec["window_style"], spec["wall"]
    holes = []
    for z0, z1 in spec["storeys"]:
        for x0, x1 in fac.get("windows", []):
            holes.append((x0, x1, z0, z1))
            rec = ws["rec"]
            c, r = ws["panes"]
            hk.sash_window(E, x0, x1, z0, z1, ws["frame"], ws["glass"], c, r, y=rec, depth=ws["depth"])
            hk.reveal(E, (x0, x1, z0, z1), 0.0, rec, ws["reveal_mat"], sides="lrt")
            hk.reveal(I, (x0, x1, z0, z1), rec, t, "Plaster")
            ar = ws["architrave"]
            hk.frame_run(E, x0, x1, z0, z1, ar["profile"], ar["mat"])
            aw = max(p[0] for p in ar["profile"])
            s = ws["sill"]
            hk.sill(E, x0 - s["ext"], x1 + s["ext"], z0, s["mat"], h=s["h"], proud=s["proud"], back=rec)
            hd = ws["hood"]
            hk.moulding_run(E, x0 - hd["ext"], x1 + hd["ext"], z1 + aw, hd["profile"], hd["mat"])
    return holes


def terrace_pilasters(E, spec, L, main):
    """Corner pilasters of one side, with a sunk panel per storey; on the front and back they wrap the corners."""
    pi = spec["pilasters"]
    pr, w = pi["proud"], pi["w"]
    e = pr if main else 0.0
    for px0, px1, (p0, p1) in ((-e, w, pi["panel"]), (L - w, L + e, (L - pi["panel"][1], L - pi["panel"][0]))):
        panels = [(p0, p1, za, zb) for za, zb in pi["panels_z"]]
        hk.flat_face(E, px0, px1, pi["z0"], pi["z1"], pi["mat"], holes=panels, y=-pr)
        for pn in panels:
            hk.flat_face(E, *pn, pi["mat"], y=-pr + pi["sunk"])
            hk.reveal(E, pn, -pr, -pr + pi["sunk"], pi["mat"])
        hk.block(E, px0, px1, -pr, 0.0, pi["z0"], pi["z1"], pi["mat"], faces="xXz")


def terrace_balcony(E, spec, hulls):
    """Two-storey cast-iron balcony across the front (house coordinates = front wall coordinates)."""
    bc = spec["balcony"]
    mat, smat = bc["mat"], bc["slab_mat"]
    bx0, bx1, yf, yr = bc["x0"], bc["x1"], bc["front"], bc["rail_y"]
    posts, nh = bc["posts"], bc["nosing"]
    for lv in bc["levels"]:
        za, zb = lv["slab"]
        hk.block(E, bx0, bx1, yf, 0.0, za + nh, zb, smat, faces="xXyzZ")
        hk.block(E, bx0 + 0.06, bx1 - 0.06, yf + 0.06, 0.0, za, za + nh, smat, faces="xXyz")
        hulls.append(hk.box_pts(bx0, bx1, yf, 0.0, za, zb))
    (la, lb), (ua, ub) = bc["levels"][0]["slab"], bc["levels"][1]["slab"]
    for k, lv in enumerate(bc["levels"]):
        zs, zt = lv["slab"][1], lv["rail_top"]
        pw = 0.13 if k == 0 else 0.10
        for pa, pb in zip(posts, posts[1:]):                                                  # front run
            hk.lace_panel(E, pa + pw / 2, pb - pw / 2, zs + 0.04, zt, yr, mat, pitch=bc["lace_pitch"])
        for px, rz in ((posts[0], -90.0), (posts[-1], 90.0)):                                 # returns to the wall
            E.push(gk.translate(px, 0.0, 0.0) @ gk.rot_z(rz))
            a, b = (0.03, -yr - pw / 2) if rz < 0 else (yr + pw / 2, -0.03)
            hk.lace_panel(E, a, b, zs + 0.04, zt, 0.0, mat, pitch=bc["lace_pitch"])
            E.pop()
    for px in posts:
        hk.cast_column(E, px, yr, lb, ua, mat, r=bc["column_r"])
        hk.newel(E, px, yr, ub, bc["newel_top"], mat)
    for pa, pb in zip(posts, posts[1:]):
        hk.spandrel(E, pa + 0.07, pb - 0.07, bc["spring"], bc["crown"], ua, yr, mat)
    for px, rz in ((posts[0], -90.0), (posts[-1], 90.0)):
        E.push(gk.translate(px, 0.0, 0.0) @ gk.rot_z(rz))
        a, b = (0.03, -yr - 0.07) if rz < 0 else (yr + 0.07, -0.03)
        hk.spandrel(E, a, b, bc["spring"] + 0.3, bc["crown"], ua, 0.0, mat)
        E.pop()


def terrace_cupola(E, spec, hulls):
    """Square clock cupola on the roof: base, walls with corner piers, a band, the clock on the front and plain
    dials on the other faces, a cornice, a tiled pyramid with an arched dormer, iron pinnacles and a finial."""
    cu = spec["cupola"]
    cx, cy, F, z0, z1 = cu["cx"], cu["cy"], cu["face"], cu["z0"], cu["z1"]
    mat, trim = cu["mat"], cu["trim"]
    P, pw = cu["pier"]["half"], cu["pier"]["w"]
    corners = {"front": ((cx - F, cy - F), (cx + F, cy - F)), "right": ((cx + F, cy - F), (cx + F, cy + F)),
               "back": ((cx + F, cy + F), (cx - F, cy + F)), "left": ((cx - F, cy + F), (cx - F, cy - F))}
    cl = cu["clock"]
    for side, (p0, p1) in corners.items():
        m, L = gk.wall_frame(p0, p1)
        E.push(m)
        hk.flat_face(E, 0.0, L, z0, z1, mat, cell=0.6)
        if side == "front":
            hk.clock_dial(E, L / 2, cl["cz"], cl["half"], cl["r"], trim, cl["ring"], "Dial", cl["marks"], brass=cl["marks"], rosette=trim, centre="Dial")
        else:
            hk.blank_dial(E, L / 2, cl["cz"], cl["half"], cl["r"], trim, trim, "Dial")
        E.pop()
    for sx in (-1, 1):
        for sy in (-1, 1):
            xa, xb = sorted((cx + sx * (P - pw), cx + sx * P))
            ya, yb = sorted((cy + sy * (P - pw), cy + sy * P))
            hk.block(E, xa, xb, ya, yb, z0, z1, mat, faces="xXyYZ")
    for (za, zb), out in ((cu["base"]["z"], cu["base"]["out"]), (cu["band"]["z"], cu["band"]["out"])):
        h = P + out                                   # round the piers
        hk.block(E, cx - h, cx + h, cy - h, cy + h, za, zb, trim, faces="xXyYzZ")
    E.sweep([(cx - F, cy - F, z1), (cx + F, cy - F, z1), (cx + F, cy + F, z1), (cx - F, cy + F, z1)], [cu["cornice"]], trim, closed=True)
    ztop = z1 + max(v for _u, v in cu["cornice"])
    rf = cu["roof"]
    hk.pyramid_roof(E, cx, cy, rf["half"], ztop, rf["apex"], rf["mat"], pitch=rf["pitch"])
    dm = cu["dormer"]
    hk.arched_block(E, cx, dm["w"], ztop, dm["z_spring"], cy - dm["front"], cy - dm["back"], trim)
    E.push(gk.translate(cx, cy - dm["front"], dm["rosette_z"]) @ gk.FACE_OUT)
    gk.rosette(E, trim, dm["w"] * 0.26, 0.05, petals=8, layers=2)
    E.pop()
    pn = cu["pinnacles"]
    for sx in (-1, 1):
        for sy in (-1, 1):
            E.push(gk.translate(cx + sx * pn["at"], cy + sy * pn["at"], ztop))
            gk.obelisk(E, pn["mat"], pn["h"], half=pn["half"])
            E.pop()
    fn = cu["finial"]
    hk.iron_finial(E, cx, cy, rf["apex"] - 0.12, fn["top"], fn["mat"])
    hulls.append(hk.box_pts(cx - P, cx + P, cy - P, cy + P, z0, ztop))
    hulls.append([(cx + sx * rf["half"], cy + sy * rf["half"], ztop) for sx in (-1, 1) for sy in (-1, 1)] + [(cx, cy, rf["apex"])])


def build_townhouse_terrace(spec):
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    F0, CEIL, TOP = spec["floor"], spec["ceiling"], spec["top"]
    up = spec["upper"]
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    for k, side in enumerate(("front", "right", "back", "left")):
        fac = spec["facades"].get(side, {})
        main = side in ("front", "back")
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        ground_holes, cut = terrace_ground(E, I, fac, spec, L, main, hulls)
        win = terrace_windows(E, I, fac, spec)
        hk.rubble_face(E, 0.0, L, up["z0"], up["z1"], up["mat"], holes=win, course=up["course"], length=tuple(up["length"]),
                       chamfer=up["chamfer"], jitter=up["jitter"], seed=41 + 17 * k)
        terrace_pilasters(E, spec, L, main)
        holes = ground_holes + win
        hk.wall_core(E, L, 0.0, TOP, t, holes=holes)
        hk.flat_face(I, t, L - t, F0, CEIL, "Plaster", holes=holes, y=t, inward=True)
        if cut:
            dx0, dx1, dz = cut
            hulls += [xf(E, hk.box_pts(0.0, dx0, 0.0, t, 0.0, TOP)), xf(E, hk.box_pts(dx1, L, 0.0, t, 0.0, TOP)),
                      xf(E, hk.box_pts(dx0, dx1, 0.0, t, dz, TOP))]
        else:
            hulls.append(xf(E, hk.box_pts(0.0, L, 0.0, t, 0.0, TOP)))
        # cornice dentils
        cn = spec["cornice"]
        dt = cn["dentils"]
        n = max(1, int(L / dt["pitch"]))
        for j in range(n):
            xc = (j + 0.5) * L / n
            hk.block(E, xc - dt["w"] / 2, xc + dt["w"] / 2, -dt["u1"], -dt["u0"], cn["z"] + dt["v0"], cn["z"] + dt["v1"], cn["mat"], faces="xXyz")
        # roof railing, set in from the cornice's inner edge
        rr = spec["roof"]["railing"]
        hk.iron_railing(E, rr["inset"], L - rr["inset"], rr["inset"], TOP, rr["h"], rr["mat"], pitch=rr["pitch"])
        hulls.append(xf(E, hk.box_pts(rr["inset"], L - rr["inset"], rr["inset"] - 0.03, rr["inset"] + 0.03, TOP, TOP + rr["h"])))
        E.pop()
        I.pop()
    for b in spec["bands"]:
        hk.band_loop(E, W, D, b["z0"], b["z1"], b["proud"], b["mat"])
    cn = spec["cornice"]
    E.sweep([(x, y, cn["z"]) for x, y in hk.plan_loop(W, D)], [cn["profile"]], cn["mat"], closed=True)
    rf = spec["roof"]
    hk.rect(E, (0.0, 0.0, TOP), (W, 0.0, TOP), (W, D, TOP), (0.0, D, TOP), rf["mat"], (0, 0, 1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, CEIL, TOP))
    rr = rf["railing"]
    po = rr["post"]
    for x in (rr["inset"], W - rr["inset"]):
        for y in (rr["inset"], D - rr["inset"]):
            hw = po["w"] / 2
            hk.block(E, x - hw, x + hw, y - hw, y + hw, TOP, TOP + po["h"], rr["mat"], faces="xXyYZ")
            hk.block(E, x - hw - 0.03, x + hw + 0.03, y - hw - 0.03, y + hw + 0.03, TOP + po["h"], TOP + po["h"] + 0.06, rr["mat"])
            E.push(gk.translate(x, y, TOP + po["h"] + 0.06))
            gk.obelisk(E, rr["mat"], po["spire"], half=hw * 0.8)
            E.pop()
    if spec.get("balcony"):
        terrace_balcony(E, spec, hulls)
    if spec.get("cupola"):
        terrace_cupola(E, spec, hulls)
    # inside: one empty room
    hk.rect(I, (t, t, F0), (W - t, t, F0), (W - t, D - t, F0), (t, D - t, F0), "Timber", (0, 0, 1))
    hk.rect(I, (t, t, CEIL), (W - t, t, CEIL), (W - t, D - t, CEIL), (t, D - t, CEIL), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, F0))
    return E, I, hulls


BUILDERS = {"townhouse_flat": build_townhouse_flat, "townhouse_gable": build_townhouse_gable, "townhouse_terrace": build_townhouse_terrace}


# ---------------------------------------------------------------------------------------------
# audit eyes and planes, renders
# ---------------------------------------------------------------------------------------------
def audit_setup_gable(spec):
    """Outside eyes only (the door is closed, nobody stands inside): street, both sides, the lower town, above."""
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    top = spec["front"]["top"] - 0.05
    outline, eave = gable_outline(spec)
    street = [("lane left", (1.0, -4.5, 1.7)), ("lane door", (4.0, -4.5, 1.7)), ("lane right", (7.0, -4.5, 1.7)),
              ("lane high", (4.0, -6.0, 9.0)), ("lane low left", (-1.5, -2.5, 1.0))]
    return [("front", (0.0, t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.03, street),
            ("right", (W - t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.04,
             [("plaza", (W + 6.0, D / 2, 1.7)), ("plaza front", (W + 4.0, -3.0, 6.0))]),
            ("left", (t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.04,
             [("left side", (-6.0, D / 2, 4.0)), ("left high", (-5.0, -2.0, 18.0))]),
            ("back", (0.0, D - t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, eave - 0.05), 0.04,
             [("lower town", (4.0, D + 9.0, 6.0)), ("lower town left", (-2.0, D + 7.0, 12.0))]),
            ("roof front", (0.0, 1.2, 0.0), (1, 0, 0), (0.05, W - 0.05, 14.0, 15.7), 0.04,
             [("above front", (W / 2, -4.0, 22.0)), ("above", (W / 2, D / 2, 26.0))]),
            ("roof back", (0.0, D / 2 + 0.6, 0.0), (1, 0, 0), (0.05, W - 0.05, 14.0, 15.9), 0.04,
             [("above back", (W / 2, D + 5.0, 22.0)), ("above", (W / 2, D / 2, 26.0))])]


def shots_gable(spec):
    W, D = spec["width"], spec["depth"]
    H = spec["roof"]["ridge"]
    return [("iso", (W + 9.0, -14.0, 12.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
            ("front_eye", (W / 2 - 1.0, -5.0, 1.7), (W / 2, 0.0, 6.0), 18, 1200, 1500),
            ("door_close", (4.0, -3.2, 2.2), (4.0, 0.0, 3.0), 24, 1200, 1200),
            ("roof", (W + 3.0, -4.0, H + 6.0), (W / 2, D / 2, H - 1.0), 28, 1400, 1000),
            ("back", (W / 2 - 4.0, D + 16.0, 6.0), (W / 2, D / 2, H / 2), 35, 1000, 1500)]


def audit_setup_terrace(spec):
    """Outside eyes only: street (through the balcony iron), both sides, the lower town, above the roof."""
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    top = spec["top"] - 0.05
    street = [("lane left", (1.0, -4.5, 1.7)), ("lane door", (4.0, -4.5, 1.7)), ("lane right", (7.0, -4.5, 1.7)),
              ("lane high", (4.0, -6.0, 9.0)), ("lane low left", (-1.5, -2.5, 1.0))]
    return [("front", (0.0, t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.03, street),
            ("right", (W - t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.04,
             [("plaza", (W + 6.0, D / 2, 1.7)), ("plaza front", (W + 4.0, -3.0, 6.0))]),
            ("left", (t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.04,
             [("left side", (-6.0, D / 2, 4.0)), ("left high", (-5.0, -2.0, 18.0))]),
            ("back", (0.0, D - t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.04,
             [("lower town", (4.0, D + 9.0, 6.0)), ("lower town left", (-2.0, D + 7.0, 12.0))]),
            ("roof", (0.0, D / 2, 0.0), (1, 0, 0), (0.05, W - 0.05, 0.05, top), 0.05,
             [("above", (W / 2 + 2.5, D / 2 + 2.0, top + 12.0)), ("above front", (W / 2, -4.0, top + 6.0))])]


def shots_terrace(spec):
    W, D = spec["width"], spec["depth"]
    H = spec["cupola"]["finial"]["top"] if spec.get("cupola") else spec["top"] + 1.5
    door = spec["facades"]["front"]["door"]
    dx = (door["x0"] + door["x1"]) / 2
    return [("iso", (W + 11.0, -17.0, 14.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
            ("front_eye", (W / 2 - 1.0, -6.0, 1.7), (W / 2, 0.0, 7.0), 16, 1200, 1500),
            ("door_close", (dx, -3.6, 2.0), (dx, 0.0, 2.2), 24, 1200, 1200),
            ("roof", (W + 4.0, -5.0, H + 2.0), (W / 2, D / 2, spec["top"] + 3.0), 28, 1400, 1100),
            ("back", (W / 2 - 4.0, D + 18.0, 7.0), (W / 2, D / 2, H / 2), 35, 1000, 1500)]


def audit_setup(spec):
    if spec["type"] == "townhouse_gable":
        return audit_setup_gable(spec)
    if spec["type"] == "townhouse_terrace":
        return audit_setup_terrace(spec)
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    floors, top = spec["floors"], spec["cornice"]["z"] + gk.ENTABLATURE_H - 0.05
    st = spec["stair"]
    out = [("front", (0.0, t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.03,
            [("lane left", (0.6, -4.5, 1.7)), ("lane door", (3.4, -4.5, 1.7)), ("lane right", (5.6, -4.5, 1.7)), ("lane high", (3.0, -4.8, 9.0))]),
           ("right", (W - t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.03,
            [("plaza", (W + 6.0, D / 2, 1.7)), ("plaza front", (W + 4.0, -3.0, 1.7))]),
           ("back", (0.0, D - t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.03,
            [("lower town", (3.0, D + 9.0, 6.0)), ("lower town left", (-2.0, D + 7.0, 12.0))]),
           ("left", (t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.03, [("left side", (-6.0, D / 2, 4.0))]),
           ("roof", (0.0, D / 2, 0.0), (1, 0, 0), (0.05, W - 0.05, 0.05, top), 0.05,
            [("above", (W / 2, D / 2, top + 9.0)), ("above front", (W / 2, -4.0, top + 6.0))])]
    eyes = []
    for k, F in enumerate(floors):
        eyes.append((f"room {k}", (W / 2, 1.8, F + 1.7)))
        eyes.append((f"landing {k}", (st["x_land"] + 0.6, D - t - 0.6, F + 1.7)))
        if k + 1 < len(floors):
            eyes.append((f"half landing {k}", ((t + st["x_half"]) / 2, D - t - 1.2, (F + floors[k + 1]) / 2 + 1.7)))
    for name, origin, udir, (u0, u1) in (("front", (0.0, t / 2, 0.0), (1, 0, 0), (0.02, W - 0.02)),
                                         ("back", (0.0, D - t / 2, 0.0), (1, 0, 0), (0.02, W - 0.02)),
                                         ("right", (W - t / 2, 0.0, 0.0), (0, 1, 0), (0.02, D - 0.02)),
                                         ("left", (t / 2, 0.0, 0.0), (0, 1, 0), (0.02, D - 0.02))):
        z0, z1 = floors[0] + 0.01, spec["ceiling"] - 0.01
        rects = [(u0, u1, z0, z1)]
        for op in spec["facades"].get(name, {}).get("openings", []):
            if op["kind"] == "door":           # an open door: from inside one sees the street through it, as one should
                rects = hk._minus(rects, (op["x0"] - 0.02, op["x1"] + 0.02, 0.0, op["z1"] - op.get("transom", 0.0)))
        for k, (a, b, c, d) in enumerate(rects):
            out.append((f"inside {name} {k}", origin, udir, (a, b, c, d), 0.06, eyes))
    return out


def shots(spec):
    if spec["type"] == "townhouse_gable":
        return shots_gable(spec)
    if spec["type"] == "townhouse_terrace":
        return shots_terrace(spec)
    W, D = spec["width"], spec["depth"]
    H = spec["cornice"]["z"] + gk.ENTABLATURE_H
    F = spec["floors"]
    st = spec["stair"]
    s = [("iso", (W + 9.0, -14.0, 12.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
         ("front_eye", (W / 2 - 1.0, -4.6, 1.7), (W / 2, 0.0, 6.0), 18, 1200, 1500),
         ("back", (W / 2 - 4.0, D + 16.0, 6.0), (W / 2, D / 2, H / 2), 35, 1000, 1500),
         ("door_close", (3.4, -1.8, 1.6), (3.4, 0.0, 1.8), 24, 1200, 1200),
         ("clock_close", (3.0, -2.0, 13.0), (3.0, 0.0, 13.3), 22, 1200, 1200),
         ("roof", (W + 3.0, -4.0, H + 6.0), (W / 2, D / 2, H), 28, 1400, 1000)]
    for k, f in enumerate(F):
        s.append((f"inside_{k}", (0.7, 0.7, f + 1.65), (st["x_land"], D - 1.2, f + 1.2), 14, 1400, 1000))
    return s


def render(objs, out_dir):
    from mathutils import Vector
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    sh = scene.display.shading
    sh.light, sh.color_type = 'STUDIO', 'MATERIAL'
    sh.show_cavity, sh.cavity_type = True, 'BOTH'
    sh.cavity_ridge_factor, sh.cavity_valley_factor = 1.2, 1.4
    sh.show_shadows, sh.show_backface_culling, sh.show_object_outline = False, True, False
    scene.world = bpy.data.worlds.new("w")
    cd = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cd.clip_start, cd.clip_end = 0.05, 600
    os.makedirs(out_dir, exist_ok=True)
    for tag, pos, at, lens, rx, ry in shots(spec):
        cd.type = 'PERSP'
        cam.location = Vector(pos)
        cam.rotation_euler = (Vector(at) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()
        cd.lens = lens
        scene.render.resolution_x, scene.render.resolution_y = rx, ry
        scene.render.filepath = os.path.join(out_dir, f"{FOLDER}_{tag}.png")
        bpy.ops.render.render(write_still=True)
    mf = spec.get("meshy_front")
    if mf and os.path.exists(mf["file"]):
        import OpenImageIO as oiio
        ref = oiio.ImageBuf(mf["file"])
        w, h = ref.spec().width, ref.spec().height
        ppm = mf["ppm"]
        cd.type = 'ORTHO'
        cd.sensor_fit = 'VERTICAL' if h >= w else 'HORIZONTAL'
        cd.ortho_scale = max(w, h) / ppm
        cx = (w / 2 - mf["col0"]) / ppm
        cz = (mf["row0"] - h / 2) / ppm
        cam.location = Vector((cx, -60.0, cz))
        cam.rotation_euler = Vector((0, 1, 0)).to_track_quat('-Z', 'Y').to_euler()
        sh.light = 'FLAT'
        scene.render.resolution_x, scene.render.resolution_y = w, h
        path = os.path.join(out_dir, f"{FOLDER}_front_ortho.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        sh.light = 'STUDIO'
        new = oiio.ImageBuf(path)
        both = oiio.ImageBuf(oiio.ImageSpec(w * 2 + 10, h, 3, oiio.UINT8))
        oiio.ImageBufAlgo.fill(both, (0.1, 0.1, 0.1))
        oiio.ImageBufAlgo.paste(both, 0, 0, 0, 0, oiio.ImageBufAlgo.channels(ref, (0, 1, 2)))
        oiio.ImageBufAlgo.paste(both, w + 10, 0, 0, 0, oiio.ImageBufAlgo.channels(new, (0, 1, 2)))
        both.write(os.path.join(out_dir, f"{FOLDER}_compare_front.png"))
    print("RENDERED", out_dir)


def clear_dirt(obj):
    col = obj.data.color_attributes["Col"]
    c = np.empty(len(obj.data.loops) * 4, dtype=np.float32)
    col.data.foreach_get("color_srgb", c)
    c = c.reshape(-1, 4)
    c[:, 1] = 0.0
    col.data.foreach_set("color_srgb", c.ravel())


def to_object_named(B, name):
    obj = B.to_object(name, hk.COLORS)
    return obj


def hull_object(points, name):
    import bmesh
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def export(objs, path):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y',
        mesh_smooth_type='FACE', colors_type='SRGB', use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    E, I, hulls = BUILDERS[spec["type"]](spec)
    ext = to_object_named(E, f"SM_House_{FOLDER}_Ext")
    inn = to_object_named(I, f"SM_House_{FOLDER}_Int")
    if not args.no_bake:
        print(f"CAVITY ext {gk.bake_cavity(ext):.3f}")
        clear_dirt(inn)                 # inside: big plain quads whose corners all sit in corners; keep them clean
        print(f"WEATHER {gk.bake_weather(ext, stone=('Stone', 'Trim', 'Brick', 'Gauged'))}")
    shell = spec.get("interior") == "shell"
    if shell:                           # one empty room: its faces go into the Ext mesh (no Int mesh)
        bpy.ops.object.select_all(action='DESELECT')
        ext.select_set(True)
        inn.select_set(True)
        bpy.context.view_layer.objects.active = ext
        bpy.ops.object.join()
        inn = None
    ucx = [hull_object(h, f"UCX_SM_House_{FOLDER}_Ext_{i:02d}") for i, h in enumerate(hulls)]
    for ob in ucx:
        ob.display_type = 'WIRE'
        ob.hide_render = True
    for ob in [o for o in (ext, inn) if o is not None]:
        co = np.empty(len(ob.data.vertices) * 3, dtype=np.float32)
        ob.data.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        print(f"BUILT {ob.name}: {len(ob.data.polygons)} faces, bounds {co.min(0).round(3)} .. {co.max(0).round(3)}")
    print(f"BUILT {len(ucx)} collision hulls")
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, f"{FOLDER}.blend"))
    export([ext] + ucx, os.path.join(OUT, f"SM_House_{FOLDER}_Ext.fbx"))
    if inn is not None:
        export([inn], os.path.join(OUT, f"SM_House_{FOLDER}_Int.fbx"))
    # separate door leaves: one FBX each (hinge space, with its own UCX box) and doors.json for import_house_ue.py
    doors, previews = [], []
    for k, d in enumerate(DOORS):
        name = f"SM_House_{FOLDER}_Door" + ("" if k == 0 else f"_{k}")
        ob = to_object_named(d["builder"], name)
        if not args.no_bake:
            gk.bake_cavity(ob)
        hull = hull_object(d["hull"], f"UCX_{name}_00")
        hull.display_type, hull.hide_render = 'WIRE', True
        export([ob, hull], os.path.join(OUT, name + ".fbx"))
        ob.hide_render = True             # it sits at the origin in hinge space; the renders use the house-space preview
        hx, hy, hz = d["hinge"]
        # Unreal: mesh y = -Blender y, so the component's yaw is minus the facade's turn; opening inwards is a further
        # -90 (left hinge) or +90 (right hinge) degrees of yaw on the Door component
        doors.append({"mesh": name, "component": name.replace(f"SM_House_{FOLDER}_", ""), "location_cm": [100 * hx, -100 * hy, 100 * hz],
                      "yaw": -d["facade_yaw"], "open_yaw_delta": -90.0 if d["left"] else 90.0})
        previews.append(d["preview"].to_object(name + "_preview", hk.COLORS))
    with open(os.path.join(OUT, "doors.json"), "w", encoding="utf-8") as f:
        json.dump(doors, f, indent=1)
    if doors:
        print("DOORS", doors)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, f"{FOLDER}.blend"))
    print("EXPORTED", OUT)
    if args.audit:
        total = 0
        for name, origin, udir, (u0, u1, z0, z1), step, eyes in audit_setup(spec):
            print(f"AUDIT plane '{name}'")
            # the grid is moved off round numbers by under 2 mm: a sight line aimed exactly at the edge of an opening
            # grazes coincident edges (a ray tracer's artefact, not a gap); a real gap still shows on its neighbours
            total += gk.audit_plane([o for o in (ext, inn) if o is not None] + previews, origin, udir, u0 + 0.0013, u1, z0 + 0.0017, z1, eyes, step=step)
        print(f"AUDIT total see-through rays: {total}")
    if args.renders:
        render([o for o in (ext, inn) if o is not None] + previews, args.renders)


main()
