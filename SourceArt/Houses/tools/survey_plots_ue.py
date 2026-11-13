"""Read-only: for each candidate plot near the cathedral, finds which sides face open street and the street height.

For every candidate house it traces straight down at 1.5 m outside the middle of each of its four sides (+X, -X,
+Y, -Y), ignoring the house itself. A side is "open" when the first hit is ground-like: it is not another house and
it lies below the house's eaves. Writes Saved/HouseCheck/district_plots.json.
"""
import unreal, json, re

PLOT_PATTERNS = [r"^Cathedral_RowHouse_", r"^Cathedral_LatticeHouse_", r"^Infill_InfillHouse_000(19|20|33|34)$",
                 r"^Girona_StreetHouse_", r"^Plaza_FlankHouse_",
                 # next ring, chosen by the user on 2026-11-13 ("ring + next streets")
                 r"^Church_SquareHouse_", r"^OldTown_OldHouse_002(2[1-9]|3[0-9]|40)$",
                 r"^SouthQuarter_CourtHouse_0033[0-4]$", r"^SouthTown_RowHouse_0060[0-2]$"]
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = eas.get_all_level_actors()
cands = [a for a in actors if isinstance(a, unreal.StaticMeshActor)
         and any(re.match(p, a.get_actor_label()) for p in PLOT_PATTERNS)]


def trace_down(x, y, ztop, ignore):
    start = unreal.Vector(x, y, ztop + 2000)
    end = unreal.Vector(x, y, ztop - 6000)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True,
                                                 ignore, unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None, None
    t = hit.to_tuple()
    blocking = t[0]
    if not blocking:
        return None, None
    loc = t[4]  # impact point
    act = t[9] if len(t) > 9 else None
    lab = act.get_actor_label() if act else ""
    return loc.z, lab


rows = []
for a in cands:
    o, e = a.get_actor_bounds(False)
    zmin, zmax = o.z - e.z, o.z + e.z
    sides = {}
    for name, (dx, dy) in {"+X": (1, 0), "-X": (-1, 0), "+Y": (0, 1), "-Y": (0, -1)}.items():
        x = o.x + dx * (e.x + 150)
        y = o.y + dy * (e.y + 150)
        z, lab = trace_down(x, y, zmax, [a])
        is_house = bool(lab) and ("House" in lab or "Cathedral_" in lab and "Lattice" in lab)
        sides[name] = {"z": round(z) if z is not None else None, "hit": lab,
                       "open": z is not None and not is_house and z < zmax - 600}
    open_sides = [k for k, v in sides.items() if v["open"]]
    street_z = min([v["z"] for v in sides.values() if v["open"]], default=None)
    rows.append({"label": a.get_actor_label(), "x": [round(o.x - e.x), round(o.x + e.x)],
                 "y": [round(o.y - e.y), round(o.y + e.y)], "zmin": round(zmin), "zmax": round(zmax),
                 "w_x_m": round(2 * e.x / 100, 2), "d_y_m": round(2 * e.y / 100, 2),
                 "open": open_sides, "street_z": street_z, "sides": sides})
rows.sort(key=lambda r: (r["label"].split("_")[0] + r["label"].split("_")[1], r["y"][0], r["x"][0]))
out = unreal.Paths.project_saved_dir() + "HouseCheck/district_plots.json"
json.dump(rows, open(out, "w"), indent=1)
for r in rows:
    sz = {k: (v["z"], v["hit"][:26]) for k, v in r["sides"].items()}
    print(f'{r["label"]:30s} x{r["x"]} y{r["y"]} {r["w_x_m"]}x{r["d_y_m"]}m z{r["zmin"]}..{r["zmax"]} '
          f'open={r["open"]} street_z={r["street_z"]}')
    print("      ", sz)
print("WROTE", out, len(rows))
