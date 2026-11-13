"""Read-only: lists the house actors nearest the Girona cathedral, with footprint and clearance.

Writes Saved/HouseCheck/district_survey.json and prints a table. A house is any StaticMeshActor whose label contains
"House". Clearance is the 2D gap between the house's world bounding box and the cathedral BP's (both axis-aligned,
so it is a lower bound for rotated houses). Footprint is the static mesh's local bounds times the actor scale, so it
is the true size regardless of yaw.
"""
import unreal, json, os, math

SURVEY_MAX_GAP_CM = 6000  # houses within 60 m of the cathedral box
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()

cath = [a for a in actors if a.get_actor_label().startswith("BP_GironaCathedral_V3")
        or a.get_class().get_name().startswith("BP_GironaCathedral_V3")]
print("cathedral actors:", [(a.get_actor_label(), a.get_class().get_name()) for a in cath])
c_org, c_ext = cath[0].get_actor_bounds(False)
cmin = (c_org.x - c_ext.x, c_org.y - c_ext.y)
cmax = (c_org.x + c_ext.x, c_org.y + c_ext.y)
print(f"cathedral box x {cmin[0]:.0f}..{cmax[0]:.0f}  y {cmin[1]:.0f}..{cmax[1]:.0f}  z top {c_org.z + c_ext.z:.0f}")


def gap2d(o, e):
    dx = max(cmin[0] - (o.x + e.x), (o.x - e.x) - cmax[0], 0)
    dy = max(cmin[1] - (o.y + e.y), (o.y - e.y) - cmax[1], 0)
    return math.hypot(dx, dy)


rows = []
for a in actors:
    lab = a.get_actor_label()
    if "House" not in lab or not isinstance(a, unreal.StaticMeshActor):
        continue
    comp = a.static_mesh_component
    mesh = comp.static_mesh
    o, e = a.get_actor_bounds(False)
    g = gap2d(o, e)
    if g > SURVEY_MAX_GAP_CM:
        continue
    lb = mesh.get_bounding_box() if mesh else None
    sc = a.get_actor_scale3d()
    size = None
    if lb:
        size = [round((lb.max.x - lb.min.x) * sc.x / 100, 2), round((lb.max.y - lb.min.y) * sc.y / 100, 2),
                round((lb.max.z - lb.min.z) * sc.z / 100, 2)]
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    rows.append({"label": lab, "mesh": mesh.get_path_name() if mesh else None,
                 "loc": [round(loc.x), round(loc.y), round(loc.z)], "yaw": round(rot.yaw, 1),
                 "size_m": size, "gap_m": round(g / 100, 2),
                 "hidden": a.is_hidden_ed() or a.is_temporarily_hidden_in_editor(),
                 "ground_z": round(o.z - e.z)})
rows.sort(key=lambda r: r["gap_m"])
os.makedirs(unreal.Paths.project_saved_dir() + "HouseCheck", exist_ok=True)
out = unreal.Paths.project_saved_dir() + "HouseCheck/district_survey.json"
json.dump(rows, open(out, "w"), indent=1)
for r in rows:
    print(f'{r["gap_m"]:6.1f} m  {r["label"]:34s} size={r["size_m"]} yaw={r["yaw"]:6.1f} loc={r["loc"]} '
          f'hid={int(r["hidden"])} {r["mesh"].split("/")[-1] if r["mesh"] else ""}')
print("WROTE", out, len(rows))
