"""Shared tail of the cathedral build scripts: command line, cavity bake, see-through check, export, previews.

A build script defines its parts and calls run():
  zone       "Tower" -> assets SM_Cathedral_Tower_<part>, files Girona_Tower_<part>.blend
  parts      {name: function(Builder) -> info dict}
  audits     {part: [(origin, udir, (u0, u1, z0, z1), step, [(eye name, (x, y, z)), ...]), ...]}
             the see-through check of gk.audit_plane on upright planes: grid points are
             origin + udir * u + (0, 0, z); every eye is a place where a player can stand
  shots      {part: [(tag, camera position, look-at point, lens, width, height), ...]}

Run (Blender 5.2, no UI):
  blender -b --factory-startup --python <script> -- --out <folder> --part <name> [--audit]
      [--renders <folder>] [--context <.blend of a neighbouring part> ...] [--template <.blend of the Meshy model>]
The other parts of the same script and the --context files only take part in the check and the previews.
"""
import argparse
import os
import sys

import bpy
import numpy as np

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gothic_kit as gk  # noqa: E402

MATERIALS = ["Ashlar", "Limestone", "Marble", "Paving", "Bronze", "BronzeRust", "Timber", "Glass", "Iron", "PaintGrey", "Tile"]
COLORS = {"Ashlar": (0.72, 0.69, 0.62), "Limestone": (0.78, 0.76, 0.70), "Marble": (0.82, 0.82, 0.80),
          "Paving": (0.42, 0.42, 0.41), "Bronze": (0.23, 0.36, 0.30), "BronzeRust": (0.36, 0.22, 0.14),
          "Timber": (0.30, 0.20, 0.12), "Glass": (0.05, 0.07, 0.12), "Iron": (0.08, 0.08, 0.09),
          "PaintGrey": (0.42, 0.46, 0.50), "Tile": (0.62, 0.40, 0.28)}


def _load(path):
    with bpy.data.libraries.load(path, link=False) as (src, dst):
        dst.objects = list(src.objects)
    out = []
    for o in dst.objects:
        if o is not None and o.type == 'MESH':
            bpy.context.scene.collection.objects.link(o)
            out.append(o)
    return out


def run(zone, parts, audits=None, shots=None, materials=MATERIALS, colors=COLORS):
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--part", required=True, choices=sorted(parts))
    ap.add_argument("--renders")
    ap.add_argument("--template")
    ap.add_argument("--context", nargs="*", default=[])
    ap.add_argument("--side", default="R", help="for scripts that build either long side: R or L")
    ap.add_argument("--audit", action="store_true", help="look for places where one can see through the stonework")
    args = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    B = gk.Builder(materials)
    info = parts[args.part](B)
    name = f"SM_Cathedral_{zone}_{args.part}"
    obj = B.to_object(name, colors)
    print(f"CAVITY mean {gk.bake_cavity(obj):.3f}")
    print(f"WEATHER streaks (mean, share of upright stone) {gk.bake_weather(obj)}")
    co = np.empty(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    print(f"BUILT {name}: {B.triangles()} triangles, {len(B.chunks)} chunks, bounds {co.min(0).round(3)} .. {co.max(0).round(3)}, info {info}")

    os.makedirs(args.out, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(args.out, f"Girona_{zone}_{args.part}.blend"))
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(args.out, name + ".fbx"), use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y',
        mesh_smooth_type='FACE', colors_type='SRGB', use_mesh_modifiers=False, add_leaf_bones=False, bake_anim=False)
    print("EXPORTED")

    others = []
    if args.audit or args.renders:
        for other, build in parts.items():
            if other != args.part:
                C = gk.Builder(materials)
                build(C)
                others.append(C.to_object("context_" + other, colors))
        for path in args.context:
            others += _load(path)
    if args.audit:
        total = 0
        for origin, udir, (u0, u1, z0, z1), step, eyes in (audits or {}).get(args.part, []):
            total += gk.audit_plane([obj] + others, origin, udir, u0, u1, z0, z1, eyes, step=step)
        print(f"AUDIT total see-through rays: {total}")

    if args.renders:                    # clay shading with cavity; back faces culled, so a flipped face shows as a hole
        from mathutils import Vector
        os.makedirs(args.renders, exist_ok=True)
        scene = bpy.context.scene
        if args.template:
            _load(args.template)
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
        cd.clip_start, cd.clip_end = 0.05, 600
        for tag, pos, at, lens, rx, ry in (shots or {}).get(args.part, []):
            cam.location = Vector(pos)
            cam.rotation_euler = (Vector(at) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()
            cd.lens = lens
            scene.render.resolution_x, scene.render.resolution_y = rx, ry
            scene.render.filepath = os.path.join(args.renders, f"{zone.lower()}_{args.part}_{tag}.png")
            bpy.ops.render.render(write_still=True)
        print("RENDERED")
