"""Playability check of one placed house (routine step 11), in the open editor.

Run through SourceArt/Houses/tools/ue_house.py with HOUSE_STEP set:
  HOUSE_STEP=traces   sweeps the player's capsule along the walking route: street -> front door -> ground-floor room ->
                      floor landing -> every flight and half landing -> every floor's room. The capsule is shrunk by the
                      character's step height at its foot and lifted by it (as CharacterMovement steps up), so a hit
                      means a real obstacle: too little width or headroom, or a missing opening.
  HOUSE_STEP=build    starts a NavMesh rebuild (it runs over editor ticks; poll with HOUSE_STEP=nav)
  HOUSE_STEP=nav      reports whether the rebuild is done, projects a point of every floor's room, landing and half
                      landing onto the NavMesh, and asks for a path from the street to the top floor's room.
The route follows the spec: the front door of "facades.front" and a "dogleg_back" stair. Reads the house transform
from "place" (ledger section 3: world = location + yaw-rotated (100 x, -100 y, 100 z)).
"""
import json
import math
import os

import unreal

FOLDER = HOUSE_NAME  # noqa: F821 (set by ue_house.py)
STEP = globals().get("HOUSE_STEP", "traces")
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
spec = json.load(open(os.path.join(PROJECT, "SourceArt", "Houses", "specs", FOLDER + ".json"), encoding="utf-8"))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
L, yaw = spec["place"]["location"], math.radians(spec["place"]["yaw"])
TT = unreal.TraceTypeQuery.TRACE_TYPE_QUERY1          # Visibility: static meshes block it like they block the pawn
STEP_UP = 45.0                                         # CharacterMovement MaxStepHeight (cm)


def log(msg):
    unreal.log(f"[HousePlay] {msg}")


def w(x, y, z):
    u, v = 100.0 * x, -100.0 * y
    return unreal.Vector(L[0] + u * math.cos(yaw) - v * math.sin(yaw), L[1] + u * math.sin(yaw) + v * math.cos(yaw), L[2] + 100.0 * z)


def capsule():
    r, hh = 42.0, 96.0
    cls = unreal.EditorAssetLibrary.load_blueprint_class("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter")
    if cls:
        try:
            cap = unreal.get_default_object(cls).get_editor_property("capsule_component")
            r, hh = cap.get_unscaled_capsule_radius(), cap.get_unscaled_capsule_half_height()
        except Exception as exc:
            log(f"capsule of BP_ThirdPersonCharacter not readable ({exc}); using {r}/{hh}")
    return r, hh


W, D, t = spec["width"], spec["depth"], spec["wall"]
F = spec["floors"]
st = spec["stair"]
fw = st.get("flight_width", 1.2)
y_back = D - t
yA = y_back - fw / 2                                   # middle of flight A (back row)
yB = y_back - 1.5 * fw - st.get("spine", 0.12)         # middle of flight B (front row)
xh, xl = st["x_half"], st["x_land"]
xm_half, xm_land = (t + xh) / 2, (xl + W - t) / 2
door = [o for o in spec["facades"]["front"]["openings"] if o["kind"] == "door"][0]
xd = (door["x0"] + door["x1"]) / 2

# the route: (name, x, y, ground z) in house space; ground is linear between consecutive points
route = [("street", xd, -1.5, 0.0), ("doorway", xd, t / 2, F[0]), ("room 0", xd, 2.0, F[0]), ("to landing 0", xm_land, 2.0, F[0])]
for k in range(len(F) - 1):
    zh = (F[k] + F[k + 1]) / 2
    route += [(f"landing {k} back", xm_land, yA, F[k]), (f"flight A{k} foot", xl, yA, F[k]), (f"flight A{k} head", xh, yA, zh),
              (f"half landing {k}", xm_half, yA, zh), (f"half landing {k} front", xm_half, yB, zh), (f"flight B{k} foot", xh, yB, zh),
              (f"flight B{k} head", xl, yB, F[k + 1]), (f"landing {k + 1} front", xm_land, yB, F[k + 1]),
              (f"room {k + 1} side", xm_land, 2.0, F[k + 1]), (f"room {k + 1}", 2.0, 2.0, F[k + 1]), (f"back to landing {k + 1}", xm_land, 2.0, F[k + 1])]
route = route[:-1]                                     # end in the top floor's room

if STEP == "traces":
    r, hh = capsule()
    shh = hh - STEP_UP / 2
    log(f"player capsule radius {r:.0f} cm, half height {hh:.0f} cm; swept as radius {r:.0f}, half height {shh:.1f}, foot {STEP_UP:.0f} cm up")
    bad = 0
    for (n0, x0, y0, z0), (n1, x1, y1, z1) in zip(route, route[1:]):
        a, b = w(x0, y0, z0), w(x1, y1, z1)
        lift = STEP_UP + shh + 2.0
        a.z += lift
        b.z += lift
        hit = unreal.SystemLibrary.capsule_trace_single(world, a, b, r, shh, TT, False, [], unreal.DrawDebugTrace.NONE, True)
        tup = hit.to_tuple() if hit else None
        if tup and tup[0]:
            bad += 1
            comp = tup[10].get_name() if len(tup) > 10 and tup[10] else "?"
            actor = tup[9].get_actor_label() if tup[9] else "?"
            log(f"BLOCKED {n0} -> {n1}: hit {actor}.{comp} at {tup[4]}, start inside: {tup[1]}")
        else:
            log(f"ok      {n0} -> {n1}")
    log(f"TRACES: {len(route) - 1} segments, {bad} blocked")
elif STEP == "negative":                             # self-test: sweeps that MUST be blocked by the house's collision
    r, hh = capsule()
    shh = hh - STEP_UP / 2
    lift = STEP_UP + shh + 2.0
    yw = y_back - 2 * fw - st.get("spine", 0.12) - 0.5           # in the room, half a metre in front of the stair well
    tests = [("through the front wall beside the door", (1.0, 2.0, F[0]), (1.0, -1.5, F[0])),
             ("through the stair-well guard on floor 1", (2.0, yw, F[1]), (2.0, yB, F[1])),
             ("through the back wall from landing 2", (xm_land, yA, F[2]), (xm_land, D + 1.5, F[2])),
             ("up through the ceiling of room 0", (2.0, 2.0, F[0]), (2.0, 2.0, F[1] + 0.5))]
    bad = 0
    for name, p0, p1 in tests:
        a, b = w(*p0), w(*p1)
        a.z += lift
        b.z += lift
        hit = unreal.SystemLibrary.capsule_trace_single(world, a, b, r, shh, TT, False, [], unreal.DrawDebugTrace.NONE, True)
        tup = hit.to_tuple() if hit else None
        blocked = bool(tup and tup[0])
        bad += 0 if blocked else 1
        log(f"{'blocked (good)' if blocked else 'NOT BLOCKED'} {name}" + (f": {tup[9].get_actor_label()} at {tup[4]}" if blocked and tup[9] else ""))
    log(f"NEGATIVE: {len(tests)} tests, {bad} not blocked")
elif STEP == "build":
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    log("NavMesh rebuild started")
else:
    nav = unreal.NavigationSystemV1.get_navigation_system(world)
    building = nav.is_navigation_being_built(world) if nav else None
    log(f"navigation system {nav is not None}, being built: {building}")
    ext = unreal.Vector(30.0, 30.0, 60.0)
    missing = 0
    for name, x, y, z in route:
        if name.startswith(("street", "doorway", "to ", "back ", "flight")):
            continue
        p = unreal.NavigationSystemV1.project_point_to_navigation(world, w(x, y, z + 0.1), None, None, ext)
        ok = p is not None and abs(p.z - w(x, y, z).z) < 40.0
        missing += 0 if ok else 1
        log(f"{'nav ' if ok else 'NONE'} {name}: {p}")
    path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, w(xd, -1.5, 0.0), w(2.0, 2.0, F[-1]))
    if path:
        pts = list(path.get_editor_property("path_points"))
        end = pts[-1] if pts else None
        log(f"PATH street -> top floor room: {len(pts)} points, length {path.get_path_length():.0f} cm, partial {path.is_partial()}, "
            f"ends at {end}, highest point z {max(p.z for p in pts) if pts else None}")
    else:
        log("PATH street -> top floor room: none")
    log(f"NAV: {missing} route points without NavMesh")
log("DONE")
