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


BUILDERS = {"townhouse_flat": build_townhouse_flat}


# ---------------------------------------------------------------------------------------------
# audit eyes and planes, renders
# ---------------------------------------------------------------------------------------------
def audit_setup(spec):
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
    ucx = [hull_object(h, f"UCX_SM_House_{FOLDER}_Ext_{i:02d}") for i, h in enumerate(hulls)]
    for ob in ucx:
        ob.display_type = 'WIRE'
        ob.hide_render = True
    for ob in (ext, inn):
        co = np.empty(len(ob.data.vertices) * 3, dtype=np.float32)
        ob.data.vertices.foreach_get("co", co)
        co = co.reshape(-1, 3)
        print(f"BUILT {ob.name}: {len(ob.data.polygons)} faces, bounds {co.min(0).round(3)} .. {co.max(0).round(3)}")
    print(f"BUILT {len(ucx)} collision hulls")
    os.makedirs(OUT, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, f"{FOLDER}.blend"))
    export([ext] + ucx, os.path.join(OUT, f"SM_House_{FOLDER}_Ext.fbx"))
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
            total += gk.audit_plane([ext, inn] + previews, origin, udir, u0 + 0.0013, u1, z0 + 0.0017, z1, eyes, step=step)
        print(f"AUDIT total see-through rays: {total}")
    if args.renders:
        render([ext, inn] + previews, args.renders)


main()
