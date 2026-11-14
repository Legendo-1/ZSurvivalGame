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


# ---------------------------------------------------------------------------------------------
# townhouse_mansard (house #4 on): rubble walls with quoins and bands under a gambrel (mansard) roof between two
# stone gables, gable windows, gabled dormers on the front slope, a timber shopfront (pilasters, shop windows with
# stallrisers, a glazed shop door, fascia, consoles, cornice) and casement windows with sills and lintels. One
# empty shell inside.
# ---------------------------------------------------------------------------------------------
def mansard_profile(spec):
    """The roof's top line [(y, z), ...] from the front eave over the ridge to the back eave: the spec's front points,
    the ridge at D/2 and the front points mirrored to the back."""
    D, rf = spec["depth"], spec["roof"]
    front = [tuple(p) for p in rf["profile"]]
    return front + [(D / 2, rf["ridge"])] + [(D - y, z) for y, z in reversed(front)]


def roof_z(prof, y):
    ys, zs = zip(*prof)
    return float(np.interp(y, ys, zs))


def mansard_windows(E, I, fac, spec, t):
    """Casement windows on the upper storeys of one side: frame and glass, reveals, flat architrave, sill, lintel."""
    ws = spec["window_style"]
    holes = []
    for z0, z1, panes in spec["storeys"]:
        for x0, x1 in fac.get("windows", []):
            holes.append((x0, x1, z0, z1))
            rec = ws["rec"]
            hk.sash_window(E, x0, x1, z0, z1, ws["frame"], ws["glass"], panes[0], panes[1], y=rec, depth=ws["depth"], fw=ws["fw"], meeting=False)
            hk.reveal(E, (x0, x1, z0, z1), 0.0, rec, ws["reveal_mat"], sides="lrt")
            hk.reveal(I, (x0, x1, z0, z1), rec, t, "Plaster")
            ar = ws["architrave"]
            hk.frame_run(E, x0, x1, z0, z1, ar["profile"], ar["mat"])
            s = ws["sill"]
            hk.sill(E, x0 - s["ext"], x1 + s["ext"], z0 - s["drop"], s["mat"], h=s["h"], proud=s["proud"], back=rec)
            hk.block(E, x0, x1, 0.0, rec, z0 - s["drop"], z0, s["mat"], faces="Z")                  # the sill's top under the frame
            ln = ws["lintel"]
            hk.lintel(E, x0 - ln["ext"], x1 + ln["ext"], z1 + ln["gap"], z1 + ln["gap"] + ln["h"], ln["mat"], proud=ln["proud"])
    return holes


def mansard_shop(E, I, spec, t, F0):
    """The timber shopfront across the front ground floor (wall coordinates = house coordinates). Returns the holes it
    leaves in the wall core and inner face, and the door cut (x0, x1, z) for collision."""
    sp = spec["shop"]
    mat = sp["mat"]
    pf, fy, fd = sp["pilasters"]["front"], sp["frame_y"], sp["frame_depth"]
    holes = []
    for px0, px1 in sp["pilasters"]["x"]:
        hk.block(E, px0, px1, pf, fy, 0.0, sp["pilasters"]["z1"], mat, faces="xXy")
    for win in sp["windows"]:
        x0, x1 = win["x"]
        holes.append((x0, x1, sp["z_glass"], sp["z1"]))
        hk.sash_window(E, x0, x1, sp["z_glass"], sp["z1"], "Metal", "Glass", win["panes"][0], win["panes"][1], y=fy, depth=fd, fw=sp["fw"],
                       meeting=False)
        hk.reveal(I, (x0, x1, sp["z_glass"], sp["z1"]), fy + fd, t, "Plaster")
        hk.block(E, x0, x1, sp["sill_front"], fy, sp["z_sill"], sp["z_glass"], mat, faces="yzZ")            # sill
        rs = sp["riser"]
        m = rs["margin"]
        panel = (x0 + m, x1 - m, m, sp["z_sill"] - m)
        hk.flat_face(E, x0, x1, 0.0, sp["z_sill"], mat, holes=[panel], y=rs["y"])
        hk.flat_face(E, *panel, mat, y=rs["y"] + rs["sunk"])
        hk.reveal(E, panel, rs["y"], rs["y"] + rs["sunk"], mat)
    # the door: the whole bay between two pilasters, a flush head panel, thin jambs, threshold and a glazed leaf
    dr = sp["door"]
    x0, x1, zh, hy = dr["x0"], dr["x1"], dr["head"], dr["head_y"]
    jw, ly, lt = dr["jamb"], dr["leaf_y"], dr["leaf_t"]
    hk.flat_face(E, x0, x1, zh, sp["z1"], mat, y=hy)
    hk.reveal(E, (x0, x1, 0.0, zh), fy, ly, mat, sides="lr")
    hk.reveal(E, (x0, x1, 0.0, zh), hy, ly, mat, sides="t")
    hk.reveal(I, (x0, x1, F0, zh), ly + lt, t, "Plaster", sides="lrt")
    for a, b in ((x0, x0 + jw), (x1 - jw, x1)):
        hk.block(E, a, b, ly - 0.06, ly + lt, F0, zh, mat, faces="xXy")
    hk.block(E, x0, x1, pf, t, 0.0, F0, dr["threshold"], faces="xXyZ")
    w, h = x1 - x0 - 2 * jw - 0.01, zh - F0 - 0.005
    left = dr.get("hinge", "left") == "left"

    def make(B, w=w, h=h):
        hk.glazed_leaf(B, w, h, "Door", "Glass", "Door", t=lt, light=tuple(dr["light"]), grid=tuple(dr["grid"]), panel_z=tuple(dr["panels"]))
    separate_leaf(E, (x0 + jw + 0.005) if left else (x1 - jw - 0.005), left, w, h, ly, F0, lt, make, seed=31 + len(DOORS))
    holes.append((x0, x1, 0.0, zh))
    # fascia, consoles, cornice
    fa = sp["fascia"]
    hk.block(E, fa["x0"], fa["x1"], fa["front"], fy, fa["z0"], fa["z1"], mat, faces="xXyz")
    co = sp["consoles"]
    for cx in co["x"]:
        hk.console(E, cx, fa["front"], fa["z1"], mat, out=co["out"], height=co["height"], width=co["width"])
    cn = sp["cornice"]
    hk.moulding_run(E, cn["x0"], cn["x1"], cn["z"], cn["profile"], mat, hub_v=0.16)
    pc = sp["pier_caps"]
    W = spec["width"]
    for a, b in ((-pc["out"], cn["x0"]), (cn["x1"], W + pc["out"])):
        hk.block(E, a, b, pc["front"], 0.0, pc["z0"], pc["z1"], pc["mat"], faces="xXyzZ")
    return holes, (x0, x1, zh)


def mansard_roof(E, spec, prof, hulls):
    """Gambrel roof over the whole house, `overhang` past both gables: slate courses on top, the slab's underside
    where it can be seen (eaves, verges), its verge and eave edges; gutters on the front and back."""
    W, D = spec["width"], spec["depth"]
    rf = spec["roof"]
    X0, X1, th = -rf["overhang"], W + rf["overhang"], rf["thick"]
    mat = rf["mat"]
    pts = list(prof)
    for yc in (0.0, D):                                         # split the line at the walls
        for k in range(len(pts) - 1):
            if pts[k][0] < yc < pts[k + 1][0]:
                pts.insert(k + 1, (yc, roof_z(prof, yc)))
                break
    ridge_k = max(range(len(pts)), key=lambda k: pts[k][1])
    for k, ((ya, za), (yb, zb)) in enumerate(zip(pts, pts[1:])):
        lo, hi = ((ya, za), (yb, zb)) if k < ridge_k else ((yb, zb), (ya, za))
        if rf.get("cover") == "seam":         # standing-seam metal (house #5): ribs on the steep slopes only
            hk.seam_facet(E, mat, (X0, lo[0], lo[1]), (X1, lo[0], lo[1]), (X1, hi[0], hi[1]), (X0, hi[0], hi[1]), pitch=rf["seam_pitch"],
                          w=rf.get("seam_w", 0.04), h=rf.get("seam_h", 0.035), ribs=abs(zb - za) > abs(yb - ya))
        else:
            gk.tile_facet(E, mat, (X0, lo[0], lo[1]), (X1, lo[0], lo[1]), (X1, hi[0], hi[1]), (X0, hi[0], hi[1]), pitch=rf["pitch"],
                          step=rf["step"], seed=60 + k)
        dn = (0.0, -(zb - za), yb - ya)
        dn = dn if dn[2] < 0 else tuple(-c for c in dn)          # the underside faces down
        spans = [(X0, 0.0), (W, X1)] if 0.0 <= ya and yb <= D else [(X0, X1)]
        for xa, xb in spans:
            hk.rect(E, (xa, ya, za - th), (xb, ya, za - th), (xb, yb, zb - th), (xa, yb, zb - th), mat, dn)
        for X, nx in ((X0, -1.0), (X1, 1.0)):                    # verge edges, up past the courses' steps
            s = rf["step"] + 0.005
            hk.rect(E, (X, ya, za - th), (X, yb, zb - th), (X, yb, zb + s), (X, ya, za + s), mat, (nx, 0, 0))
    for (y, z), ny in ((pts[0], -1.0), (pts[-1], 1.0)):          # eave edges
        hk.rect(E, (X0, y, z - th), (X1, y, z - th), (X1, y, z + rf["step"]), (X0, y, z + rf["step"]), mat, (0, ny, 0))
    gu = spec.get("gutter")                                       # house #6 has none
    if gu:
        hk.block(E, X0 + 0.03, X1 - 0.03, gu["y0"], 0.0, gu["z0"], gu["z1"], gu["mat"], faces="xXyzZ")
        hk.block(E, X0 + 0.03, X1 - 0.03, D, D - gu["y0"], gu["z0"], gu["z1"], gu["mat"], faces="xXYzZ")
    hulls.append([(x, y, z) for x in (X0, X1) for y, z in prof] + [(x, y, spec["ceiling"]) for x in (X0, X1) for y in (0.0, D)])


def mansard_dormer(E, spec, prof, xc, hulls):
    """One gabled dormer on the front slope: front face with a casement window, cheeks down to the roof, a head band, a
    slate gable roof running back into the main roof, its soffits and edges."""
    dm = spec["dormers"]
    mat, trim = dm["mat"], dm["trim"]
    hw, ww, yf, z0 = dm["half"], dm["win_half"], dm["front"], dm["z0"]
    wz0, wz1 = dm["win"]
    eh, ez, rz, th = dm["eave_half"], dm["eave_z"], dm["ridge_z"], dm["roof_thick"]
    s = (rz - ez) / eh                                            # roof slope (rise per run)
    yfr, ybk = dm["roof_front"], dm["roof_back"]

    def under(x):                                                 # the dormer roof's underside at x
        return rz - th - abs(x - xc) * s
    # front face (wall coordinates: the plane y = yf) up to the roof's underside, with the window
    top = [(xc - hw, under(xc - hw)), (xc, under(xc)), (xc + hw, under(xc + hw))]
    hole = (xc - ww, xc + ww, wz0, wz1)
    E.push(gk.translate(0.0, yf, 0.0))
    hk.outline_face(E, top, z0, mat, holes=[hole])
    E.pop()
    hk.sash_window(E, xc - ww, xc + ww, wz0, wz1, "Metal", "Glass", dm["panes"][0], dm["panes"][1], y=yf + dm["rec"], depth=dm["depth"], fw=0.06,
                   meeting=False)
    hk.reveal(E, hole, yf, yf + dm["rec"], trim, sides="lrt")
    hk.sill(E, xc - ww - 0.06, xc + ww + 0.06, wz0, trim, h=0.10, proud=0.05 - yf, back=yf + dm["rec"])
    hd = dm["head"]
    hx = abs(rz - th - hd["z1"]) / s                              # where the head band meets the roof underside
    hk.block(E, xc - min(hw, hx), xc + min(hw, hx), yf - hd["proud"], yf, hd["z0"], hd["z1"], trim, faces="xXyzZ")
    # cheeks: from the front back to where the main roof reaches their top, the foot 5 cm under the roof
    ztop = under(xc + hw)
    yb = float(np.interp(ztop, [p[1] for p in prof[:8]], [p[0] for p in prof[:8]]))
    foot = [(y, z - 0.05) for y, z in prof[:8] if yf < y < yb]
    poly_yz = [(yf, z0), *foot, (yb, ztop)]                      # a convex polygon, fanned from its top front corner
    for X, nx in ((xc - hw, -1.0), (xc + hw, 1.0)):
        E.fan([(X, y, z) for y, z in poly_yz], mat, (nx, 0, 0), hub=(X, yf, ztop))
    # the gable roof: slate on both slopes, the front edge, soffits in front of the face and outside the cheeks
    for sx in (-1.0, 1.0):
        xe = xc + sx * eh
        a, b = (xe, yfr, ez), (xe, ybk, ez)
        c, d = (xc, ybk, rz), (xc, yfr, rz)
        gk.tile_facet(E, "Roof", a, b, c, d, pitch=0.22, step=0.015, seed=90 + int(xc * 10) + int(sx))
        hk.rect(E, (xe, yfr, ez - th), (xe, ybk, ez - th), (xe, ybk, ez + 0.02), (xe, yfr, ez + 0.02), "Roof", (sx, 0, 0))   # eave edge
        hk.rect(E, (xe, yfr, ez - th), (xc, yfr, rz - th), (xc, yfr, rz + 0.02), (xe, yfr, ez + 0.02), "Roof", (0, -1, 0))   # front edge
        dn = (-sx * s, 0.0, -1.0)
        hk.rect(E, (xe, yfr, ez - th), (xc, yfr, rz - th), (xc, yf, rz - th), (xe, yf, ez - th), "Roof", dn)                # front soffit
        xw = xc + sx * hw
        hk.rect(E, (xe, yf, ez - th), (xw, yf, under(xw)), (xw, ybk, under(xw)), (xe, ybk, ez - th), "Roof", dn)          # side soffit
    hulls.append(hk.box_pts(xc - eh, xc + eh, yfr, ybk, z0, rz))


def build_townhouse_mansard(spec):
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    F0, CEIL, TOP = spec["floor"], spec["ceiling"], spec["top"]
    prof = mansard_profile(spec)
    q = spec["quoins"]
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    gw = spec["gable_window"]
    ws = spec["window_style"]
    for k, side in enumerate(("front", "right", "back", "left")):
        fac = spec["facades"].get(side, {})
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        holes, cut = mansard_windows(E, I, fac, spec, t), None
        if side == "front":
            shop_holes, cut = mansard_shop(E, I, spec, t, F0)
            sp = spec["shop"]
            z_up = sp["cornice"]["z"] + max(v for _u, v in sp["cornice"]["profile"])
            pl = sp["pilasters"]["x"]
            hk.flat_face(E, 0.0, pl[0][0], 0.0, sp["pier_caps"]["z0"], "Stone", cell=0.6)            # the stone piers
            hk.flat_face(E, pl[-1][1], L, 0.0, sp["pier_caps"]["z0"], "Stone", cell=0.6)
            hk.flat_face(E, 0.0, L, z_up, TOP, "Stone", holes=holes, cell=0.6)
            holes = holes + shop_holes
        else:
            hk.flat_face(E, 0.0, L, 0.0, TOP, "Stone", holes=holes, cell=0.6)
        hk.quoin_corner(E, 0.0, TOP, q["course"], q["long"], q["short"], q["proud"], q["mat"], start_long=(k % 2 == 0))
        hk.wall_core(E, L, 0.0, TOP, t, holes=holes)
        hk.flat_face(I, t, L - t, F0, CEIL, "Plaster", holes=holes, y=t, inward=True)
        if side in ("front", "back"):
            # the wall carried up behind the gutter to the roof's underside (closes the slot over the wall head)
            hk.flat_face(E, 0.0, L, TOP, roof_z(prof, 0.0) - spec["roof"]["thick"] + 0.02, "Stone", cell=0.6)
        else:
            # the gable: stone up to the roof's underside, its window, a verge band under the roof, a core inside
            th = spec["roof"]["thick"]
            yy = [p[0] for p in prof if 0.0 < p[0] < D]
            outline = [(0.0, roof_z(prof, 0.0) - th)] + [(y, roof_z(prof, y) - th) for y in yy] + [(D, roof_z(prof, D) - th)]
            ghole = (gw["x0"], gw["x1"], gw["z0"], gw["z1"])
            hk.outline_face(E, outline, TOP, "Stone", holes=[ghole])
            for yc, inward in ((t / 2 - 0.001, False), (t / 2 + 0.001, True)):
                hk.outline_face(E, outline, TOP, "Plaster", holes=[ghole], y=yc, inward=inward, var=0.2)
            vg = spec["verge"]
            hk.verge_band(E, outline, vg["h"], vg["proud"], vg["mat"])
            rec = ws["rec"]
            hk.sash_window(E, *ghole, ws["frame"], ws["glass"], gw["panes"][0], gw["panes"][1], y=rec, depth=ws["depth"], fw=ws["fw"], meeting=False)
            hk.reveal(E, ghole, 0.0, rec, ws["reveal_mat"], sides="lrt")
            hk.reveal(E, ghole, rec + ws["depth"], t / 2, "Plaster")
            ar = ws["architrave"]
            hk.frame_run(E, *ghole, ar["profile"], ar["mat"])
            s, ln = ws["sill"], ws["lintel"]
            hk.sill(E, gw["x0"] - s["ext"], gw["x1"] + s["ext"], gw["z0"] - s["drop"], s["mat"], h=s["h"], proud=s["proud"], back=rec)
            hk.block(E, gw["x0"], gw["x1"], 0.0, rec, gw["z0"] - s["drop"], gw["z0"], s["mat"], faces="Z")
            hk.lintel(E, gw["x0"] - ln["ext"], gw["x1"] + ln["ext"], gw["z1"] + ln["gap"], gw["z1"] + ln["gap"] + ln["h"], ln["mat"], proud=ln["proud"])
            hulls.append(xf(E, [(x, y, z) for x, z in outline for y in (0.0, t)] + [(0.0, 0.0, TOP), (L, 0.0, TOP), (0.0, t, TOP), (L, t, TOP)]))
        if cut:
            dx0, dx1, dz = cut
            hulls += [xf(E, hk.box_pts(0.0, dx0, 0.0, t, 0.0, TOP)), xf(E, hk.box_pts(dx1, L, 0.0, t, 0.0, TOP)),
                      xf(E, hk.box_pts(dx0, dx1, 0.0, t, dz, TOP))]
        else:
            hulls.append(xf(E, hk.box_pts(0.0, L, 0.0, t, 0.0, TOP)))
        E.pop()
        I.pop()
    for b in spec["bands"]:
        hk.band_loop(E, W, D, b["z0"], b["z1"], b["proud"], b["mat"], sides=b.get("sides"))
    mansard_roof(E, spec, prof, hulls)
    for xc in spec["dormers"]["x"]:
        mansard_dormer(E, spec, prof, xc, hulls)
    # inside: one empty room
    hk.rect(I, (t, t, F0), (W - t, t, F0), (W - t, D - t, F0), (t, D - t, F0), "Timber", (0, 0, 1))
    hk.rect(I, (t, t, CEIL), (W - t, t, CEIL), (W - t, D - t, CEIL), (t, D - t, CEIL), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, F0))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_zinc (house #5 on): a Paris-style ashlar house: a plinth with basement vent grilles, rusticated corner
# pilasters, a band and a crowning cornice right round; windows with architraves, sills on brackets and hoods; an
# arched front door (archivolt on pedestals, keystone, sunburst fanlight, two leaves, steps) and square doors; a
# mansard of standing-seam metal with a flat top between two stone gables (mansard_roof with "cover": "seam"), an
# iron railing with corner finials round the flat top, pedimented dormers on the front and back slopes. One empty
# shell inside.
# ---------------------------------------------------------------------------------------------
def zinc_windows(E, I, wins, ws, t):
    """Windows of one side (wall coordinates): [(x0, x1, z0, z1, (cols, rows)), ...] in the style ws: frame and glass,
    reveals, and when given an architrave, a sill (on brackets) and a hood. Returns their holes."""
    holes = []
    for x0, x1, z0, z1, panes in wins:
        holes.append((x0, x1, z0, z1))
        rec = ws["rec"]
        hk.sash_window(E, x0, x1, z0, z1, ws["frame"], ws["glass"], panes[0], panes[1], y=rec, depth=ws["depth"], fw=ws["fw"], meeting=False)
        hk.reveal(E, (x0, x1, z0, z1), 0.0, rec, ws["reveal_mat"], sides="lrt")
        hk.reveal(I, (x0, x1, z0, z1), rec, t, "Plaster")
        aw = 0.0
        if ws.get("architrave"):
            ar = ws["architrave"]
            hk.frame_run(E, x0, x1, z0, z1, ar["profile"], ar["mat"])
            aw = max(p[0] for p in ar["profile"])
        s = ws["sill"]
        zs = z0 - s["drop"]
        hk.sill(E, x0 - s["ext"], x1 + s["ext"], zs, s["mat"], h=s["h"], proud=s["proud"], back=rec)
        if s["drop"] > 0:
            hk.block(E, x0, x1, 0.0, rec, zs, z0, s["mat"], faces="Z")              # the sill's top under the frame
        if ws.get("brackets"):
            b = ws["brackets"]
            zb1 = zs - s["h"]
            for bx in (x0 - aw + b["inset"], x1 + aw - b["inset"] - b["w"]):
                hk.block(E, bx, bx + b["w"], -b["proud"], 0.0, zb1 - b["h"], zb1, b["mat"], faces="xXyzZ")
        if ws.get("hood"):
            hd = ws["hood"]
            hk.moulding_run(E, x0 - hd["ext"], x1 + hd["ext"], z1 + aw + hd["gap"], hd["profile"], hd["mat"])
    return holes


def zinc_plinth(E, L, pl, holes, grilles):
    """The plinth along one side (wall coordinates): its face `proud` in front of the wall from the street to z1, with
    holes, the top strip back to the wall (it covers the side's right-hand corner), and the vent grilles."""
    p, z1, mat = pl["proud"], pl["z1"], pl["mat"]
    gh = [(g[0], g[1], *pl["grille_z"]) for g in grilles]
    hk.flat_face(E, -p, L + p, 0.0, z1, mat, holes=holes + gh, y=-p, cell=0.6)
    spans = [(0.0, L + p)]
    for hx0, hx1, _hz0, hz1 in holes:                             # no top strip over a doorway through the plinth
        if hz1 >= z1 - 1e-6:
            spans = [s for a, b in spans for s in ((a, min(b, hx0)), (max(a, hx1), b)) if s[1] - s[0] > 1e-6]
    for a, b in spans:
        hk.rect(E, (a, -p, z1), (b, -p, z1), (b, 0.0, z1), (a, 0.0, z1), mat, (0, 0, 1))
    for x0, x1, z0, zt in gh:
        hk.vent_grille(E, x0, x1, z0, zt, -p, p + pl["grille_depth"], mat, pl["iron"], pl["iron"])
    return gh


def zinc_arch_door(E, I, spec, dr, t, F0):
    """The arched front door (wall coordinates): steps, pedestals, archivolt with keystone, spandrels, reveals and
    soffit back to the leaves, a transom rail, the sunburst fanlight and two closed leaves (separate meshes).
    Returns (the hole in the wall face, the hole in the core and inner face)."""
    x0, x1, zs = dr["x0"], dr["x1"], dr["spring"]
    xc, r = (x0 + x1) / 2, (x1 - x0) / 2
    crown = zs + r
    sr = dr["surround"]
    sw, sp, sz0, smat = sr["w"], sr["proud"], sr["z0"], sr["mat"]
    ly, lt = dr["leaf_y"], dr["leaf_t"]
    ped = dr["pedestals"]
    # steps (the lowest wider, in front of the pedestals), threshold at the floor
    for k, st in enumerate(dr["steps"]):
        sx0, sx1 = (st["x"] if "x" in st else (x0, x1))
        hk.block(E, sx0, sx1, st["y"], ly + lt, 0.0, st["z"], st["mat"], faces=("xXyZ" if "x" in st else "yZ"))
    # pedestals under the archivolt, then its jambs, ring and keystone
    pp = ped["proud"]
    hk.block(E, x0 - ped["w"], x0, -pp, 0.0, 0.0, sz0, ped["mat"], faces="xyZ")
    hk.block(E, x1, x1 + ped["w"], -pp, 0.0, 0.0, sz0, ped["mat"], faces="XyZ")
    hk.block(E, x0 - sw, x0, -sp, 0.0, sz0, zs, smat, faces="xy")
    hk.block(E, x1, x1 + sw, -sp, 0.0, sz0, zs, smat, faces="Xy")
    hk.arch_ring(E, xc, zs, r, r + sw, -sp, smat)
    ks = dr["keystone"]
    hk.block(E, xc - ks["w"] / 2, xc + ks["w"] / 2, -ks["proud"], 0.0, crown, ks["z1"], smat, faces="xXyzZ")
    # the wall face round the archivolt: its hole is the archivolt's bounding box; spandrels fill the corners
    z_top = ks["z1"]
    hk.arch_spandrels(E, xc, zs, r + sw, x0 - sw, x1 + sw, z_top, 0.0, spec["stone"])
    # jambs and soffit from the front of the stone back to the leaves: stone to the lining depth, then timber lining
    yl = dr["lining_y"]
    for (za, zb, ya) in ((0.0, sz0, -pp), (sz0, zs, -sp)):
        hk.reveal(E, (x0, x1, za, zb), ya, yl, smat, sides="lr")
    hk.reveal(E, (x0, x1, 0.0, zs), yl, ly, "Door", sides="lr")
    hk.arch_intrados(E, xc, zs, r, -sp, yl, smat)
    hk.arch_intrados(E, xc, zs, r, yl, ly + lt, "Door")
    # transom rail and fanlight
    tz0 = dr["transom"]
    hk.block(E, x0, x1, ly - 0.02, ly + lt + 0.02, tz0, zs, "Door", faces="yzZ")
    hk.fanlight(E, xc, zs, r, ly + lt / 2, "Glass", dr["iron"], spokes=dr.get("spokes", 10))
    # two leaves, each its own mesh hinged at its jamb
    w, h = (x1 - x0 - 0.01) / 2, tz0 - F0 - 0.005
    for left in (True, False):
        def make(B, w=w, h=h):
            hk.panel_leaf(B, w, h, "Door", t=lt, cols=1, weights=dr["panel_weights"], frame=dr.get("frame", 0.13))
        separate_leaf(E, (x0 + 0.005) if left else (x1 - 0.005), left, w, h, ly, F0, lt, make, seed=31 + len(DOORS))
    return (x0 - sw, x1 + sw, sz0, z_top), (x0, x1, 0.0, crown)


def zinc_square_door(E, I, dr, t):
    """A square-headed door (wall coordinates): a sill, reveals back to the leaves, a transom rail and light, and two
    closed leaves (separate meshes). Returns its hole."""
    x0, x1, z0, z1 = dr["x0"], dr["x1"], dr["z0"], dr["z1"]
    ly, lt = dr["leaf_y"], dr["leaf_t"]
    tz0, tz1 = dr["transom"]
    hk.block(E, x0, x1, -dr["sill_proud"], ly + lt, z0 - 0.12, z0, dr["sill_mat"], faces="xXyzZ")
    hk.reveal(E, (x0, x1, z0, z1), 0.0, ly, dr["reveal_mat"], sides="lrt")
    hk.reveal(I, (x0, x1, z0, z1), ly + lt, t, "Plaster", sides="lrt")
    hk.block(E, x0, x1, ly - 0.02, ly + lt + 0.02, tz0, tz1, "Door", faces="yzZ")
    hk.sash_window(E, x0, x1, tz1, z1, "Metal", "Glass", dr["lights"], 1, y=ly, depth=lt, fw=0.05, meeting=False)
    w, h = (x1 - x0 - 0.01) / 2, tz0 - z0 - 0.005
    for left in (True, False):
        def make(B, w=w, h=h):
            hk.panel_leaf(B, w, h, "Door", t=lt, cols=1, weights=dr["panel_weights"], frame=dr.get("frame", 0.11))
        separate_leaf(E, (x0 + 0.005) if left else (x1 - 0.005), left, w, h, ly, z0, lt, make, seed=31 + len(DOORS))
    return (x0, x1, z0 - 0.12, z1)


def zinc_dormer(E, spec, front, xc, hulls):
    """One pedimented dormer on a roof slope (wall coordinates of that side; front = the roof's front points
    [(y, z), ...] from the eave up to the break): a stone front with a window in an architrave, a sill, an entablature
    under a pediment, stone cheeks down to the roof, and a metal gable roof whose front edge is the raking cornice."""
    dm = spec["dormers"]
    mat, rmat = dm["mat"], dm["roof"]
    trim = dm.get("trim", mat)          # house #6: timber architrave, sill, head beam and bargeboards on a plaster face
    hw, ww, yf, z0 = dm["half"], dm["win_half"], dm["front"], dm["z0"]
    wz0, wz1 = dm["win"]
    pd = dm["pediment"]
    eh, ez, rz, th = pd["half"], pd["z0"], pd["apex"], pd["thick"]
    s = (rz - ez) / eh                                            # roof slope (rise per run)
    yfr, ybk = yf - pd["proud"], dm["roof_back"]

    def under(x):                                                 # the dormer roof's underside at x
        return rz - th - abs(x - xc) * s
    top = [(xc - hw, under(xc - hw)), (xc, under(xc)), (xc + hw, under(xc + hw))]
    hole = (xc - ww, xc + ww, wz0, wz1)
    E.push(gk.translate(0.0, yf, 0.0))
    hk.outline_face(E, top, z0, mat, holes=[hole])
    ar = dm["architrave"]
    hk.frame_run(E, *hole, ar["profile"], trim)
    E.pop()
    hk.sash_window(E, xc - ww, xc + ww, wz0, wz1, "Metal", "Glass", dm["panes"][0], dm["panes"][1], y=yf + dm["rec"], depth=dm["depth"], fw=0.05,
                   meeting=False)
    hk.reveal(E, hole, yf, yf + dm["rec"], trim, sides="lrt")
    sl = dm["sill"]
    hk.block(E, xc - sl["half"], xc + sl["half"], yf - sl["proud"], yf + dm["rec"], sl["z0"], wz0, trim, faces="xXyzZ")
    en = dm["entablature"]
    hk.block(E, xc - en["half"], xc + en["half"], yf - en["proud"], yf, en["z0"], ez, trim, faces="xXyzZ")
    for px0, px1 in dm.get("posts", []):                          # house #6: timber posts either side of the window
        hk.block(E, px0, px1, yf - en["proud"], yf, z0, en["z0"], trim, faces="xXyz")
    # cheeks: from the front back to where the main roof reaches their top, the foot 5 cm under the roof
    ztop = under(xc + hw)
    ys, zs = [p[0] for p in front], [p[1] for p in front]
    yb = float(np.interp(ztop, zs, ys))
    foot = [(y, z - 0.05) for y, z in front if yf < y < yb]
    poly_yz = [(yf, z0), *foot, (yb, ztop)]
    for X, nx in ((xc - hw, -1.0), (xc + hw, 1.0)):
        E.fan([(X, y, z) for y, z in poly_yz], mat, (nx, 0, 0), hub=(X, yf, ztop))
    # the gable roof: metal on both slopes, its eave edges, the raking cornice at the front, soffits
    for sx in (-1.0, 1.0):
        xe = xc + sx * eh
        hk.rect(E, (xe, yfr, ez), (xe, ybk, ez), (xc, ybk, rz), (xc, yfr, rz), rmat, (sx * s, 0.0, 1.0))
        hk.rect(E, (xe, yfr, ez - th), (xe, ybk, ez - th), (xe, ybk, ez), (xe, yfr, ez), rmat, (sx, 0, 0))                # eave edge
        hk.rect(E, (xe, yfr, ez - th), (xc, yfr, rz - th), (xc, yfr, rz), (xe, yfr, ez), trim, (0, -1, 0))                 # raking cornice
        dn = (-sx * s, 0.0, -1.0)
        hk.rect(E, (xe, yfr, ez - th), (xc, yfr, rz - th), (xc, yf, rz - th), (xe, yf, ez - th), trim, dn)                 # front soffit
        xw = xc + sx * hw
        hk.rect(E, (xe, yf, ez - th), (xw, yf, under(xw)), (xw, ybk, under(xw)), (xe, ybk, ez - th), rmat, dn)            # side soffit
    hulls.append(xf(E, hk.box_pts(xc - eh, xc + eh, yfr, ybk, z0, rz)))


def build_townhouse_zinc(spec):
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    F0, CEIL, TOP = spec["floor"], spec["ceiling"], spec["top"]
    prof = mansard_profile(spec)
    front_pts = [tuple(p) for p in spec["roof"]["profile"]]
    th = spec["roof"]["thick"]
    stone = spec["stone"]
    pl, pi = spec["plinth"], spec["pilasters"]
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    for k, side in enumerate(("front", "right", "back", "left")):
        fac = spec["facades"].get(side, {})
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        wins = [(*w["x"], *w["z"], tuple(w["panes"])) for w in fac.get("ground", [])]
        holes = zinc_windows(E, I, wins, spec["ground_style"], t)
        wins = [(*w["x"], *w["z"], tuple(w["panes"])) for w in fac.get("upper", [])]
        holes += zinc_windows(E, I, wins, spec["window_style"], t)
        face_holes, core_holes, plinth_holes, cut = list(holes), list(holes), [], None
        if fac.get("arch_door"):
            dr = fac["arch_door"]
            fh, ch = zinc_arch_door(E, I, spec, dr, t, F0)
            face_holes.append(fh)
            core_holes.append(ch)
            plinth_holes.append((dr["x0"] - dr["pedestals"]["w"], dr["x1"] + dr["pedestals"]["w"], 0.0, pl["z1"]))
            cut = (ch[0], ch[1], ch[3])
        if fac.get("square_door"):
            dr = fac["square_door"]
            h = zinc_square_door(E, I, dr, t)
            face_holes.append(h)
            core_holes.append(h)
            cut = (h[0], h[1], h[3], h[2])
        gh = zinc_plinth(E, L, pl, plinth_holes, fac.get("grilles", []))
        hk.flat_face(E, 0.0, L, pl["z1"], TOP, stone, holes=face_holes, cell=0.6)
        for z0, z1 in pi["z"]:
            hk.quoin_corner(E, z0, z1, pi["course"], pi["long"], pi["short"], pi["proud"], pi["mat"], start_long=True)
        hk.wall_core(E, L, 0.0, TOP, t, holes=core_holes)
        inner = [(a, b, max(c, F0), d) for a, b, c, d in core_holes if d > F0]
        hk.flat_face(I, t, L - t, F0, CEIL, "Plaster", holes=inner, y=t, inward=True)
        if side in ("front", "back"):
            # the wall carried up behind the gutter to the roof's underside
            hk.flat_face(E, 0.0, L, TOP, roof_z(prof, 0.0) - th + 0.02, stone, cell=0.6)
            for xc in spec["dormers"]["x"]:
                zinc_dormer(E, spec, front_pts, xc, hulls)
        else:
            # the gable: stone up to the roof's underside, a core inside
            yy = [p[0] for p in prof if 0.0 < p[0] < D]
            outline = [(0.0, roof_z(prof, 0.0) - th)] + [(y, roof_z(prof, y) - th) for y in yy] + [(D, roof_z(prof, D) - th)]
            hk.outline_face(E, outline, TOP, stone)
            for yc, inward in ((t / 2 - 0.001, False), (t / 2 + 0.001, True)):
                hk.outline_face(E, outline, TOP, "Plaster", y=yc, inward=inward, var=0.2)
            hulls.append(xf(E, [(x, y, z) for x, z in outline for y in (0.0, t)] + [(0.0, 0.0, TOP), (L, 0.0, TOP), (0.0, t, TOP), (L, t, TOP)]))
        if cut and len(cut) == 3:                                 # a door from the street: walls either side and over it
            dx0, dx1, dz = cut
            hulls += [xf(E, hk.box_pts(0.0, dx0, 0.0, t, 0.0, TOP)), xf(E, hk.box_pts(dx1, L, 0.0, t, 0.0, TOP)),
                      xf(E, hk.box_pts(dx0, dx1, 0.0, t, dz, TOP))]
        elif cut:                                                 # a raised door: also the wall under it
            dx0, dx1, dz, dz0 = cut
            hulls += [xf(E, hk.box_pts(0.0, dx0, 0.0, t, 0.0, TOP)), xf(E, hk.box_pts(dx1, L, 0.0, t, 0.0, TOP)),
                      xf(E, hk.box_pts(dx0, dx1, 0.0, t, dz, TOP)), xf(E, hk.box_pts(dx0, dx1, 0.0, t, 0.0, dz0))]
        else:
            hulls.append(xf(E, hk.box_pts(0.0, L, 0.0, t, 0.0, TOP)))
        E.pop()
        I.pop()
    for b in spec["bands"]:
        hk.band_loop(E, W, D, b["z0"], b["z1"], b["proud"], b["mat"], sides=b.get("sides"))
    cn = spec["cornice"]
    E.sweep([(x, y, cn["z"]) for x, y in hk.plan_loop(W, D)], [[tuple(p) for p in cn["profile"]]], cn["mat"], closed=True)
    mansard_roof(E, spec, prof, hulls)
    # the iron railing round the flat top, with a finial post at each corner
    rl = spec["railing"]
    ri = rl["inset"]
    loop = [(rl["x0"], ri), (rl["x1"], ri), (rl["x1"], D - ri), (rl["x0"], D - ri)]
    for k in range(4):
        m, L = gk.wall_frame(loop[k], loop[(k + 1) % 4])
        E.push(m)
        hk.iron_railing(E, 0.0, L, 0.0, rl["z0"], rl["h"], rl["mat"], pitch=rl["pitch"], spear=rl["spear"])
        E.pop()
    for x, y in loop:
        hk.iron_finial(E, x, y, rl["z0"], rl["post_top"], rl["mat"])
    # inside: one empty room
    hk.rect(I, (t, t, F0), (W - t, t, F0), (W - t, D - t, F0), (t, D - t, F0), "Timber", (0, 0, 1))
    hk.rect(I, (t, t, CEIL), (W - t, t, CEIL), (W - t, D - t, CEIL), (t, D - t, CEIL), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, F0))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_jetty (house #6 on): a half-timbered house over a stone ground floor. The ground floor (house space:
# x 0..W, y 0..D) is ashlar on a plinth, its front opened by a deep round-arched porch (archivolt on imposts, a stone
# soffit back to a tympanum with a barred window and a painted fascia) in front of a timber shopfront with a glazed
# door; narrow arched passages run through the piers from both sides into the porch. Above it, timber-framed storeys
# (spec "storeys"), each a box of its own that may jetty out over the one below, with plaster infill, posts, rails,
# braces, plates and bressummers, casement windows with open plank shutters, jetty soffits and corbels. A gable roof
# from the spec's profile (eave, optional kick points, ridge, back eave: the ridge need not be in the middle) over gable walls framed up to
# its underside, a timber-and-plaster dormer, and a wrought-iron hanging sign. One empty shell inside.
# ---------------------------------------------------------------------------------------------
def jetty_mirror(wl, L):
    """A wall spec mirrored end for end (the left side as the mirror image of the right)."""
    out = {k: v for k, v in wl.items() if k not in ("posts", "rails", "plates", "braces", "windows", "mirror")}
    out["posts"] = [[L - b, L - a] for a, b in wl.get("posts", [])]
    out["rails"] = [[L - b, L - a, c, d] for a, b, c, d in wl.get("rails", [])]
    out["plates"] = [[L - b, L - a, c, d, p] for a, b, c, d, p in wl.get("plates", [])]
    out["braces"] = [[L - xa, za, L - xb, zb] for xa, za, xb, zb in wl.get("braces", [])]
    out["windows"] = [[L - b, L - a, c, d, n, m] for a, b, c, d, n, m in wl.get("windows", [])]
    return out


def jetty_wall(E, I, wl, L, z0, top, t, ws, hulls):
    """One timber-framed wall in wall coordinates (infill plane y = 0) from z0 up to `top` (a height, or a top outline
    [(x, z), ...] such as a gable under the roof): plaster infill with window holes, the hidden core, the inner face,
    the framing (house_kit.timber_members), casement windows with reveals and open shutters. Adds its hull."""
    flat = not isinstance(top, list)
    outline = [(0.0, top), (L, top)] if flat else top
    wins = [tuple(w[:4]) for w in wl.get("windows", [])]
    # house #7 (all optional): "holes" are openings through the whole wall that the caller fills (loft doors),
    # "louvres" are louvre boards in the outer face (closed at their back, so core and inner face stay whole),
    # "infill" is the outer face's material (a timber-clad wall)
    wins += [tuple(h[:4]) for h in wl.get("holes", [])]
    louv = [tuple(h[:4]) for h in wl.get("louvres", [])]
    infill = wl.get("infill", "Plaster")
    if flat:
        hk.flat_face(E, 0.0, L, z0, top, infill, holes=wins + louv, cell=0.6)
        hk.wall_core(E, L, z0, top, t, holes=wins)
        hk.flat_face(I, 0.0, L, z0, top, "Plaster", holes=wins, y=t, inward=True)
    else:
        hk.outline_face(E, outline, z0, infill, holes=wins + louv)
        for yc, inward in ((t / 2 - 0.001, False), (t / 2 + 0.001, True)):
            hk.outline_face(E, outline, z0, "Plaster", holes=wins, y=yc, inward=inward, var=0.2)
        hk.outline_face(I, [(x, z - 0.03) for x, z in outline], z0, "Plaster", holes=wins, y=t, inward=True)
        hk.rect(E, (0.0, 0.0, z0 - 0.01), (L, 0.0, z0 - 0.01), (L, t, z0 - 0.01), (0.0, t, z0 - 0.01), "Plaster", (0, 0, 1), 0.2)
    for lv in louv:
        hk.louvres(E, *lv, wl.get("louvre_mat", "Timber"))          # "louvre_mat" (house #8): a dark iron vent slit
    hk.timber_members(E, wl, z0, top if flat else outline)
    sh = ws.get("shutter")
    for x0, x1, wz0, wz1, c, r in wl.get("windows", []):
        hole = (x0, x1, wz0, wz1)
        hk.sash_window(E, x0, x1, wz0, wz1, ws["frame"], "Glass", c, r, y=ws["rec"], depth=ws["depth"], fw=ws["fw"], meeting=False)
        hk.reveal(E, hole, 0.0, ws["rec"], ws.get("reveal", "Timber"))  # "reveal" (house #8): stone reveals
        hk.reveal(I, hole, ws["rec"], t, "Plaster")
        if wl.get("shutters"):
            g, w = sh["gap"], sh["w"]
            hk.plank_shutter(E, x0 - g - w, x0 - g, wz0, wz1, -0.08, sh["mat"], sh["iron"], hinge="r")
            hk.plank_shutter(E, x1 + g, x1 + g + w, wz0, wz1, -0.08, sh["mat"], sh["iron"], hinge="l")
    hulls.append(xf(E, [(x, y, z) for x, z in outline for y in (0.0, t)] + [(x, y, z0) for x in (0.0, L) for y in (0.0, t)]))


def jetty_storey(E, I, spec, st, prof, hulls):
    """One timber-framed storey: four walls round the box st["x"] x (st["front"]..st["back"]) from st["z"][0] to
    st["z"][1], or with "top": "roof" up to the roof's underside (gables on the sides)."""
    th = spec["roof"]["thick"]
    xl, xr = st["x"]
    yf, yb = st["front"], st["back"]
    z0, z1 = st["z"]

    def under(y):
        return roof_z(prof, y) - th
    inner_ys = [p[0] for p in prof if yf < p[0] < yb]
    frames = {"front": ((xl, yf), (xr, yf)), "right": ((xr, yf), (xr, yb)), "back": ((xr, yb), (xl, yb)), "left": ((xl, yb), (xl, yf))}
    for side in ("front", "right", "back", "left"):
        m, L = gk.wall_frame(*frames[side])
        wl = st["walls"].get(side, {})
        if "mirror" in wl:
            wl = jetty_mirror(st["walls"][wl["mirror"]], L)
        if st.get("top") != "roof":
            top = z1
        elif side == "front":
            top = under(yf)
        elif side == "back":
            top = under(yb)
        else:
            ys = [yf] + inner_ys + [yb]
            pts = [((y - yf) if side == "right" else (yb - y), under(y)) for y in ys]
            top = sorted(pts)
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, z0, top, st["t"], spec["window_style"], hulls)
        E.pop()
        I.pop()


def jetty_ground(E, I, spec, hulls):
    """The stone ground floor (house coordinates): plinth, ashlar faces, the arched porch with its archivolt, imposts,
    soffit, tympanum (barred window, fascia) and porch ceiling, the side passages through the piers, the timber
    shopfront with its glazed door (a separate leaf), the inner faces and the collision."""
    W, D, F0 = spec["width"], spec["depth"], spec["floor"]
    g = spec["ground"]
    t, Z1, stone = g["t"], g["z1"], g["stone"]
    pl, ar, tn, sp = g["plinth"], g["arch"], g["tunnel"], g["shop"]
    xc, zs, r, ro = ar["xc"], ar["spring"], ar["r"], ar["ring"]
    xa, xb = xc - r, xc + r
    ty, tz, tt = ar["tympanum_y"], ar["tympanum_z"], ar["tympanum_t"]
    ys, s_t = sp["y"], sp["t"]
    tr, tsp = tn["r"], tn["spring"]

    def tunnel(c):
        return (c - tr, c + tr, 0.0, tsp + tr)
    # front: plinth, the face round the archivolt, spandrels, archivolt, imposts, the arch soffit back to the tympanum
    zinc_plinth(E, W, pl, [(xa, xb, 0.0, pl["z1"])], [])
    hk.reveal(E, (xa, xb, 0.0, pl["z1"]), -pl["proud"], 0.0, pl["mat"], sides="lr")
    hk.flat_face(E, 0.0, W, pl["z1"], Z1, stone, holes=[(xc - ro, xc + ro, zs, ar["top"]), (xa, xb, 0.0, zs)], cell=0.6)
    hk.arch_spandrels(E, xc, zs, ro, xc - ro, xc + ro, ar["top"], 0.0, stone)
    hk.arch_ring(E, xc, zs, r, ro, -ar["ring_proud"], ar["ring_mat"])
    hk.arch_intrados(E, xc, zs, r, -ar["ring_proud"], ty, stone)
    im = ar["impost"]
    for a, b in ((-im["out"], xa + im["in"]), (xb - im["in"], W + im["out"])):
        hk.block(E, a, b, -im["proud"], 0.0, im["z0"], im["z1"], im["mat"], faces="xXyYzZ")
    # the porch's side faces (facing into the porch), with the passages' arched mouths
    for left in (True, False):
        m, L = gk.wall_frame((xa, 0.0), (xa, ys)) if left else gk.wall_frame((xb, ys), (xb, 0.0))
        c = tn["y"] if left else ys - tn["y"]
        step = (ty, L, tz, zs) if left else (0.0, L - ty, tz, zs)
        E.push(m)
        hk.flat_face(E, 0.0, L, 0.0, zs, stone, holes=[tunnel(c), step], cell=0.6)
        hk.arch_spandrels(E, c, tsp, tr, c - tr, c + tr, tsp + tr, 0.0, stone)
        E.pop()
    # tympanum with the barred window, its fascia, lintel and cap; the porch ceiling behind it
    arc = hk.arc_pts(xc, zs, r, 180.0, 0.0, n=24)
    gr = g["grille"]
    gh = (gr["x"][0], gr["x"][1], gr["z"][0], gr["z"][1])
    E.push(gk.translate(0.0, ty, 0.0))
    hk.outline_face(E, arc, tz, stone, holes=[gh])
    hk.barred_window(E, *gh, gr["depth"], stone, gr["frame"], "Glass", "Bronze", cols=gr["cols"], rows=gr["rows"])
    E.pop()
    hk.outline_face(I, [(x, z - 0.02) for x, z in arc], tz, "Plaster", holes=[gh], y=ty + tt, inward=True)
    hk.reveal(I, gh, ty + gr["depth"], ty + tt, "Plaster")
    fa = g["fascia"]
    l0, l1, lp = fa["lintel"]
    hk.block(E, xa, xb, ty - lp, ty, l0, l1, fa["mat"], faces="yzZ")
    for z0_, z1_, p in ((fa["z"][0], fa["z"][1], fa["proud"]), tuple(fa["cap"])):
        hw = math.sqrt(r * r - (z1_ - zs) ** 2) - 0.02
        hk.block(E, xc - hw, xc + hw, ty - p, ty, z0_, z1_, fa["mat"], faces="xXyzZ")
    hk.rect(E, (xa, ty, tz), (xb, ty, tz), (xb, ys, tz), (xa, ys, tz), stone, (0, 0, -1))
    # the shopfront on the plane ys: head, stiles, shop windows on stallrisers, door posts, the door and its transom
    smat = sp["mat"]
    h0, h1, hp = sp["head"]
    hk.block(E, xa, xb, ys - hp, ys, h0, h1, smat, faces="yz")
    st_ = sp["stile"]
    hk.block(E, xa, xa + st_, ys - 0.06, ys, 0.0, h0, smat, faces="Xy")
    hk.block(E, xb - st_, xb, ys - 0.06, ys, 0.0, h0, smat, faces="xy")
    gz0, gz1 = sp["glass_z"]
    s0, s1, sp_ = sp["sill"]
    rs = sp["riser"]
    inner = []
    for w in sp["windows"]:
        x0, x1 = w["x"]
        hk.sash_window(E, x0, x1, gz0, gz1, smat, "Glass", w["panes"][0], w["panes"][1], y=ys, depth=0.06, fw=0.06, meeting=False)
        hk.reveal(I, (x0, x1, gz0, gz1), ys + 0.06, ys + s_t, "Plaster")
        hk.block(E, x0, x1, ys - sp_, ys + 0.06, s0, s1, smat, faces="yzZ")
        mg = rs["margin"]
        panel = (x0 + mg, x1 - mg, mg, s0 - mg)
        hk.flat_face(E, x0, x1, 0.0, s0, smat, holes=[panel], y=ys)
        hk.flat_face(E, *panel, smat, y=ys + rs["sunk"])
        hk.reveal(E, panel, ys, ys + rs["sunk"], smat)
        inner.append((x0, x1, gz0, gz1))
    for px0, px1 in sp["posts"]:
        hk.block(E, px0, px1, ys - sp["post_proud"], ys, 0.0, h0, smat, faces="xXy")
    dr = sp["door"]
    dx0, dx1 = dr["x"]
    r0, r1 = dr["rail"]
    ly, lt = dr["leaf_y"], dr["leaf_t"]
    hk.block(E, dx0, dx1, ys - 0.02, ys + 0.08, r0, r1, smat, faces="yzZ")
    hk.sash_window(E, dx0, dx1, r1, h0, smat, "Glass", dr["transom_lights"], 1, y=ys, depth=0.06, fw=0.05, meeting=False)
    hk.reveal(E, (dx0, dx1, 0.0, r0), ys, ys + ly, smat, sides="lr")
    hk.reveal(I, (dx0, dx1, F0, r0), ys + ly + lt, ys + s_t, "Plaster", sides="lrt")
    hk.block(E, dx0, dx1, ys - 0.05, ys + s_t, 0.0, F0, stone, faces="yZ")
    w, h = dx1 - dx0 - 0.01, r0 - F0 - 0.005
    left = dr.get("hinge", "left") == "left"

    def make(B, w=w, h=h):
        hk.glazed_leaf(B, w, h, smat, "Glass", smat, t=lt, light=tuple(dr["light"]), grid=tuple(dr["grid"]), panel_z=tuple(dr["panels"]))
    separate_leaf(E, (dx0 + 0.005) if left else (dx1 - 0.005), left, w, h, ys + ly, F0, lt, make, seed=31 + len(DOORS))
    inner += [(dx0, dx1, 0.0, r0), (dx0, dx1, r1, h0)]
    hk.flat_face(I, xa, xb, F0, tz, "Plaster", holes=inner, y=ys + s_t, inward=True)
    for a, b in ((t, xa), (xb, W - t)):                       # the piers' backs, inside
        hk.flat_face(I, a, b, F0, Z1, "Plaster", y=ys + s_t, inward=True)
    # the sides: plinth, face, the passage through the pier (mouth, soffit, jambs), core, inner face
    for side in ("right", "left"):
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        c = tn["y"] if side == "right" else D - tn["y"]
        hole = tunnel(c)
        zinc_plinth(E, L, pl, [hole], [])
        hk.reveal(E, (hole[0], hole[1], 0.0, pl["z1"]), -pl["proud"], 0.0, pl["mat"], sides="lr")
        hk.flat_face(E, 0.0, L, pl["z1"], Z1, stone, holes=[hole], cell=0.6)
        hk.arch_spandrels(E, c, tsp, tr, hole[0], hole[1], hole[3], 0.0, stone)
        depth = xa if side == "left" else W - xb
        hk.arch_intrados(E, c, tsp, tr, 0.0, depth, stone)
        hk.reveal(E, (hole[0], hole[1], 0.0, tsp), 0.0, depth, stone, sides="lr")
        hk.wall_core(E, L, 0.0, Z1, t, holes=[hole])
        a, b = (ys + s_t, D - t) if side == "right" else (t, D - ys - s_t)
        hk.flat_face(I, a, b, F0, Z1, "Plaster", y=t, inward=True)
        E.pop()
        I.pop()
    m, L = hk.facade_frame("back", W, D)
    E.push(m)
    I.push(m)
    zinc_plinth(E, L, pl, [], [])
    hk.flat_face(E, 0.0, L, pl["z1"], Z1, stone, cell=0.6)
    hk.wall_core(E, L, 0.0, Z1, t)
    hk.flat_face(I, t, L - t, F0, Z1, "Plaster", y=t, inward=True)
    E.pop()
    I.pop()
    # collision: the piers round the passages, the wall over the arch in strips, tympanum, shopfront round the door,
    # side and back walls, the floor
    ty0, ty1 = tn["y"] - tr, tn["y"] + tr
    for x0, x1 in ((0.0, xa), (xb, W)):
        hulls += [hk.box_pts(x0, x1, 0.0, ty0, 0.0, Z1), hk.box_pts(x0, x1, ty1, ys, 0.0, Z1), hk.box_pts(x0, x1, ty0, ty1, tsp + tr, Z1)]
    n = 8
    for k in range(n):
        u0, u1 = xa + (xb - xa) * k / n, xa + (xb - xa) * (k + 1) / n
        dmin = min(abs(u0 - xc), abs(u1 - xc)) if not (u0 < xc < u1) else 0.0
        zl = zs + math.sqrt(max(r * r - dmin * dmin, 0.0))
        hulls.append(hk.box_pts(u0, u1, 0.0, ty, zl, Z1))
    hulls.append(hk.box_pts(xa, xb, ty, ys, tz, Z1))
    hulls += [hk.box_pts(xa, dx0, ys, ys + s_t, 0.0, tz), hk.box_pts(dx1, xb, ys, ys + s_t, 0.0, tz), hk.box_pts(dx0, dx1, ys, ys + s_t, r0, tz)]
    hulls += [hk.box_pts(0.0, t, ys, D, 0.0, Z1), hk.box_pts(W - t, W, ys, D, 0.0, Z1), hk.box_pts(t, W - t, D - t, D, 0.0, Z1)]
    hulls.append(hk.box_pts(t, W - t, ys + s_t, D - t, -0.3, F0))


def build_townhouse_jetty(spec):
    W, D, F0, CEIL = spec["width"], spec["depth"], spec["floor"], spec["ceiling"]
    prof = [tuple(p) for p in spec["roof"]["profile"]]
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    jetty_ground(E, I, spec, hulls)
    for st in spec["storeys"]:
        jetty_storey(E, I, spec, st, prof, hulls)
    for so in spec["soffits"]:
        z = so["z"]
        for x0, x1, y0, y1 in so["rects"]:
            hk.rect(E, (x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z), so.get("mat", "Timber"), (0, 0, -1))
    for co in spec["corbels"]:
        (y0, y1), (z0, z1), hw = co["y"], co["z"], co["w"] / 2
        for x in co["x"]:
            hk.block(E, x - hw, x + hw, y0, y1, z0, z1, "Timber", faces="xXyz")
    mansard_roof(E, spec, prof, hulls)
    for xc in spec["dormers"]["x"]:
        zinc_dormer(E, spec, prof[:max(range(len(prof)), key=lambda k: prof[k][1]) + 1], xc, hulls)
    sg = spec.get("sign")
    if sg:
        hk.hanging_sign(E, sg["x"], sg["y_wall"], sg["z_arm"], sg["reach"], tuple(sg["board"]), sg["mat"])
    # inside: one empty room from the shop floor up to a ceiling under the roof
    ig = spec["inside"]
    x0, x1, y0, y1 = ig["floor"]
    hk.rect(I, (x0, y0, F0), (x1, y0, F0), (x1, y1, F0), (x0, y1, F0), "Timber", (0, 0, 1))
    x0, x1, y0, y1 = ig["ceiling"]
    hk.rect(I, (x0, y0, CEIL), (x1, y0, CEIL), (x1, y1, CEIL), (x0, y1, CEIL), "Plaster", (0, 0, -1))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_loft (house #7 on): a drying loft. A rubble-stone ground floor (house space x 0..W, y 0..D) with a ledged
# plank door in a dressed-stone surround (jambs, lintel); timber storeys set back from the stone by spec "frame"
# "inset", each wall a jetty_wall (posts, rails, fixed louvres or plaster panels, "infill" Timber for a clad face);
# timber bands round the house at the storey joints with ledges back to the timber; a gable roof whose ridge runs
# front to back (mansard_roof turned a quarter, so the gables are the front and the back), the gables framed with
# plaster infill under principal rafters; a loft opening in the front gable closed by two plank leaves under an iron
# hoist; a window in the back gable. One empty shell inside.
# ---------------------------------------------------------------------------------------------
def loft_roof_frame(spec):
    """The frame mansard_roof runs in (its ridge along local x): local x along house +y from the front timber wall,
    local y across from the right-hand timber wall (house x = W - inset) to the left one."""
    W, i = spec["width"], spec["frame"]["inset"]
    return gk.translate(W - i, i, 0.0) @ gk.rot_z(90.0)


def loft_ground(E, I, spec, hulls):
    """The rubble ground floor: four rubble faces, the door (jambs, lintel, reveals, threshold, a separate plank
    leaf), cores and inner faces up to the timber, collision."""
    W, D, F0 = spec["width"], spec["depth"], spec["floor"]
    g = spec["ground"]
    t, Z1, ZT, stone, rb, dr = g["t"], g["z1"], g["top"], g["stone"], g["rubble"], g["door"]
    for k, side in enumerate(("front", "right", "back", "left")):
        m, L = hk.facade_frame(side, W, D)
        E.push(m)
        I.push(m)
        face_holes, core_holes, inner_holes, cut = [], [], [], None
        if side == dr["side"]:
            x0, x1 = dr["x"]
            zh = dr["z1"]
            jw, jp, jm = dr["jamb"]["w"], dr["jamb"]["proud"], dr["jamb"]["mat"]
            ln = dr["lintel"]
            lx0, lx1 = ln["x"]
            ly, lt = dr["leaf_y"], dr["leaf_t"]
            for a, b in ((x0 - jw, x0), (x1, x1 + jw)):
                hk.block(E, a, b, -jp, 0.0, 0.0, zh, jm, faces="xXy")
            hk.block(E, lx0, lx1, -ln["proud"], 0.0, zh, ln["z1"], jm, faces="xXyzZ")
            hk.reveal(E, (x0, x1, 0.0, zh), 0.0, ly, jm, sides="lrt")
            hk.reveal(I, (x0, x1, F0, zh), ly + lt, t, "Plaster", sides="lrt")
            hk.block(E, x0, x1, -jp, t, 0.0, F0, jm, faces="yZ")                     # threshold
            w, h = x1 - x0 - 0.01, zh - F0 - 0.005
            left = dr.get("hinge", "left") == "left"

            def make(B, w=w, h=h, left=left):
                hk.plank_leaf(B, w, h, dr["mat"], dr["iron"], t=lt, planks=dr["planks"], straps=tuple(dr["straps"]),
                              hinge="l" if left else "r", ring=dr.get("ring", 0.45))
            separate_leaf(E, (x0 + 0.005) if left else (x1 - 0.005), left, w, h, ly, F0, lt, make, seed=31 + len(DOORS))
            face_holes = [(x0 - jw, x1 + jw, 0.0, zh), (lx0, lx1, zh, ln["z1"])]
            core_holes = [(x0, x1, 0.0, zh)]
            inner_holes = [(x0, x1, F0, zh)]
            cut = (x0, x1, zh)
        hk.rubble_face(E, 0.0, L, 0.0, Z1, stone, holes=face_holes, course=rb["course"], length=tuple(rb["length"]),
                       chamfer=rb["chamfer"], jitter=rb["jitter"], seed=7 + 101 * k)
        hk.wall_core(E, L, 0.0, ZT, t, holes=core_holes)
        hk.flat_face(I, t, L - t, F0, ZT, "Plaster", holes=inner_holes, y=t, inward=True)
        if cut:
            dx0, dx1, dz = cut
            hulls += [xf(E, hk.box_pts(0.0, dx0, 0.0, t, 0.0, ZT)), xf(E, hk.box_pts(dx1, L, 0.0, t, 0.0, ZT)),
                      xf(E, hk.box_pts(dx0, dx1, 0.0, t, dz, ZT))]
        else:
            hulls.append(xf(E, hk.box_pts(0.0, L, 0.0, t, 0.0, ZT)))
        E.pop()
        I.pop()


def build_townhouse_loft(spec):
    W, D, F0, CEIL = spec["width"], spec["depth"], spec["floor"], spec["ceiling"]
    i, tt = spec["frame"]["inset"], spec["frame"]["t"]
    rf = spec["roof"]
    th = rf["thick"]
    prof = sorted(tuple(p) for p in rf["profile"])                # [(house x, top of the slates)], eave to eave

    def under(x):
        return roof_z(prof, x) - th
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    loft_ground(E, I, spec, hulls)
    # the timber storeys round the box inset from the stone
    xl, xr, yf, yb = i, W - i, i, D - i
    frames = {"front": ((xl, yf), (xr, yf)), "right": ((xr, yf), (xr, yb)), "back": ((xr, yb), (xl, yb)), "left": ((xl, yb), (xl, yf))}
    ws = spec["window_style"]
    for st in spec["storeys"]:
        z0, z1 = st["z"]
        for side in ("front", "right", "back", "left"):
            m, L = gk.wall_frame(*frames[side])
            wl = st["walls"].get(side, {})
            if "mirror" in wl:
                wl = jetty_mirror(st["walls"][wl["mirror"]], L)
            top = z1 if side in ("front", "back") else st.get("side_top", z1)
            E.push(m)
            I.push(m)
            jetty_wall(E, I, wl, L, z0, top, tt, ws, hulls)
            E.pop()
            I.pop()
    # the gables, framed up to the roof's underside, with principal rafters under it; loft doors or a window
    gb = spec["gables"]
    for side in ("front", "back"):
        m, L = gk.wall_frame(*frames[side])
        wl = gb[side]

        def hx(u, side=side):
            return i + u if side == "front" else W - i - u
        us = sorted({0.0, L} | {(p[0] - i) if side == "front" else (W - i - p[0]) for p in prof if i < p[0] < W - i})
        outline = [(u, under(hx(u))) for u in us]
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, gb["z0"], outline, tt, ws, hulls)
        hk.verge_band(E, outline, gb["rafter"][0], gb["rafter"][1], "Timber")
        lf = wl.get("leaves")
        for x0, x1, hz0, hz1 in (wl.get("holes", []) if lf else []):
            ly, lt = lf["y"], lf["t"]
            hk.reveal(E, (x0, x1, hz0, hz1), 0.0, ly, "Timber")
            hk.reveal(I, (x0, x1, hz0, hz1), ly + lt, tt, "Plaster")
            w, h = (x1 - x0) / 2 - 0.0025, hz1 - hz0 - 0.01
            for left in (True, False):
                def make(B, w=w, h=h, left=left):
                    hk.plank_leaf(B, w, h, lf["mat"], lf["iron"], t=lt, planks=lf["planks"], straps=tuple(lf["straps"]),
                                  hinge="l" if left else "r", ring=0.0)
                separate_leaf(E, (x0 + 0.0025) if left else (x1 - 0.0025), left, w, h, ly, hz0 + 0.005, lt, make, seed=31 + len(DOORS))
        E.pop()
        I.pop()
    # bands round the house at the storey joints, and their ledges back to the timber walls
    outer, inner = hk.plan_loop(W, D), hk.plan_loop(W, D, inset=i)
    for b in spec["bands"]:
        hk.band_loop(E, W, D, b["z0"], b["z1"], b["proud"], b["mat"])
        for which in b.get("ledges", []):
            z, nz = (b["z1"], 1.0) if which == "top" else (b["z0"], -1.0)
            for k in range(4):
                o0, o1, n0, n1 = outer[k], outer[(k + 1) % 4], inner[k], inner[(k + 1) % 4]
                hk.rect(E, (*o0, z), (*o1, z), (*n1, z), (*n0, z), b["mat"], (0, 0, nz))
    loft_roof(E, spec, prof, hulls)
    ho = spec.get("hoist")
    if ho:
        hk.hoist_crane(E, ho["x"], ho["y_wall"], ho["y_tip"], ho["z_beam"], ho["beam_h"], tuple(ho["foot"]), ho["wheel_r"],
                       tuple(ho["block"]), ho["block_r"], ho["hook_z"], ho["iron"], ho["rope"])
    # inside: one empty room from the floor up to a ceiling under the roof
    t = spec["ground"]["t"]
    hk.rect(I, (t, t, F0), (W - t, t, F0), (W - t, D - t, F0), (t, D - t, F0), "Timber", (0, 0, 1))
    c0, c1, c2, c3 = i + tt, W - i - tt, i + tt, D - i - tt
    hk.rect(I, (c0, c2, CEIL), (c1, c2, CEIL), (c1, c3, CEIL), (c0, c3, CEIL), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, F0))
    return E, I, hulls


def loft_roof(E, spec, prof, hulls):
    """The gable roof whose ridge runs front to back (house #7, split out by house #8): mansard_roof in its
    quarter-turned frame, an optional ridge cap and bargeboards over both verges."""
    W, D, CEIL = spec["width"], spec["depth"], spec["ceiling"]
    i = spec["frame"]["inset"]
    rf = spec["roof"]
    th = rf["thick"]
    # the roof: mansard_roof in its quarter-turned frame; its hull comes back in that frame's coordinates
    sub = {"width": D - 2 * i, "depth": W - 2 * i, "ceiling": CEIL, "roof": dict(rf, overhang=rf["overhang"] + i)}
    E.push(loft_roof_frame(spec))
    n = len(hulls)
    mansard_roof(E, sub, [(W - i - x, z) for x, z in reversed(prof)], hulls)
    hulls[n:] = [xf(E, h) for h in hulls[n:]]
    E.pop()
    rc = rf.get("ridge_cap")                                      # [half width, height over the ridge]: closes the seam
    if rc:
        xr, zr = max(prof, key=lambda p: p[1])
        ov = rf["overhang"]
        hk.block(E, xr - rc[0], xr + rc[0], -ov - 0.01, D + ov + 0.01, zr - rc[0] * 1.2, zr + rc[1], rf["mat"], faces="xXyYZ")
    bb = rf.get("barge")             # bargeboards on both verges: [proud, top over the slates, foot under the underside]
    if bb:
        ov = rf["overhang"]
        top = [(x, z + bb[1]) for x, z in prof]
        for yv, front in ((-ov, True), (D + ov, False)):
            m, _ = gk.wall_frame((0.0, yv), (W, yv)) if front else gk.wall_frame((W, yv), (0.0, yv))
            outl = top if front else [(W - x, z) for x, z in reversed(top)]
            E.push(m)
            hk.verge_band(E, outl, bb[1] + th + bb[2], bb[0], rf["mat"])
            for (xa, za), (xb, zb) in zip(outl, outl[1:]):
                hk.rect(E, (xa, -bb[0], za), (xb, -bb[0], zb), (xb, 0.0, zb), (xa, 0.0, za), rf["mat"], (-(zb - za), 0.0, xb - xa))
            E.pop()


# ---------------------------------------------------------------------------------------------
# townhouse_stack (house #8, GlassblowersHighDraftFurnace): a narrow ashlar house with its gable to the front (the
# ridge runs front to back, roof as townhouse_loft's); four plain stone walls (jetty_wall with a "Stone" infill:
# windows, vent slits as louvres, the door hole), proud stone trim blocks per wall (door surround, sills, heads), a
# plank door in a timber frame (separate leaf); an octagonal brick chimney stack against one side wall: a stone pier
# (with a small window), a brick taper from the pier to the octagon, the tapering shaft with iron bands, a corbelled
# cap and a sooty flue; iron tie rods along the side wall and round the corners to anchor plates. One empty shell.
# ---------------------------------------------------------------------------------------------
def stack_chimney(E, spec, hulls):
    """The chimney stack (house coordinates): pier, taper, shaft, bands, cap, flue, tie rods and anchor plates."""
    ch = spec["stack"]
    cx, cy = ch["cx"], ch["cy"]
    pr = ch["pier"]
    (px0, px1), (py0, py1), pz = pr["x"], pr["y"], pr["z1"]
    pcx, pcy, hx, hy = (px0 + px1) / 2, (py0 + py1) / 2, (px1 - px0) / 2, (py1 - py0) / 2
    hk.block(E, px0, px1, py0, py1, 0.0, pz, pr["mat"], faces="yYZ")
    m, L = gk.wall_frame((px0, py1), (px0, py0))                 # the pier's outer face, seen from outside (-x)
    E.push(m)
    holes = []
    pw = pr.get("window")
    if pw:
        u0, u1 = py1 - pw["y"][1], py1 - pw["y"][0]
        hole = (u0, u1, pw["z"][0], pw["z"][1])
        holes.append(hole)
        hk.sash_window(E, *hole, pw["frame"], "Glass", pw["cols"], pw["rows"], y=pw["rec"], depth=pw["depth"], fw=pw["fw"], meeting=False)
        hk.reveal(E, hole, 0.0, pw["rec"], pr["mat"])
        hk.block(E, u0 - 0.06, u1 + 0.06, -0.05, 0.0, hole[2] - 0.12, hole[2], pr.get("trim", pr["mat"]), faces="xXyzZ")   # a sill
    hk.flat_face(E, 0.0, L, 0.0, pz, pr["mat"], holes=holes, cell=0.6)
    E.pop()
    sh = ch["shaft"]
    (z0, r0), (z1, r1) = sh["z0"], sh["z1"]

    def r_at(z):
        return r0 + (r1 - r0) * (z - z0) / (z1 - z0)
    tp = ch["taper"]
    lo = hk.rect_ring(pcx, pcy, hx, hy, tp["chamfer"], pz)
    hi = hk.oct_ring(cx, cy, r0, z0)
    hk.ring_skin(E, lo, hi, sh["mat"])
    hk.octagon_profile(E, cx, cy, [(z0, r0), (z1, r1)] + [tuple(p) for p in ch["cap"]], sh["mat"])
    top_z, top_r = ch["cap"][-1]
    fl = ch["flue"]                                               # [apothem, depth, material]: the sooty opening
    hk.octagon_profile(E, cx, cy, [(top_z, top_r), (top_z, fl[0]), (top_z - fl[1], fl[0]), (top_z - fl[1], 0.0)], fl[2])
    for bz0, bz1 in ch["bands"]:
        hk.octagon_band(E, cx, cy, bz0, bz1, r_at(bz0), r_at(bz1), ch["band_proud"], ch["iron"])
    hulls.append(hk.box_pts(px0, px1, py0, py1, 0.0, pz))
    hulls.append([p for p in lo] + [p for p in hi])
    capr = max(r for _, r in ch["cap"])
    hulls.append(hk.oct_ring(cx, cy, r0, z0) + hk.oct_ring(cx, cy, r1, z1))
    hulls.append(hk.oct_ring(cx, cy, capr, z1) + hk.oct_ring(cx, cy, capr, top_z))
    # tie rods: along the side wall at x = rx, round the corners and along the front and back to anchor plates
    W, D = spec["width"], spec["depth"]
    for tr in ch["ties"]:
        z, rx = tr["z"], tr["x"]
        if tr.get("corners"):
            xp = tr["corners"]                                    # the anchor plates' x on the front and back faces
            hk.tie_rod(E, [(xp, rx, z), (rx, rx, z), (rx, D - rx, z), (xp, D - rx, z)], ch["iron"])
            hk.anchor_plate(E, (xp, 0.0, z), (0, -1, 0), ch["iron"])
            hk.anchor_plate(E, (xp, D, z), (0, 1, 0), ch["iron"])
        else:
            ys = tr["plates"]
            hk.tie_rod(E, [(rx, ys[0], z), (rx, ys[-1], z)], ch["iron"])
        for y in tr["plates"]:
            hk.anchor_plate(E, (0.0, y, z), (-1, 0, 0), ch["iron"])


def build_townhouse_stack(spec):
    W, D, F0, CEIL, t = spec["width"], spec["depth"], spec["floor"], spec["ceiling"], spec["wall"]
    rf = spec["roof"]
    th = rf["thick"]
    prof = sorted(tuple(p) for p in rf["profile"])                # [(house x, top of the slates)], eave to eave

    def under(x):
        return roof_z(prof, x) - th
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    ws = spec["window_style"]
    frames = {"front": ((0.0, 0.0), (W, 0.0)), "right": ((W, 0.0), (W, D)), "back": ((W, D), (0.0, D)), "left": ((0.0, D), (0.0, 0.0))}
    dr = spec["door"]
    for side in ("front", "right", "back", "left"):
        m, L = gk.wall_frame(*frames[side])
        wl = dict(spec["walls"].get(side, {}))
        wl.setdefault("infill", "Stone")
        if side in ("front", "back"):                             # the gables, up to the roof's underside
            def hx(u, side=side):
                return u if side == "front" else W - u
            us = sorted({0.0, L} | {(p[0] if side == "front" else W - p[0]) for p in prof if 0.0 < p[0] < W})
            top = [(u, under(hx(u))) for u in us]
        else:
            top = under(0.0 if side == "left" else W)
        if side == dr["side"]:
            wl["holes"] = wl.get("holes", []) + [[dr["x"][0], dr["x"][1], 0.0, dr["z1"]]]
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, 0.0, top, t, ws, hulls)
        for x0, x1, y0, y1, bz0, bz1, mat in wl.get("blocks", []):    # proud stone trim: surrounds, sills, heads
            hk.block(E, x0, x1, y0, y1, bz0, bz1, mat, faces="xXyzZ")
        if side == dr["side"]:
            x0, x1 = dr["x"]
            zh, ly, lt = dr["z1"], dr["leaf_y"], dr["leaf_t"]
            fr = dr["frame"]
            fw, fy, head = fr["w"], fr["y"], fr["head"]
            hk.reveal(E, (x0, x1, 0.0, zh), 0.0, fy, dr["reveal"], sides="lrt")
            hk.block(E, x0, x0 + fw, fy, ly + lt, 0.0, head, fr["mat"], faces="Xyz")      # the timber frame
            hk.block(E, x1 - fw, x1, fy, ly + lt, 0.0, head, fr["mat"], faces="xyz")
            hk.block(E, x0, x1, fy, ly + lt, head, zh, fr["mat"], faces="yz")
            hk.reveal(I, (x0, x1, F0, zh), ly + lt, t, "Plaster", sides="lrt")
            hk.block(E, x0, x1, -0.02, t, 0.0, F0, dr["reveal"], faces="yZ")                # threshold
            lx0, lx1 = x0 + fw, x1 - fw
            w, h = lx1 - lx0 - 0.01, head - F0 - 0.005
            left = dr.get("hinge", "left") == "left"

            def make(B, w=w, h=h, left=left):
                hk.plank_leaf(B, w, h, dr["mat"], dr["iron"], t=lt, planks=dr["planks"], straps=tuple(dr["straps"]),
                              hinge="l" if left else "r", ring=dr.get("ring", 0.45))
            separate_leaf(E, (lx0 + 0.005) if left else (lx1 - 0.005), left, w, h, ly, F0, lt, make, seed=31 + len(DOORS))
        E.pop()
        I.pop()
    loft_roof(E, dict(spec, frame={"inset": 0.0}), prof, hulls)
    stack_chimney(E, spec, hulls)
    # inside: one empty room from the floor up to a ceiling under the roof
    hk.rect(I, (t, t, F0), (W - t, t, F0), (W - t, D - t, F0), (t, D - t, F0), "Timber", (0, 0, 1))
    hk.rect(I, (t, t, CEIL), (W - t, t, CEIL), (W - t, D - t, CEIL), (t, D - t, CEIL), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(t, W - t, t, D - t, -0.3, F0))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_tavern (house #9, HighSpireTavern): a half-timbered shop house with its gable to the street and a stone
# tower at its back-left corner. A dark timber shopfront round the front and both sides of the front block
# (pilasters, shop windows on stallrisers, a glazed door under a transom, fascia, frieze, consoles, a bressummer);
# a jettied timber first floor (jetty_wall) with a canted oriel bay on corbels; the front gable jettied again over a
# bressummer, framed, with a verge rafter and bargeboards; a gable roof whose ridge runs front to back, its left slope
# cut back to the tower; a stone back block (jetty_wall, Stone) up to the eaves and the back gable; the square stone
# tower with quoins, round-headed windows high up, a round-headed door on its left face reached by a stone stair
# (front steps and a sloping side wall), a hipped bell-cast spire with a gabled dormer on every face, a short ridge
# and two spike finials. One empty shell inside.
# ---------------------------------------------------------------------------------------------
def shop_wall(E, I, wl, L, t, F0, sp, hulls):
    """One side of the shopfront in wall coordinates (pilaster faces on y = 0): pilasters, plaster spans, the recessed
    face (y = sp["face"]) with shop windows on sunk-panel stallrisers and (front) the door with its transom and a
    separate glazed leaf, the core, inner face, inner reveals and collision. Fascia, frieze and beam are separate."""
    fy, mat, head = sp["face"], sp["mat"], sp["head"]
    g0, g1 = sp["glass"]
    pz, pp = sp["plinth"]
    u0, u1 = wl.get("span", [0.0, L])
    solid = [tuple(p) for p in wl.get("pilasters", [])] + [tuple(p) for p in wl.get("plaster", [])]
    holes, inner = [], []
    for x0, x1 in wl.get("pilasters", []):
        hk.block(E, x0, x1, 0.0, fy, pz, head, mat, faces="xXy")
        hk.block(E, x0 - 0.02, x1 + 0.02, -pp, fy, 0.0, pz, mat, faces="xXyZ")
    for x0, x1 in wl.get("plaster", []):
        hk.flat_face(E, x0, x1, pz, head, "Plaster", cell=0.6)
        hk.block(E, x0, x1, -pp, 0.0, 0.0, pz, mat, faces="yZ")
    rs = sp["riser"]
    s0, s1, sproud = sp["sill"]
    for x0, x1, cols, rows in wl.get("windows", []):
        hk.sash_window(E, x0, x1, g0, g1, mat, "Glass", cols, rows, y=fy, depth=0.10, fw=0.07, meeting=False)
        hk.block(E, x0, x1, fy - sproud, fy + 0.10, s0, s1, mat, faces="xXyzZ")
        panel = (x0 + rs["margin"], x1 - rs["margin"], pz + rs["margin"], s0 - rs["margin"])
        holes.append(panel)
        hk.flat_face(E, *panel, mat, y=fy + rs["sunk"])
        hk.reveal(E, panel, fy, fy + rs["sunk"], mat)
        hk.block(E, x0, x1, fy - pp, fy, 0.0, pz, mat, faces="yZ")
        hk.reveal(I, (x0, x1, g0, g1), fy + 0.10, t, "Plaster")
        holes.append((x0, x1, g0, g1))
        inner.append((x0, x1, g0, g1))
    dr = wl.get("door")
    if dr:
        d = sp["door"]
        x0, x1 = dr
        r0, r1 = d["rail"]
        ly, lt = d["leaf_y"], d["leaf_t"]
        hk.block(E, x0, x1, fy - 0.03, fy + 0.10, r0, r1, mat, faces="yzZ")
        hk.sash_window(E, x0, x1, r1, g1, mat, "Glass", d["transom"], 1, y=fy, depth=0.08, fw=0.05, meeting=False)
        hk.reveal(E, (x0, x1, 0.0, r0), fy, ly, mat, sides="lr")
        hk.reveal(I, (x0, x1, F0, g1), ly + lt, t, "Plaster", sides="lrt")
        sw, smat = d["step"]
        hk.block(E, x0 - 0.05, x1 + 0.05, -sw, t, 0.0, F0, smat, faces="xXyZ")
        w, h = x1 - x0 - 0.01, r0 - F0 - 0.005
        left = d.get("hinge", "left") == "left"

        def make(B, w=w, h=h):
            hk.glazed_leaf(B, w, h, mat, "Glass", mat, t=lt, light=tuple(d["light"]), grid=tuple(d["grid"]), panel_z=tuple(d["panels"]))
        separate_leaf(E, (x0 + 0.005) if left else (x1 - 0.005), left, w, h, ly, F0, lt, make, seed=31 + len(DOORS))
        holes.append((x0, x1, 0.0, g1))
        inner.append((x0, x1, F0, g1))
    # the recessed face between the pilasters and plaster spans, up to the fascia
    spans = [(u0, u1)]
    for a, b in solid:
        spans = [s for sa, sb in spans for s in ((sa, min(sb, a)), (max(sa, b), sb)) if s[1] - s[0] > 1e-6]
    for a, b in spans:
        hk.flat_face(E, a, b, 0.0, head, mat, holes=[h for h in holes if a - 1e-6 <= h[0] and h[1] <= b + 1e-6], y=fy)
    top = sp["beam"][1]
    hk.wall_core(E, L, 0.0, top, t, holes=inner)
    hk.flat_face(I, u0, u1, F0, top, "Plaster", holes=inner, y=t, inward=True)
    if dr:
        x0, x1 = dr
        hulls += [xf(E, hk.box_pts(u0, x0, 0.0, t, 0.0, top)), xf(E, hk.box_pts(x1, u1, 0.0, t, 0.0, top)),
                  xf(E, hk.box_pts(x0, x1, 0.0, t, g1, top))]
    else:
        hulls.append(xf(E, hk.box_pts(u0, u1, 0.0, t, 0.0, top)))


def tavern_shop(E, I, spec, hulls):
    """The shopfront on the front and both sides of the front block, the corner pilasters, and (house coordinates) the
    fascia, frieze and beam round it, and the consoles over the pilasters."""
    W, F0 = spec["width"], spec["floor"]
    sp = spec["shop"]
    t, fy, mat = sp["t"], sp["face"], sp["mat"]
    Y = sp["y1"]                                                  # the front block's sides end at the tower / back block
    frames = {"front": ((0.0, 0.0), (W, 0.0)), "right": ((W, 0.0), (W, Y)), "left": ((0.0, Y), (0.0, 0.0))}
    for side, (p0, p1) in frames.items():
        m, L = gk.wall_frame(p0, p1)
        E.push(m)
        I.push(m)
        shop_wall(E, I, sp["walls"][side], L, t, F0, sp, hulls)
        cn = sp["console"]
        for x0, x1 in sp["walls"][side].get("pilasters", []) + sp["walls"][side].get("consoles", []):
            hk.console(E, (x0 + x1) / 2, 0.0, sp["frieze"][1], mat, out=cn["out"], height=cn["height"], width=cn["width"])
        E.pop()
        I.pop()
    pz, pp = sp["plinth"]
    head = sp["head"]
    for x0, x1, y0, y1 in sp["corners"]:                          # the corner pilasters, one block each
        hk.block(E, x0, x1, y0, y1, pz, head, mat, faces="xXyY")
        hk.block(E, x0 - (pp if x0 < 0.1 else 0.0), x1 + (pp if x1 > W - 0.1 else 0.0), y0 - pp, y1, 0.0, pz, mat, faces="xXyYZ")
    # fascia, frieze and beam round the front block: [z0, z1, proud], each back to the recessed face
    for (z0, z1, p), m_ in ((sp["fascia"], mat), (sp["frieze"], mat), (sp["beam"][:3], sp["beam"][3])):
        pr = sp.get("beam_right", W + p) if m_ == sp["beam"][3] else W + p
        hk.block(E, -p, pr, -p, fy, z0, z1, m_, faces="xXyzZ")
        hk.block(E, -p, fy, fy, Y, z0, z1, m_, faces="xzZY")
        hk.block(E, W - fy, pr, fy, Y, z0, z1, m_, faces="XzZY")


def tavern_oriel(E, I, spec, hulls):
    """The canted oriel bay on the first floor (house coordinates): its three glazed faces (timber frames, mullions,
    casements), the sill shelf and top block, the floor and roof plates, corbels under it, the inside faces."""
    o = spec["oriel"]
    P = [tuple(p) for p in o["plan"]]                            # back-left, front-left, front-right, back-right
    z0, z1 = o["glass"]
    b0, b1 = o["shelf"]
    c0, c1 = o["top"]
    ov = o["outset"]
    for k in range(3):
        m, L = gk.wall_frame(P[k], P[k + 1])
        n, mw, ew = o["lights"][k], o["mullion"], o["end"]
        lw = (L - 2 * ew - (n - 1) * mw) / n
        holes = [(ew + j * (lw + mw), ew + j * (lw + mw) + lw, z0 + 0.06, z1 - 0.06) for j in range(n)]
        E.push(m)
        I.push(m)
        hk.flat_face(E, 0.0, L, z0, z1, "Timber", holes=holes)
        for h in holes:
            hk.sash_window(E, *h, "Metal", "Glass", o["panes"][k][0], o["panes"][k][1], y=0.03, depth=0.05, fw=0.05, meeting=False)
            hk.reveal(E, h, 0.0, 0.03, "Timber")
            hk.reveal(E, h, 0.08, 0.10, "Timber")
        hk.flat_face(I, 0.0, L, z0, z1, "Timber", holes=holes, y=0.10, inward=True)
        ext = 0.0 if k == 1 else ov
        hk.block(E, -ext, L + ext, -ov, 0.10, b0, b1, "Timber", faces="xXyzZ")
        hk.block(E, -ext, L + ext, -ov, 0.10, c0, c1, "Timber", faces="xXyzZ")
        E.pop()
        I.pop()
    E.fan([(x, y, b0) for x, y in P], "Timber", (0, 0, -1))
    E.fan([(x, y, c1) for x, y in P], "Timber", (0, 0, 1))
    I.fan([(x, y, b1) for x, y in P], "Timber", (0, 0, 1))
    I.fan([(x, y, c0) for x, y in P], "Plaster", (0, 0, -1))
    for x0, x1 in o["corbels"]["x"]:
        y0, y1 = o["corbels"]["y"]
        hk.block(E, x0, x1, y0, y1, o["corbels"]["z"][0], b0, "Timber", faces="xXyz")
    hulls.append([(x, y, z) for x, y in P for z in (b0, c1)])


def tavern_roof(E, spec, prof, hulls):
    """The main gable roof, its ridge front to back: slates on the left slope in front of the tower, on the left slope
    behind it from the tower's right face up, and on the right slope; undersides, verge and eave edges, a ridge cap,
    bargeboards on the front verge and the back one, a spike finial at the front of the ridge."""
    rf = spec["roof"]
    th, mat, st = rf["thick"], rf["mat"], rf["step"]
    (xl, zl), (xm, zm), (xe, ze) = prof
    y0, y1 = rf["front"], rf["back"]
    cx, cy = rf["cut"]
    zc = roof_z(prof, cx)
    s = (zm - zl) / (xm - xl)
    parts = [((xl, zl), y0, cy, 70), ((cx, zc), cy, y1, 74), ((xe, ze), y0, y1, 78)]
    for (xa, za), ya, yb, seed in parts:
        gk.tile_facet(E, mat, (xa, ya, za), (xa, yb, za), (xm, yb, zm), (xm, ya, zm), pitch=rf["pitch"], step=st, seed=seed)
        dn = (s, 0.0, -1.0) if xa < xm else (-s, 0.0, -1.0)
        hk.rect(E, (xa, ya, za - th), (xa, yb, za - th), (xm, yb, zm - th), (xm, ya, zm - th), mat, dn)
        for yv, ny in ((ya, -1.0), (yb, 1.0)):                    # verge edges (the cut one sits against the tower)
            if (ny < 0 and abs(yv - y0) < 1e-6) or (ny > 0 and abs(yv - y1) < 1e-6):
                hk.rect(E, (xa, yv, za - th), (xm, yv, zm - th), (xm, yv, zm + st), (xa, yv, za + st), mat, (0, ny, 0))
        hulls.append([(x, y, z) for x, z0 in ((xa, za), (xm, zm)) for y in (ya, yb) for z in (z0 - th, z0)])
    for (x, z), ya, yb, nx in (((xl, zl), y0, cy, -1.0), ((xe, ze), y0, y1, 1.0)):     # eave edges
        hk.rect(E, (x, ya, z - th), (x, yb, z - th), (x, yb, z + st), (x, ya, z + st), mat, (nx, 0, 0))
    rc = rf["ridge_cap"]
    hk.block(E, xm - rc[0], xm + rc[0], y0 - 0.01, y1 + 0.01, zm - rc[0] * 1.2, zm + rc[1], mat, faces="xXyYZ")
    bb = rf["barge"]                                              # [proud, top over the slates, foot under the underside]
    for yv, front, pts in ((y0, True, prof), (y1, False, [(cx, zc), (xm, zm), (xe, ze)])):
        top = [(x, z + bb[1]) for x, z in pts]
        m, _ = gk.wall_frame((0.0, yv), (10.0, yv)) if front else gk.wall_frame((10.0, yv), (0.0, yv))
        outl = top if front else [(10.0 - x, z) for x, z in reversed(top)]
        E.push(m)
        hk.verge_band(E, outl, bb[1] + th + bb[2], bb[0], mat)
        for (xa, za), (xb, zb) in zip(outl, outl[1:]):
            hk.rect(E, (xa, -bb[0], za), (xb, -bb[0], zb), (xb, 0.0, zb), (xa, 0.0, za), mat, (-(zb - za), 0.0, xb - xa))
        E.pop()
    fn = rf["finial"]
    hk.spike_finial(E, xm, y0 - bb[0] / 2, zm + rc[1] - 0.02, fn["top"], fn["mat"], r=fn["r"])


def tavern_tower(E, I, spec, hulls):
    """The stone tower: four walls (jetty_wall, Stone) with quoins, round-headed windows, slits and the round-headed
    door on one face (a separate plank leaf under a fixed timber tympanum), the inside floor and ceiling."""
    tw = spec["tower"]
    X0, X1, Y0, Y1 = tw["box"]
    t, Z1, F0 = tw["t"], tw["z1"], spec["floor"]
    ws = spec["stone_window_style"]
    frames = {"front": ((X0, Y0), (X1, Y0)), "right": ((X1, Y0), (X1, Y1)), "back": ((X1, Y1), (X0, Y1)), "left": ((X0, Y1), (X0, Y0))}
    qn = tw["quoins"]
    for k, side in enumerate(("front", "right", "back", "left")):
        m, L = gk.wall_frame(*frames[side])
        fc = tw["faces"].get(side, {})
        wl = {"infill": "Stone", "louvres": fc.get("slits", []), "louvre_mat": "Metal"}
        arches = fc.get("windows", []) + ([fc["door"]] if "door" in fc else [])
        wl["holes"] = [[a["xc"] - a["r"], a["xc"] + a["r"], a["z0"], a["zs"] + a["r"]] for a in arches]
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, 0.0, Z1, t, ws, hulls)
        hk.quoin_corner(E, 0.0, Z1, qn["course"], qn["long"], qn["short"], qn["proud"], qn["mat"], start_long=(k % 2 == 0))
        for a in fc.get("windows", []):
            xc, z0, zs, r = a["xc"], a["z0"], a["zs"], a["r"]
            hk.arched_opening(E, I, xc, z0, zs, r, t, a["rec"], "Stone", trim=a["trim"], ring_w=a["ring"], proud=a["proud"])
            hk.sash_window(E, xc - r, xc + r, z0, zs, a["frame"], "Glass", a["cols"], a["rows"], y=a["rec"], depth=0.06, fw=0.05, meeting=False)
            hk.fanlight(E, xc, zs, r, a["rec"] + 0.03, "Glass", a["frame"], spokes=a["spokes"])
        dr = fc.get("door")
        if dr:
            xc, z0, zs, r = dr["xc"], dr["z0"], dr["zs"], dr["r"]
            ly, lt = dr["leaf_y"], dr["leaf_t"]
            hk.arched_opening(E, I, xc, z0, zs, r, t, ly, "Stone", trim=dr["trim"], ring_w=dr["ring"], proud=dr["proud"], sill_h=0.0,
                              inner_from=ly + lt)
            arc = hk.arc_pts(xc, zs, r - 0.005, n=24)
            E.fan([(x, ly, z) for x, z in arc], dr["mat"], (0, -1, 0))               # the fixed tympanum over the leaf
            E.fan([(x, ly + lt, z) for x, z in arc], dr["mat"], (0, 1, 0))
            hk.reveal(E, (xc - r, xc + r, z0, zs), -dr["proud"], ly, "Stone", sides="b")
            hk.block(E, xc - r, xc + r, ly, ly + lt, zs - 0.06, zs, dr["mat"], faces="yYz")
            w, h = 2 * r - 0.01, zs - 0.06 - z0 - 0.005

            def make(B, w=w, h=h):
                hk.plank_leaf(B, w, h, dr["mat"], dr["iron"], t=lt, planks=dr["planks"], straps=tuple(dr["straps"]), hinge="l", ring=0.45)
            separate_leaf(E, xc - r + 0.005, True, w, h, ly, z0, lt, make, seed=31 + len(DOORS))
            hulls.pop()                                           # the wall's hull, replaced by three round the doorway
            hulls += [xf(E, hk.box_pts(0.0, xc - r, 0.0, t, 0.0, Z1)), xf(E, hk.box_pts(xc + r, L, 0.0, t, 0.0, Z1)),
                      xf(E, hk.box_pts(xc - r, xc + r, 0.0, t, zs + r, Z1)), xf(E, hk.box_pts(xc - r, xc + r, 0.0, t, 0.0, z0))]
        E.pop()
        I.pop()
    hk.rect(I, (X0 + t, Y0 + t, F0), (X1 - t, Y0 + t, F0), (X1 - t, Y1 - t, F0), (X0 + t, Y1 - t, F0), "Timber", (0, 0, 1))
    zc = tw["ceiling"]
    hk.rect(I, (X0 + t, Y0 + t, zc), (X1 - t, Y0 + t, zc), (X1 - t, Y1 - t, zc), (X0 + t, Y1 - t, zc), "Plaster", (0, 0, -1))
    hulls.append(hk.box_pts(X0 + t, X1 - t, Y0 + t, Y1 - t, -0.3, F0))


def tavern_spire(E, spec, hulls):
    """The hipped bell-cast spire on the tower (house_kit.spire_rings), a gabled dormer on every face
    (mansard_dormer in that face's frame), a ridge cap and a spike finial at each end of the ridge."""
    tw, sp = spec["tower"], spec["spire"]
    X0, X1, Y0, Y1 = tw["box"]
    rings = [tuple(r) for r in sp["rings"]]
    cs = hk.spire_rings(E, X0, X1, Y0, Y1, rings, "Roof", tw["z1"], pitch=sp["pitch"], step=sp["step"])
    hulls.append([p for c in (cs[0], cs[-1]) for p in c] + [(p[0], p[1], tw["z1"]) for p in cs[0]])
    frames = {"front": ((X0, Y0), (X1, Y0)), "right": ((X1, Y0), (X1, Y1)), "back": ((X1, Y1), (X0, Y1)), "left": ((X0, Y1), (X0, Y0))}
    dm = sp["dormer"]
    for side in ("front", "right", "back", "left"):
        m, L = gk.wall_frame(*frames[side])
        k = 1 if side in ("front", "back") else 2
        prof = [(rg[k], rg[0]) for rg in rings]
        sub = {"dormers": dict(dm, roof_back=dm["roof_back"][side in ("right", "left")])}
        E.push(m)
        n = len(hulls)
        mansard_dormer(E, sub, prof, L / 2, hulls)
        del hulls[n:]                                             # inside the spire's hull
        E.pop()
    z, iy, ix = rings[-1]
    rc = sp["ridge_cap"]
    yc = (Y0 + Y1) / 2
    hk.block(E, X0 + ix - 0.02, X1 - ix + 0.02, yc - rc[0], yc + rc[0], z - 0.02, z + rc[1], "Roof", faces="xXyYZ")
    fn = sp["finials"]
    for x in fn["x"]:
        hk.spike_finial(E, x, yc, z + rc[1] - 0.02, fn["top"], fn["mat"], r=fn["r"])


def tavern_stair(E, spec, hulls):
    """The stone stair to the tower door: a flight of steps in front of the tower between its side wall and the shop,
    with a landing at the tower, and the sloping side wall along the tower's face up to the door."""
    st = spec["stair"]
    x0, x1 = st["x"]
    ys, ye, yl = st["y"]                                          # first riser, top of the flight, the tower's face
    n, zt = st["risers"], st["top"]
    for k in range(n):
        ya = ys + (ye - ys) * k / n
        hk.block(E, x0, x1, ya, yl, 0.0, zt * (k + 1) / n, st["mat"], faces="yZ")
    hulls.append([(x, y, z) for x in (x0, x1) for y, z in ((ys, 0.0), (ye, zt), (yl, zt), (yl, 0.0))])
    sw = st["wall"]
    a, b = sw["x"]
    poly = [tuple(p) for p in sw["profile"]]                      # (y, z), the foot first, round to the back
    for x, nx in ((a, -1.0), (b, 1.0)):
        E.fan([(x, y, z) for y, z in poly], st["mat"], (nx, 0, 0))
    for (ya, za), (yb, zb) in zip(poly, poly[1:] + poly[:1]):
        if za < 1e-6 and zb < 1e-6:
            continue                                              # the foot on the ground
        hk.rect(E, (a, ya, za), (b, ya, za), (b, yb, zb), (a, yb, zb), st["mat"], (0.0, -(zb - za), yb - ya))
    hulls.append([(x, y, z) for x in (a, b) for y, z in poly])


def build_townhouse_tavern(spec):
    W, D, F0, CEIL = spec["width"], spec["depth"], spec["floor"], spec["ceiling"]
    rf = spec["roof"]
    th = rf["thick"]
    prof = sorted(tuple(p) for p in rf["profile"])                # [(house x, top of the slates)], eave, ridge, eave

    def under(x):
        return roof_z(prof, x) - th
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    ws = spec["window_style"]
    tavern_shop(E, I, spec, hulls)
    # the jettied first floor: front and both sides (they end against the tower and the back block)
    ff = spec["first"]
    (xa, xb), yf, Y = ff["x"], ff["front"], spec["shop"]["y1"]
    z0, z1 = ff["z"]
    frames = {"front": ((xa, yf), (xb, yf)), "right": ((xb, yf), (xb, Y)), "left": ((xa, Y), (xa, yf))}
    for side, (p0, p1) in frames.items():
        m, L = gk.wall_frame(p0, p1)
        wl = ff["walls"][side]
        if "mirror" in wl:
            wl = jetty_mirror(ff["walls"][wl["mirror"]], L)
        top = z1 if side == "front" else under(p0[0])
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, z0, top, ff["t"], ws, hulls)
        E.pop()
        I.pop()
    tavern_oriel(E, I, spec, hulls)
    for c in ff.get("consoles", []):                              # brackets under the gable's jetty at the corners
        hk.console(E, c["x"], c["y"], c["z_top"], "Timber", out=c["out"], height=c["height"], width=c["width"])
    # the front gable over its bressummer, framed up to the roof's underside under a verge rafter
    gb = spec["gable"]
    ga, gbx = gb["x"]
    bx0, bx1, by0, by1, bz0, bz1 = gb["bressummer"]
    hk.block(E, bx0, bx1, by0, by1, bz0, bz1, "Timber", faces="xXyzZ")
    m, L = gk.wall_frame((ga, gb["y"]), (gbx, gb["y"]))
    us = sorted({0.0, L} | {p[0] - ga for p in prof if ga < p[0] < gbx})
    outline = [(u, under(u + ga)) for u in us]
    E.push(m)
    I.push(m)
    jetty_wall(E, I, gb["wall"], L, bz1, outline, gb["t"], ws, hulls)
    hk.verge_band(E, outline, gb["rafter"][0], gb["rafter"][1], "Timber")
    E.pop()
    I.pop()
    # the stone back block: its right side under the eave, the back gable, the return at the shop's side
    bk = spec["back_block"]
    x0b, y0b = bk["x0"], bk["y0"]
    xr = bk["x1"]
    sws = spec["stone_window_style"]
    for side in ("right", "back"):
        if side == "right":
            m, L = gk.wall_frame((xr, y0b), (xr, D))
            top = under(xr)
        else:
            m, L = gk.wall_frame((xr, D), (x0b, D))
            us = sorted({0.0, L} | {xr - p[0] for p in prof if x0b < p[0] < xr})
            top = [(u, under(xr - u)) for u in us]
        wl = dict(bk["walls"].get(side, {}), infill="Stone")
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, 0.0, top, bk["t"], sws, hulls)
        E.pop()
        I.pop()
    rx0 = bk["return_x0"]
    hk.flat_face(E, rx0, xr, 0.0, under(xr), "Stone", y=y0b)
    dp = bk.get("downpipe")
    if dp:
        hk.block(E, *dp, "Metal", faces="xXyYZ")
    tavern_roof(E, spec, prof, hulls)
    tavern_tower(E, I, spec, hulls)
    tavern_spire(E, spec, hulls)
    tavern_stair(E, spec, hulls)
    # inside: the floor of the shop and back block, and one ceiling under the attic
    for x0, x1, y0, y1 in spec["inside"]["floor"]:
        hk.rect(I, (x0, y0, F0), (x1, y0, F0), (x1, y1, F0), (x0, y1, F0), "Timber", (0, 0, 1))
        hulls.append(hk.box_pts(x0, x1, y0, y1, -0.3, F0))
    for x0, x1, y0, y1 in spec["inside"]["ceiling"]:
        hk.rect(I, (x0, y0, CEIL), (x1, y0, CEIL), (x1, y1, CEIL), (x0, y1, CEIL), "Plaster", (0, 0, -1))
    return E, I, hulls


# ---------------------------------------------------------------------------------------------
# townhouse_gate (house #10, WaterWardensGatehouse): a stone gate block on battered corner and central buttresses,
# two flattened (elliptical) round arches running through it from the front to the back under barrel vaults,
# arch rings, impost bands round the piers, blind round-arched niches on the sides (the Meshy cross passages), a
# string course, a crenellated parapet (merlons with caps) round a slate-covered walk, an iron railing across the back
# arches (the plot ends at a drop); on the walk a timber watch room: plank walls (jetty_wall with a plank infill,
# dark posts, rails and top plates), round-headed glazed windows, scroll brackets under the eaves, a hipped slate
# roof (spire_rings), a ridge cap and two spike finials. The watch room is the one empty space inside.
# ---------------------------------------------------------------------------------------------
def arched_glass(B, xc, z0, zs, r, y, mat="Glass"):
    """Two-sided glass (2 mm apart) filling a round-headed opening in the plane y (wall coordinates)."""
    arc = hk.arc_pts(xc, zs, r, n=24)
    for yy, want in ((y - 0.001, (0, -1, 0)), (y + 0.001, (0, 1, 0))):
        hk.rect(B, (xc - r, yy, z0), (xc + r, yy, z0), (xc + r, yy, zs), (xc - r, yy, zs), mat, want)
        B.fan([(x, yy, z) for x, z in arc], mat, want)


def gate_block(E, spec, hulls):
    """The stone gate block (house coordinates): buttresses, faces with the arch and niche holes, the through
    passages (pier walls, elliptical vaults), arch rings, impost bands, string course, parapet, merlons, walk."""
    g = spec["gate"]
    W, D = spec["width"], spec["depth"]
    bx0, bx1, by0, by1 = g["body"]
    dep = by1 - by0
    st, tr = g["stone"], g["trim"]
    im = g["impost"]
    iz0, iz1, ip = im["z"][0], im["z"][1], im["proud"]
    rg = g["ring"]
    pp = g["parapet"]
    t, sill, walk = pp["t"], pp["sill"], pp["walk"]
    frames = {"front": ((bx0, by0), (bx1, by0)), "right": ((bx1, by0), (bx1, by1)),
              "back": ((bx1, by1), (bx0, by1)), "left": ((bx0, by1), (bx0, by0))}
    # --- buttresses: corner ones battered on both outer faces, central ones on the front / back face only
    bt = g["buttress"]
    cw, cd, zb = bt["corner"][0], bt["corner"][1], iz0
    for sx in (1, -1):
        for sy in (1, -1):
            def P(x, y, z, sx=sx, sy=sy):                  # corner-local (x, y from the corner outwards = negative)
                return (x if sx > 0 else W - x, y if sy > 0 else D - y, z)
            xo0, yo0 = 0.0, 0.0                            # the foot's outer corner
            xo1, yo1 = bx0 - ip, by0 - ip                  # the top's outer line (flush with the impost band)
            xi, yi = cw, cd
            pts = [P(xo0, yo0, 0.0), P(xi, yo0, 0.0), P(xi, yi, 0.0), P(xo0, yi, 0.0),
                   P(xo1, yo1, zb), P(xi, yo1, zb), P(xi, yi, zb), P(xo1, yi, zb)]
            a0, b0, c0, d0, a1, b1, c1, d1 = pts
            hk.rect(E, a0, b0, b1, a1, mat=st, want=(0, -sy, 0.4))                         # front slope
            hk.rect(E, a0, d0, d1, a1, mat=st, want=(-sx, 0, 0.4))                         # side slope
            hk.rect(E, b0, P(xi, by0, 0.0), P(xi, by0, zb), b1, mat=st, want=(sx, 0, 0))   # inner face across x
            hk.rect(E, d0, P(bx0, yi, 0.0), P(bx0, yi, zb), d1, mat=st, want=(0, sy, 0))   # inner face across y
            hulls.append([p for p in pts])
    for xc in bt["central"]:
        x0, x1 = xc - bt["central_half"], xc + bt["central_half"]
        for sy in (1, -1):
            yf = 0.0 if sy > 0 else D
            yt = (by0 - ip) if sy > 0 else (by1 + ip)
            yw = by0 if sy > 0 else by1
            hk.rect(E, (x0, yf, 0.0), (x1, yf, 0.0), (x1, yt, zb), (x0, yt, zb), st, (0, -sy, 0.4))
            for x, nx in ((x0, -1), (x1, 1)):
                hk.rect(E, (x, yf, 0.0), (x, yw, 0.0), (x, yw, zb), (x, yt, zb), st, (nx, 0, 0))
            hulls.append([(x, y, z) for x in (x0, x1) for y, z in ((yf, 0.0), (yw, 0.0), (yt, zb), (yw, zb))])
    # --- the four faces up to the parapet's sill, with the arch (front, back) and niche (sides) holes
    arches = g["arches"]                                   # [{"xc", "r", "rz"}] in house x; spring at the impost top
    zs = iz1
    nc = g["niche"]
    for side, (p0, p1) in frames.items():
        m, L = gk.wall_frame(p0, p1)
        E.push(m)
        if side in ("front", "back"):
            loc = [(a["xc"] - bx0) if side == "front" else (bx1 - a["xc"]) for a in arches]
            holes = [(xc - a["r"], xc + a["r"], 0.0, zs + a["rz"]) for xc, a in zip(loc, arches)]
            hk.flat_face(E, 0.0, L, 0.0, sill, st, holes=holes, cell=0.6)
            for xc, a in zip(loc, arches):
                r, rz = a["r"], a["rz"]
                hk.arch_spandrels(E, xc, zs, r, xc - r, xc + r, zs + rz, 0.0, st, rz=rz)
                hk.arch_ring(E, xc, zs, r, r + rg["w"], -rg["proud"], tr, rz0=rz, rz1=rz + rg["crown"])
                if side == "front":                        # the passage: pier walls and the vault, front to back
                    hk.reveal(E, (xc - r, xc + r, 0.0, zs), 0.0, dep, st, sides="lr")
                    hk.arch_intrados(E, xc, zs, r, -rg["proud"], dep + rg["proud"], st, rz=rz)
            # impost bands over the piers, wrapping the corners
            edges = sorted([0.0 - ip, L + ip] + [v for xc, a in zip(loc, arches) for v in (xc - a["r"], xc + a["r"])])
            for k in range(0, len(edges), 2):
                hk.block(E, edges[k], edges[k + 1], -ip, 0.0, iz0, iz1, tr, faces="xXyzZ")
        else:
            xc, r = L / 2, nc["r"]
            hole = (xc - r, xc + r, 0.0, zs + r)
            hk.flat_face(E, 0.0, L, 0.0, sill, st, holes=[hole], cell=0.6)
            hk.arch_spandrels(E, xc, zs, r, xc - r, xc + r, zs + r, 0.0, st)
            hk.arch_ring(E, xc, zs, r, r + nc["ring"], -rg["proud"], tr)
            hk.reveal(E, hole, 0.0, nc["depth"], st, sides="lr")
            hk.arch_intrados(E, xc, zs, r, -rg["proud"], nc["depth"], st)
            hk.rect(E, (xc - r, nc["depth"], 0.0), (xc + r, nc["depth"], 0.0), (xc + r, nc["depth"], zs), (xc - r, nc["depth"], zs), st, (0, -1, 0))
            E.fan([(x, nc["depth"], z) for x, z in hk.arc_pts(xc, zs, r, n=24)], st, (0, -1, 0))
            hk.block(E, 0.0, xc - r, -ip, 0.0, iz0, iz1, tr, faces="XyzZ")
            hk.block(E, xc + r, L, -ip, 0.0, iz0, iz1, tr, faces="xyzZ")
        E.pop()
    # --- string course right round the body
    sc = g["string"]
    E.push(gk.translate(bx0, by0, 0.0))
    hk.band_loop(E, bx1 - bx0, dep, sc["z"][0], sc["z"][1], sc["proud"], tr)
    E.pop()
    # --- parapet: inner faces, the sill's top, the walk, merlons with caps
    ix0, ix1, iy0, iy1 = bx0 + t, bx1 - t, by0 + t, by1 - t
    outer = [(bx0, by0), (bx1, by0), (bx1, by1), (bx0, by1)]
    inner = [(ix0, iy0), (ix1, iy0), (ix1, iy1), (ix0, iy1)]
    ins = [(0, 1, 0), (-1, 0, 0), (0, -1, 0), (1, 0, 0)]
    for k in range(4):
        a, b = inner[k], inner[(k + 1) % 4]
        hk.rect(E, (*a, walk), (*b, walk), (*b, sill), (*a, sill), st, ins[k])
        o0, o1 = outer[k], outer[(k + 1) % 4]
        hk.rect(E, (*o0, sill), (*o1, sill), (*b, sill), (*a, sill), st, (0, 0, 1))
    wr = spec["watch"]
    rx0, rx1, ry0, ry1 = wr["planks"]                       # the plank planes: the walk runs up to them
    for x0, x1, y0, y1 in ((ix0, ix1, iy0, ry0), (ix0, ix1, ry1, iy1), (ix0, rx0, ry0, ry1), (rx1, ix1, ry0, ry1)):
        hk.rect(E, (x0, y0, walk), (x1, y0, walk), (x1, y1, walk), (x0, y1, walk), g["walk_mat"], (0, 0, 1))
    mz1, (cz, cp) = pp["merlon_top"], pp["cap"]
    blocks = []
    fc, sc_ = pp["corner"]                                  # corner merlons: length along the front / along the side
    for x0, x1 in ((bx0, bx0 + fc), (bx1 - fc, bx1)):
        for y0, y1 in ((by0, by0 + sc_), (by1 - sc_, by1)):
            blocks.append((x0, x1, y0, y1))
    for x0, x1 in pp["front"]:                              # the others, in house x (front and back alike)
        blocks += [(x0, x1, by0, iy0), (x0, x1, iy1, by1)]
    for y0, y1 in pp["side"]:
        blocks += [(bx0, ix0, y0, y1), (ix1, bx1, y0, y1)]
    for x0, x1, y0, y1 in blocks:
        hk.block(E, x0, x1, y0, y1, sill, mz1, st, faces="xXyY")
        hk.block(E, x0 - cp, x1 + cp, y0 - cp, y1 + cp, mz1, mz1 + cz, tr, faces="xXyYzZ")
    # --- an iron railing across each back arch, over the drop behind the plot
    rl = g.get("railing")
    if rl:
        for a in arches:
            x0, x1 = a["xc"] - a["r"], a["xc"] + a["r"]
            hk.iron_railing(E, x0, x1, by1 - rl["in"], 0.0, rl["h"], rl["mat"], pitch=rl["pitch"], ends=False)
            hulls.append(hk.box_pts(x0, x1, by1 - rl["in"] - 0.05, by1 - rl["in"] + 0.05, 0.0, rl["h"]))
    # --- collision: piers, the masonry over the vaults, the parapet
    xs = sorted([bx0, bx1] + [v for a in arches for v in (a["xc"] - a["r"], a["xc"] + a["r"])])
    for k in range(0, len(xs), 2):
        hulls.append(hk.box_pts(xs[k], xs[k + 1], by0, by1, 0.0, walk))
    for a in arches:
        hulls.append(hk.box_pts(a["xc"] - a["r"], a["xc"] + a["r"], by0, by1, zs + 0.5 * a["rz"], walk))
    top = mz1 + cz
    hulls += [hk.box_pts(bx0, bx1, by0, iy0, walk, top), hk.box_pts(bx0, bx1, iy1, by1, walk, top),
              hk.box_pts(bx0, ix0, iy0, iy1, walk, top), hk.box_pts(ix1, bx1, iy0, iy1, walk, top)]


def gate_watch(E, I, spec, hulls):
    """The timber watch room on the walk: four plank walls (jetty_wall, plank infill in the Door slot, posts, rails
    and top plates as plates in the Timber slot), corner posts, round-headed glazed windows, scroll brackets under
    the eaves, the hipped roof (spire_rings) with a ridge cap and two spike finials; the empty room inside."""
    wr = spec["watch"]
    walk = spec["gate"]["parapet"]["walk"]
    rx0, rx1, ry0, ry1 = wr["planks"]
    px0, px1, py0, py1 = wr["posts"]                        # the posts' outer faces: the walls' rectangle for the roof
    zt, t = wr["top"], wr["t"]
    frames = {"front": ((rx0, ry0), (rx1, ry0)), "right": ((rx1, ry0), (rx1, ry1)),
              "back": ((rx1, ry1), (rx0, ry1)), "left": ((rx0, ry1), (rx0, ry0))}
    ws = {"frame": "Timber", "rec": wr["rec"], "depth": 0.04, "fw": 0.04}
    for side, (p0, p1) in frames.items():
        m, L = gk.wall_frame(p0, p1)
        wl = dict(wr["walls"]["front" if side in ("front", "back") else "side"])
        wins = wl.pop("windows", [])
        wl["holes"] = [[w["xc"] - w["r"], w["xc"] + w["r"], w["z0"], w["zs"] + w["r"]] for w in wins]
        wl["infill"] = wr["infill"]
        E.push(m)
        I.push(m)
        jetty_wall(E, I, wl, L, walk, zt, t, ws, hulls)
        hulls.pop()                                         # one box for the whole room instead
        for w in wins:
            hk.arched_opening(E, I, w["xc"], w["z0"], w["zs"], w["r"], t, wr["rec"], wr["infill"], trim=None)
            arched_glass(E, w["xc"], w["z0"], w["zs"], w["r"], wr["rec"])
        bk = wr["brackets"]
        for x in wl.get("bracket_x", []):
            hk.console(E, x, -bk["wall"], bk["z_top"], "Timber", out=bk["out"], height=bk["height"], width=bk["width"])
        E.pop()
        I.pop()
    for x0, x1 in ((px0, rx0 + (rx0 - px0) * 1.25), (rx1 - (px1 - rx1) * 1.25, px1)):
        for y0, y1 in ((py0, ry0 + (ry0 - py0) * 1.25), (ry1 - (py1 - ry1) * 1.25, py1)):
            hk.block(E, x0, x1, y0, y1, walk, zt, "Timber", faces="xXyY")
    hulls.append(hk.box_pts(px0, px1, py0, py1, walk, zt))
    # the room inside: floor on the walk, a ceiling under the roof
    fx0, fx1, fy0, fy1 = rx0 + t, rx1 - t, ry0 + t, ry1 - t
    hk.rect(I, (fx0, fy0, walk + 0.005), (fx1, fy0, walk + 0.005), (fx1, fy1, walk + 0.005), (fx0, fy1, walk + 0.005), "Timber", (0, 0, 1))
    c = wr["ceiling"]
    hk.rect(I, (fx0, fy0, c), (fx1, fy0, c), (fx1, fy1, c), (fx0, fy1, c), "Plaster", (0, 0, -1))
    # the hipped roof, its ridge cap and the finials
    rf = wr["roof"]
    rings = [tuple(r) for r in rf["rings"]]
    cs = hk.spire_rings(E, px0, px1, py0, py1, rings, "Roof", zt, pitch=rf["pitch"], step=rf["step"])
    hulls.append([p for cc in (cs[0], cs[-1]) for p in cc] + [(p[0], p[1], zt) for p in cs[0]])
    z, iy, ix = rings[-1]
    rc = rf["ridge_cap"]
    yc = (py0 + py1) / 2
    hk.block(E, px0 + ix - 0.02, px1 - ix + 0.02, yc - rc[0], yc + rc[0], z - 0.02, z + rc[1], "Roof", faces="xXyYZ")
    fn = rf["finials"]
    for x in (px0 + ix, px1 - ix):
        hk.spike_finial(E, x, yc, z + rc[1] - 0.02, fn["top"], fn["mat"], r=fn["r"])


def build_townhouse_gate(spec):
    E, I = hk.Builder(hk.MATERIALS, seed=11), hk.Builder(hk.MATERIALS, seed=12)
    hulls = []
    gate_block(E, spec, hulls)
    gate_watch(E, I, spec, hulls)
    return E, I, hulls


BUILDERS = {"townhouse_flat": build_townhouse_flat, "townhouse_gable": build_townhouse_gable, "townhouse_terrace": build_townhouse_terrace,
            "townhouse_mansard": build_townhouse_mansard, "townhouse_zinc": build_townhouse_zinc, "townhouse_jetty": build_townhouse_jetty,
            "townhouse_loft": build_townhouse_loft, "townhouse_stack": build_townhouse_stack, "townhouse_tavern": build_townhouse_tavern,
            "townhouse_gate": build_townhouse_gate}


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


def audit_setup_mansard(spec):
    """Outside eyes only: street, both sides, the lower town; the gables and the attic under the roof from above."""
    W, D, t = spec["width"], spec["depth"], spec["wall"]
    top = spec["top"] - 0.05
    prof = mansard_profile(spec)
    th = spec["roof"]["thick"]
    street = [("lane left", (1.0, -4.5, 1.7)), ("lane door", (5.3, -4.5, 1.7)), ("lane right", (9.6, -4.5, 1.7)),
              ("lane high", (5.0, -6.0, 9.0)), ("lane low left", (-1.5, -2.5, 1.0))]
    above = [("above front", (W / 2, -4.0, 22.0)), ("above", (W / 2, D / 2, 26.0)), ("above back", (W / 2, D + 5.0, 22.0)),
             ("left high", (-5.0, -2.0, 18.0)), ("lane high", (5.0, -6.0, 9.0)), ("plaza high", (W + 6.0, -3.0, 14.0))]
    y_in = 1.6
    gz1 = roof_z(prof, y_in) - th - 0.1
    sides = [("plaza", (W + 6.0, D / 2, 1.7)), ("plaza front", (W + 4.0, -3.0, 6.0)), ("plaza high", (W + 6.0, -3.0, 14.0))]
    lefts = [("left side", (-6.0, D / 2, 4.0)), ("left high", (-5.0, -2.0, 18.0))]
    return [("front", (0.0, t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.03, street),
            ("right", (W - t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.04, sides),
            ("left", (t / 2, 0.0, 0.0), (0, 1, 0), (0.03, D - 0.03, 0.02, top), 0.04, lefts),
            ("back", (0.0, D - t / 2, 0.0), (1, 0, 0), (0.03, W - 0.03, 0.02, top), 0.04,
             [("lower town", (5.0, D + 9.0, 6.0)), ("lower town left", (-2.0, D + 7.0, 12.0))]),
            ("gable right", (W - t / 2, 0.0, 0.0), (0, 1, 0), (y_in, D - y_in, top + 0.1, gz1), 0.05, sides),
            ("gable left", (t / 2, 0.0, 0.0), (0, 1, 0), (y_in, D - y_in, top + 0.1, gz1), 0.05, lefts),
            ("roof front", (0.0, 1.0, 0.0), (1, 0, 0), (0.05, W - 0.05, top + 0.1, roof_z(prof, 1.0) - th - 0.1), 0.06, above),
            ("roof mid", (0.0, D / 2, 0.0), (1, 0, 0), (0.05, W - 0.05, top + 0.1, spec["roof"]["ridge"] - th - 0.1), 0.06, above),
            ("roof back", (0.0, D - 1.0, 0.0), (1, 0, 0), (0.05, W - 0.05, top + 0.1, roof_z(prof, 1.0) - th - 0.1), 0.06,
             above + [("lower town", (5.0, D + 9.0, 6.0))])]


def shots_mansard(spec):
    W, D = spec["width"], spec["depth"]
    H = spec["roof"]["ridge"]
    dr = spec["shop"]["door"]
    dx = (dr["x0"] + dr["x1"]) / 2
    return [("iso", (W + 11.0, -17.0, 13.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
            ("front_eye", (W / 2 - 1.0, -6.0, 1.7), (W / 2, 0.0, 7.0), 16, 1200, 1500),
            ("door_close", (dx, -3.6, 1.8), (dx, 0.0, 1.6), 24, 1200, 1200),
            ("roof", (W + 4.0, -5.0, H + 3.0), (W / 2, D / 2, H - 2.5), 28, 1400, 1100),
            ("side", (W + 16.0, D / 2, 8.0), (W, D / 2, 8.5), 30, 1000, 1500),
            ("back", (W / 2 - 4.0, D + 18.0, 7.0), (W / 2, D / 2, H / 2), 35, 1000, 1500)]


def shots_zinc(spec):
    W, D = spec["width"], spec["depth"]
    H = spec["railing"]["post_top"]
    dr = spec["facades"]["front"]["arch_door"]
    dx = (dr["x0"] + dr["x1"]) / 2
    return [("iso", (W + 10.0, -15.0, 11.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
            ("front_eye", (W / 2 - 1.0, -6.0, 1.7), (W / 2, 0.0, 6.5), 16, 1200, 1500),
            ("door_close", (dx, -3.6, 1.8), (dx, 0.0, 2.2), 24, 1200, 1200),
            ("roof", (W + 4.0, -5.0, H + 3.0), (W / 2, D / 2, H - 2.5), 28, 1400, 1100),
            ("side", (W + 16.0, D / 2, 7.0), (W, D / 2, 7.5), 30, 1000, 1500),
            ("back", (W / 2 - 4.0, D + 18.0, 6.0), (W / 2, D / 2, H / 2), 35, 1000, 1500)]


def audit_setup_jetty(spec):
    """Outside eyes only: street (into the porch and up the jetties), both sides (through the passages), the lower
    town, above the roof. Planes run just inside the shell."""
    W, D = spec["width"], spec["depth"]
    prof = [tuple(p) for p in spec["roof"]["profile"]]
    th = spec["roof"]["thick"]
    CEIL = spec["ceiling"]
    ys = spec["ground"]["shop"]["y"] + spec["ground"]["shop"]["t"]
    street = [("lane left", (1.0, -4.5, 1.7)), ("lane door", (3.3, -4.5, 1.7)), ("lane right", (6.2, -4.5, 1.7)),
              ("lane high", (3.6, -6.0, 9.0)), ("lane low left", (-1.5, -2.5, 1.0)), ("lane low right", (W + 1.5, -2.5, 1.0))]
    rights = [("plaza", (W + 6.0, D / 2, 1.7)), ("plaza front", (W + 4.0, -3.0, 1.7)), ("plaza high", (W + 6.0, -3.0, 14.0)),
              ("passage right", (W + 3.0, 1.38, 1.5))]
    lefts = [("left side", (-6.0, D / 2, 4.0)), ("left high", (-5.0, -2.0, 18.0)), ("passage left", (-3.0, 1.38, 1.5))]
    above = [("above front", (W / 2, -5.0, 24.0)), ("above", (W / 2, D / 2, 28.0)), ("above back", (W / 2, D + 5.0, 24.0))]
    lower = [("lower town", (4.0, D + 9.0, 6.0)), ("lower town left", (-2.0, D + 7.0, 12.0))]
    return [("ground", (0.0, ys + 0.3, 0.0), (1, 0, 0), (0.5, W - 0.5, 0.05, 6.3), 0.04, street + rights[1:2] + rights[3:] + lefts[2:]),
            ("upper front", (0.0, 0.25, 0.0), (1, 0, 0), (0.1, W - 0.1, 6.45, CEIL - 0.05), 0.04, street),
            ("right", (W - 0.3, 0.0, 0.0), (0, 1, 0), (ys + 0.1, D - 0.3, 0.05, CEIL - 0.05), 0.04, rights),
            ("left", (0.3, 0.0, 0.0), (0, 1, 0), (ys + 0.1, D - 0.3, 0.05, CEIL - 0.05), 0.04, lefts),
            ("back", (0.0, D - 0.3, 0.0), (1, 0, 0), (0.1, W - 0.1, 0.05, roof_z(prof, D) - th - 0.1), 0.04, lower),
            ("attic front", (0.0, 1.5, 0.0), (1, 0, 0), (0.1, W - 0.1, CEIL + 0.1, roof_z(prof, 1.5) - th - 0.1), 0.05, above + street[3:4]),
            ("attic back", (0.0, 5.0, 0.0), (1, 0, 0), (0.1, W - 0.1, CEIL + 0.1, roof_z(prof, 5.0) - th - 0.1), 0.05, above + lower)]


def shots_jetty(spec):
    W, D = spec["width"], spec["depth"]
    H = max(p[1] for p in spec["roof"]["profile"])
    dx = sum(spec["ground"]["shop"]["door"]["x"]) / 2
    return [("iso", (W + 11.0, -16.0, 12.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
            ("front_eye", (W / 2 - 1.0, -6.0, 1.7), (W / 2, 0.0, 7.5), 16, 1200, 1500),
            ("door_close", (dx, -4.0, 1.8), (dx, 1.8, 2.0), 24, 1200, 1200),
            ("roof", (W + 4.0, -6.0, H + 3.0), (W / 2, D / 2, H - 3.0), 28, 1400, 1100),
            ("side", (W + 16.0, D / 2, 8.0), (W, D / 2, 8.5), 30, 1000, 1500),
            ("back", (W / 2 - 4.0, D + 18.0, 7.0), (W / 2, D / 2, H / 2), 35, 1000, 1500)]


def audit_setup_loft(spec):
    """Outside eyes only: street (the door, the louvres, the loft doors), both sides, the lower town, above the roof.
    Planes run 0.5 m inside the outer faces, below the ceiling; the attic planes run front to back under the roof."""
    W, D, CEIL = spec["width"], spec["depth"], spec["ceiling"]
    th = spec["roof"]["thick"]
    prof = sorted(tuple(p) for p in spec["roof"]["profile"])
    xr = max(prof, key=lambda p: p[1])[0]
    street = [("lane left", (1.0, -4.5, 1.7)), ("lane door", (3.1, -4.5, 1.7)), ("lane right", (5.3, -4.5, 1.7)),
              ("lane high", (3.1, -6.0, 9.0)), ("lane low left", (-1.5, -2.5, 1.0)), ("lane low right", (W + 1.5, -2.5, 1.0)),
              ("lane very high", (3.1, -8.0, 16.0))]
    rights = [("plaza", (W + 6.0, D / 2, 1.7)), ("plaza front", (W + 4.0, -3.0, 6.0)), ("plaza high", (W + 6.0, -3.0, 14.0))]
    lefts = [("left side", (-6.0, D / 2, 4.0)), ("left high", (-5.0, -2.0, 16.0))]
    lower = [("lower town", (3.0, D + 9.0, 6.0)), ("lower town left", (-2.0, D + 7.0, 12.0)), ("lower town high", (3.0, D + 8.0, 16.0))]
    above = [("above front", (W / 2, -5.0, 22.0)), ("above", (W / 2, D / 2, 24.0)), ("above back", (W / 2, D + 5.0, 22.0)),
             ("above left", (-5.0, D / 2, 20.0)), ("above right", (W + 5.0, D / 2, 20.0))]
    zt = CEIL - 0.05
    return [("front", (0.0, 0.5, 0.0), (1, 0, 0), (0.5, W - 0.5, 0.05, zt), 0.04, street),
            ("right", (W - 0.5, 0.0, 0.0), (0, 1, 0), (0.5, D - 0.5, 0.05, zt), 0.04, rights),
            ("left", (0.5, 0.0, 0.0), (0, 1, 0), (0.5, D - 0.5, 0.05, zt), 0.04, lefts),
            ("back", (0.0, D - 0.5, 0.0), (1, 0, 0), (0.5, W - 0.5, 0.05, zt), 0.04, lower),
            ("attic ridge", (xr, 0.0, 0.0), (0, 1, 0), (0.5, D - 0.5, CEIL + 0.1, roof_z(prof, xr) - th - 0.1), 0.05, above + street[3:] + lower),
            ("attic left", (1.2, 0.0, 0.0), (0, 1, 0), (0.5, D - 0.5, CEIL + 0.1, roof_z(prof, 1.2) - th - 0.1), 0.05, above + lefts + street[3:]),
            ("attic right", (W - 1.2, 0.0, 0.0), (0, 1, 0), (0.5, D - 0.5, CEIL + 0.1, roof_z(prof, W - 1.2) - th - 0.1), 0.05,
             above + rights + lower)]


def shots_loft(spec):
    W, D = spec["width"], spec["depth"]
    H = max(p[1] for p in spec["roof"]["profile"])
    dx = sum(spec["ground"]["door"]["x"]) / 2
    return [("iso", (W + 11.0, -16.0, 12.0), (W / 2, D / 2, H / 2), 35, 1200, 1600),
            ("front_eye", (W / 2 - 1.0, -6.0, 1.7), (W / 2, 0.0, 7.0), 16, 1200, 1500),
            ("door_close", (dx, -3.6, 1.6), (dx, 0.0, 1.4), 24, 1200, 1200),
            ("gable", (W / 2 + 2.5, -6.0, 12.0), (W / 2, -0.5, 13.2), 22, 1200, 1200),
            ("roof", (W + 4.0, -6.0, H + 3.0), (W / 2, D / 2, H - 3.0), 28, 1400, 1100),
            ("side", (W + 16.0, D / 2, 8.0), (W, D / 2, 8.0), 30, 1000, 1500),
            ("back", (W / 2 - 4.0, D + 18.0, 7.0), (W / 2, D / 2, H / 2), 35, 1000, 1500)]


def audit_setup_tavern(spec):
    """Outside eyes only: street (shopfront, oriel, gable), both sides, the lower town, above; planes just inside the
    front block, the back block, the attic, the tower and the spire."""
    W, D, CEIL = spec["width"], spec["depth"], spec["ceiling"]
    X0, X1, Y0, Y1 = spec["tower"]["box"]
    th = spec["roof"]["thick"]
    prof = sorted(tuple(p) for p in spec["roof"]["profile"])
    xm, zm = max(prof, key=lambda p: p[1])
    tc = spec["tower"]["ceiling"]
    xc, yc = (X0 + X1) / 2, (Y0 + Y1) / 2
    street = [("lane left", (0.8, -4.5, 1.7)), ("lane door", (2.5, -4.5, 1.7)), ("lane right", (5.0, -4.5, 1.7)),
              ("lane high", (3.1, -6.0, 9.0)), ("lane low left", (-2.5, -2.5, 1.0)), ("lane low right", (W + 1.5, -2.5, 1.0)),
              ("lane very high", (3.1, -8.0, 16.0)), ("lane tower", (-1.0, -6.0, 12.0))]
    rights = [("side right", (W + 6.0, D / 2, 1.7)), ("side right front", (W + 4.0, -3.0, 6.0)), ("side right high", (W + 6.0, -3.0, 14.0))]
    lefts = [("side left", (-7.0, 1.5, 4.0)), ("side left high", (-6.0, -2.0, 16.0)), ("side left low", (-4.0, 0.5, 1.5))]
    lower = [("lower town", (3.0, D + 9.0, 6.0)), ("lower town left", (-3.0, D + 7.0, 12.0)), ("lower town high", (3.0, D + 8.0, 18.0))]
    above = [("above front", (3.0, -5.0, 22.0)), ("above", (3.0, 3.0, 25.0)), ("above back", (3.0, D + 5.0, 22.0)),
             ("above left", (-5.0, 1.5, 20.0)), ("above right", (W + 5.0, D / 2, 20.0))]
    tower_eyes = [("tower left", (-8.0, yc, 6.0)), ("tower left high", (-8.0, yc, 12.5)), ("tower front", (xc, -8.0, 12.5)),
                  ("tower back", (xc, D + 9.0, 12.5)), ("tower right", (W + 6.0, yc, 13.0)), ("tower door", (-4.0, yc, 4.6))]
    spire_eyes = [("spire front", (xc, -8.0, 16.0)), ("spire left", (-8.0, yc, 16.0)), ("spire right", (W + 6.0, yc, 16.5)),
                  ("spire back", (xc, D + 9.0, 16.0)), ("spire above", (xc + 0.5, yc - 0.5, 26.0))]
    return [("front", (0.0, 0.6, 0.0), (1, 0, 0), (0.5, W - 0.5, 0.2, CEIL - 0.1), 0.04, street),
            ("oriel", (0.0, -0.45, 0.0), (1, 0, 0), (2.0, 4.2, 4.75, 6.6), 0.04, street),
            ("right", (W - 0.5, 0.0, 0.0), (0, 1, 0), (0.5, D - 0.5, 0.2, CEIL - 0.1), 0.04, rights + lower[:1]),
            ("left", (0.5, 0.0, 0.0), (0, 1, 0), (0.5, Y0 - 0.2, 0.2, CEIL - 0.1), 0.04, lefts + street[4:5]),
            ("back", (0.0, D - 0.6, 0.0), (1, 0, 0), (X1 + 0.6, W - 0.5, 0.2, CEIL - 0.1), 0.04, lower),
            ("attic ridge", (xm, 0.0, 0.0), (0, 1, 0), (-0.2, D - 0.6, CEIL + 0.1, zm - th - 0.15), 0.05, above + street[3:] + lower),
            ("attic right", (W - 1.0, 0.0, 0.0), (0, 1, 0), (0.0, D - 0.6, CEIL + 0.1, roof_z(prof, W - 1.0) - th - 0.1), 0.05, above + rights + lower),
            ("attic left", (0.9, 0.0, 0.0), (0, 1, 0), (-0.1, Y0 - 0.2, CEIL + 0.1, roof_z(prof, 0.9) - th - 0.1), 0.05, above + lefts + street[3:]),
            ("tower", (xc, 0.0, 0.0), (0, 1, 0), (Y0 + 0.6, Y1 - 0.6, 0.3, tc - 0.1), 0.05, tower_eyes + lower + lefts),
            ("spire", (0.0, yc, 0.0), (1, 0, 0), (X0 + 0.9, X1 - 0.9, spec["tower"]["z1"] + 0.1, 17.4), 0.05, spire_eyes)]


def shots_tavern(spec):
    W, D = spec["width"], spec["depth"]
    X0, X1, Y0, Y1 = spec["tower"]["box"]
    H = spec["spire"]["finials"]["top"]
    dx = sum(spec["shop"]["walls"]["front"]["door"]) / 2
    return [("iso", (W + 12.0, -17.0, 13.0), (W / 2 - 1.0, D / 2, H / 2), 32, 1200, 1600),
            ("iso_left", (-15.0, -13.0, 12.0), (0.5, D / 2, H / 2), 32, 1200, 1600),
            ("front_eye", (W / 2 - 1.5, -7.0, 1.7), (W / 2 - 1.0, 0.0, 8.0), 14, 1200, 1500),
            ("door_close", (dx, -3.6, 1.6), (dx, 0.0, 1.8), 24, 1200, 1200),
            ("oriel", (3.1, -5.0, 6.0), (3.1, -0.5, 6.4), 22, 1200, 1200),
            ("spire", (-6.0, -7.0, H - 1.0), ((X0 + X1) / 2, (Y0 + Y1) / 2, 16.0), 24, 1200, 1200),
            ("stair", (-6.0, -2.0, 3.0), (-1.0, 3.5, 2.5), 26, 1200, 1200),
            ("back", (W / 2 - 4.0, D + 20.0, 8.0), (W / 2 - 1.0, D / 2, H / 2), 35, 1000, 1500)]


def audit_setup_gate(spec):
    """Outside eyes only: the lane (and through the arches), inside both passages, both sides (into the niches), the
    lower town, the walk, above. Planes inside the piers, in the masonry over the vaults, in the watch room and under
    its roof."""
    W, D = spec["width"], spec["depth"]
    g, wr = spec["gate"], spec["watch"]
    bx0, bx1, by0, by1 = g["body"]
    walk = g["parapet"]["walk"]
    zs = g["impost"]["z"][1]
    arches = g["arches"]
    nd = g["niche"]["depth"]
    px0, px1, py0, py1 = wr["posts"]
    rx0, rx1, ry0, ry1 = wr["planks"]
    ym = (by0 + by1) / 2
    zr = wr["roof"]["rings"][-1][0]
    street = [("lane left", (1.0, -4.5, 1.7)), ("lane mid", (W / 2, -4.5, 1.7)), ("lane right", (W - 1.0, -4.5, 1.7)),
              ("lane arch L", (arches[0]["xc"] + 0.8, -3.0, 1.2)), ("lane arch R", (arches[1]["xc"] - 0.8, -3.0, 1.2)),
              ("lane high", (W / 2, -8.0, 9.5)), ("lane very high", (W / 2, -10.0, 16.0)),
              ("lane low left", (-1.5, -2.5, 1.0)), ("lane low right", (W + 1.5, -2.5, 1.0))]
    passages = [(f"passage {k}", (a["xc"] + dx, ym + dy, 1.7)) for k, a in enumerate(arches) for dx, dy in ((-0.9, -1.5), (0.9, 1.5))]
    sides = [("side left", (-6.0, ym, 3.0)), ("side left high", (-6.0, ym, 9.5)), ("side left low", (-3.0, ym - 1.0, 1.0)),
             ("side right", (W + 6.0, ym, 3.0)), ("side right high", (W + 6.0, ym, 9.5)), ("side right low", (W + 3.0, ym + 1.0, 1.0))]
    lower = [("lower town", (W / 2, D + 9.0, 1.0)), ("lower town left", (2.0, D + 7.0, 2.0)), ("lower town high", (W - 2.0, D + 9.0, 8.0))]
    above = [("above", (W / 2, ym, 22.0)), ("above front", (W / 2, -5.0, 18.0)), ("above back", (W / 2, D + 5.0, 18.0)),
             ("above left", (-5.0, ym, 16.0)), ("above right", (W + 5.0, ym, 16.0))]
    walkers = [("walk front", (W / 2 - 1.2, (by0 + py0) / 2 + 0.2, walk + 1.7)), ("walk left", ((bx0 + px0) / 2 + 0.2, ym, walk + 1.7)),
               ("walk back", (W / 2 + 1.2, (by1 + py1) / 2 - 0.2, walk + 1.7))]
    outside = street + passages + sides + lower + above + walkers
    out = []
    piers = [(bx0, arches[0]["xc"] - arches[0]["r"]), (arches[0]["xc"] + arches[0]["r"], arches[1]["xc"] - arches[1]["r"]),
             (arches[1]["xc"] + arches[1]["r"], bx1)]
    for y in (by0 + 0.3, ym, by1 - 0.3):
        for k, (a, b) in enumerate(piers):
            a2 = a + (nd + 0.08 if k == 0 else 0.05)
            b2 = b - (nd + 0.08 if k == 2 else 0.05)
            out.append((f"pier {k} y{y:.2f}", (0.0, y, 0.0), (1, 0, 0), (a2, b2, 0.05, walk - 0.05), 0.05, outside))
        for k, a in enumerate(arches):
            out.append((f"over vault {k} y{y:.2f}", (0.0, y, 0.0), (1, 0, 0), (a["xc"] - a["r"] + 0.02, a["xc"] + a["r"] - 0.02,
                        zs + a["rz"] + 0.05, walk - 0.05), 0.05, outside))
    for x in (bx0 + nd + 0.1, bx1 - nd - 0.1):
        out.append((f"pier side x{x:.2f}", (x, 0.0, 0.0), (0, 1, 0), (by0 + 0.05, by1 - 0.05, 0.05, walk - 0.05), 0.05, outside))
    out.append(("watch room", (0.0, (py0 + py1) / 2, 0.0), (1, 0, 0), (rx0 + 0.25, rx1 - 0.25, walk + 0.05, wr["ceiling"] - 0.05), 0.04, outside))
    out.append(("watch room x", ((px0 + px1) / 2, 0.0, 0.0), (0, 1, 0), (ry0 + 0.25, ry1 - 0.25, walk + 0.05, wr["ceiling"] - 0.05), 0.04, outside))
    out.append(("roof", (0.0, (py0 + py1) / 2, 0.0), (1, 0, 0), (px0 + 1.0, px1 - 1.0, wr["top"] + 0.05, zr - 0.15), 0.05, outside))
    return out


def shots_gate(spec):
    W, D = spec["width"], spec["depth"]
    H = spec["watch"]["roof"]["finials"]["top"]
    a0 = spec["gate"]["arches"][0]
    return [("iso", (W + 13.0, -18.0, 14.0), (W / 2, D / 2, H / 2), 32, 1400, 1400),
            ("iso_left", (-12.0, -16.0, 12.0), (W / 2, D / 2, H / 2), 32, 1400, 1400),
            ("front_eye", (W / 2 - 1.5, -9.0, 1.7), (W / 2, 0.0, 6.0), 18, 1500, 1200),
            ("passage", (a0["xc"] - 0.5, -4.0, 1.6), (a0["xc"], D, 2.0), 22, 1200, 1200),
            ("walk", (W / 2 + 4.0, -5.0, 11.0), (W / 2, D / 2, 8.0), 24, 1400, 1100),
            ("side", (W + 16.0, D / 2, 6.0), (W, D / 2, 6.5), 30, 1000, 1500),
            ("back", (W / 2 - 4.0, D + 22.0, 2.0), (W / 2, D / 2, H / 2 - 1.0), 32, 1400, 1400)]


def audit_setup(spec):
    if spec["type"] == "townhouse_gate":
        return audit_setup_gate(spec)
    if spec["type"] == "townhouse_tavern":
        return audit_setup_tavern(spec)
    if spec["type"] in ("townhouse_loft", "townhouse_stack"):     # house #8 uses the loft's eyes and planes
        return audit_setup_loft(spec)
    if spec["type"] == "townhouse_jetty":
        return audit_setup_jetty(spec)
    if spec["type"] == "townhouse_gable":
        return audit_setup_gable(spec)
    if spec["type"] == "townhouse_terrace":
        return audit_setup_terrace(spec)
    if spec["type"] in ("townhouse_mansard", "townhouse_zinc"):      # house #5 uses the mansard's eyes and planes
        return audit_setup_mansard(spec)
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


def shots_stack(spec):
    W, D = spec["width"], spec["depth"]
    H = spec["stack"]["cap"][-1][0]
    dx = sum(spec["door"]["x"]) / 2
    return [("iso", (W + 11.0, -17.0, 13.0), (W / 2 - 1.0, D / 2, H / 2), 32, 1200, 1600),
            ("iso_left", (-14.0, -12.0, 11.0), (-0.5, D / 2, H / 2), 32, 1200, 1600),
            ("front_eye", (W / 2 - 1.5, -7.0, 1.7), (W / 2 - 1.0, 0.0, 8.0), 14, 1200, 1500),
            ("door_close", (dx, -3.6, 1.6), (dx, 0.0, 1.8), 24, 1200, 1200),
            ("stack_foot", (-5.0, -3.0, 2.0), (-1.0, D / 2 - 1.0, 4.5), 26, 1200, 1200),
            ("stack_top", (-7.0, -6.0, H + 2.0), (-1.0, D / 2, H - 1.5), 24, 1200, 1200),
            ("back", (W / 2 - 4.0, D + 18.0, 7.0), (W / 2 - 1.0, D / 2, H / 2), 35, 1000, 1500)]


def shots(spec):
    if spec["type"] == "townhouse_gate":
        return shots_gate(spec)
    if spec["type"] == "townhouse_tavern":
        return shots_tavern(spec)
    if spec["type"] == "townhouse_stack":
        return shots_stack(spec)
    if spec["type"] == "townhouse_gable":
        return shots_gable(spec)
    if spec["type"] == "townhouse_terrace":
        return shots_terrace(spec)
    if spec["type"] == "townhouse_mansard":
        return shots_mansard(spec)
    if spec["type"] == "townhouse_zinc":
        return shots_zinc(spec)
    if spec["type"] == "townhouse_jetty":
        return shots_jetty(spec)
    if spec["type"] == "townhouse_loft":
        return shots_loft(spec)
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
