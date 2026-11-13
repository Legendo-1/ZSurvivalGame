"""Shared geometry kit for the Girona cathedral rebuild (Blender 5.2, Python).

Everything is built in "wall coordinates": x runs along the wall to the viewer's right, y points
INTO the wall (so a part standing 0.3 m proud of the wall face is at y = -0.3) and z is up. The
frame is right-handed, so a part is put on another wall of the building with a plain rotation
(Builder.push), never a mirror.

A Builder collects chunks of geometry as numpy arrays and writes one mesh at the end. Chunks never
share vertices, so every chunk boundary is a hard edge and a chunk marked smooth shades smoothly
inside. Each face gets box-projected UVs at TILE metres per texture repeat unless UVs are given,
and each vertex a "variation" value that ends up in the red vertex-colour channel.

Profiles for sweep() and lathe() are lists of runs; a run is a list of (u, v) points. List the
points so the outside of the moulding is on the right-hand side when walking along the profile
with u to the right and v up: from the wall at the bottom, around the front, to the wall at the top.
"""
import math
import random

import bpy
import numpy as np

TILE = 2.0          # metres of stone per texture repeat
QUARTER = [math.pi / 2 * i / 5 for i in range(1, 6)]      # angles for quarter-round profile pieces


def _unit(v):
    v = np.asarray(v, dtype=np.float64)
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v


def rot_z(deg):
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0, 0], [s, c, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]], dtype=np.float64)


def translate(x, y, z):
    m = np.eye(4)
    m[:3, 3] = (x, y, z)
    return m


# local z -> out of the wall (-y), local y -> up: lays a part built in the x-y plane onto the wall face
FACE_OUT = np.array([[1, 0, 0, 0], [0, 0, -1, 0], [0, 1, 0, 0], [0, 0, 0, 1.0]])


class Builder:
    def __init__(self, materials, seed=7):
        self.materials = list(materials)
        self.chunks = []
        self.stack = [np.eye(4)]
        self.rng = random.Random(seed)

    # -- transforms -------------------------------------------------------------------------
    def push(self, m):
        self.stack.append(self.stack[-1] @ np.asarray(m, dtype=np.float64))

    def pop(self):
        self.stack.pop()

    # -- adding geometry --------------------------------------------------------------------
    def add(self, verts, faces, mat, uv=None, smooth=False, var=0.5, uv_off=None):
        """verts (N,3); faces (M,3) or (M,4) indices into verts; uv (M,k,2) per corner or None for
        a box projection; var: one number or (N,) values in 0..1."""
        verts = np.asarray(verts, dtype=np.float64).reshape(-1, 3)
        faces = np.asarray(faces, dtype=np.int64)
        if len(faces) == 0:
            return
        m = self.stack[-1]
        v = verts @ m[:3, :3].T + m[:3, 3]
        if np.linalg.det(m[:3, :3]) < 0:
            faces = faces[:, ::-1]
            if uv is not None:
                uv = np.asarray(uv)[:, ::-1]
        if uv is None:
            uv = self._box_uv(v, faces, uv_off)
        var = np.full(len(v), var, dtype=np.float32) if np.isscalar(var) else np.asarray(var, dtype=np.float32)
        if isinstance(mat, str):
            mat = self.materials.index(mat)
        self.chunks.append((v.astype(np.float32), faces, int(mat), np.asarray(uv, dtype=np.float32), bool(smooth), var))

    def _box_uv(self, v, faces, off):
        p = v[faces]                                    # (M,k,3)
        n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        ax = np.abs(n).argmax(axis=1)
        if off is None:
            off = (self.rng.random() * TILE, self.rng.random() * TILE)
        uv = np.empty(p.shape[:2] + (2,), dtype=np.float64)
        for a, (i, j) in enumerate(((1, 2), (0, 2), (0, 1))):
            sel = ax == a
            uv[sel, :, 0] = p[sel][:, :, i]
            uv[sel, :, 1] = p[sel][:, :, j]
        return (uv + np.asarray(off)) / TILE

    def grid(self, P, mat, want=None, smooth=False, var=0.5, su=None, sv=None, uv_off=None, reverse=None):
        """Quad surface from a point grid P (n,m,3). `want` is the direction the surface should face:
        a vector, or ('from', point) / ('to', point) to face away from / towards a point. The winding
        is fixed to match (or pass reverse=True/False to set it directly). su / sv are distances along
        the two grid directions, for the UVs."""
        P = np.asarray(P, dtype=np.float64)
        n, m = P.shape[:2]
        idx = np.arange(n * m).reshape(n, m)
        q = np.stack([idx[:-1, :-1], idx[1:, :-1], idx[1:, 1:], idx[:-1, 1:]], axis=-1).reshape(-1, 4)
        if reverse is not None:
            if reverse:
                q = q[:, ::-1]
        elif want is not None:
            nrm = np.cross(P[1:, :-1] - P[:-1, :-1], P[:-1, 1:] - P[:-1, :-1]).reshape(-1, 3)
            if isinstance(want, tuple) and want[0] in ('from', 'to'):
                ref = P[:-1, :-1].reshape(-1, 3) - np.asarray(want[1], dtype=np.float64)
                if want[0] == 'to':
                    ref = -ref
            else:
                ref = np.broadcast_to(np.asarray(want, dtype=np.float64), nrm.shape)
            if (nrm * ref).sum() < 0:
                q = q[:, ::-1]
        uv = None
        if su is not None:
            uu, vv = np.meshgrid(np.asarray(su, dtype=np.float64), np.asarray(sv, dtype=np.float64), indexing='ij')
            if uv_off is None:
                uv_off = (self.rng.random(), self.rng.random())
            uv = (np.stack([uu, vv], axis=-1).reshape(-1, 2) / TILE)[q] + np.asarray(uv_off)
        self.add(P.reshape(-1, 3), q, mat, uv=uv, smooth=smooth, var=var, uv_off=uv_off)

    def fan(self, pts, mat, want, var=0.5, hub=None):
        """Flat polygon as a triangle fan around its centroid (or around `hub`), facing `want`."""
        pts = np.asarray(pts, dtype=np.float64)
        c = pts.mean(axis=0) if hub is None else np.asarray(hub, dtype=np.float64)
        v = np.vstack([c, pts])
        k = len(pts)
        f = np.array([(0, 1 + i, 1 + (i + 1) % k) for i in range(k if hub is None else k - 1)])
        nrm = np.cross(v[f[:, 1]] - v[0], v[f[:, 2]] - v[0]).sum(axis=0)
        if np.dot(nrm, want) < 0:
            f = f[:, ::-1]
        self.add(v, f, mat, var=var)

    def quad(self, a, b, c, d, mat, want=None, var=0.5):
        self.grid(np.array([[a, d], [b, c]], dtype=np.float64), mat, want=want, var=var)

    def box(self, x0, x1, y0, y1, z0, z1, mat, var=0.5, faces="xXyYzZ"):
        """Axis-aligned box; `faces` picks which sides are made (x = low-x side, X = high-x side...)."""
        c = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        sides = {"x": ((0, 4, 7, 3), (-1, 0, 0)), "X": ((1, 2, 6, 5), (1, 0, 0)), "y": ((0, 1, 5, 4), (0, -1, 0)),
                 "Y": ((3, 7, 6, 2), (0, 1, 0)), "z": ((0, 3, 2, 1), (0, 0, -1)), "Z": ((4, 5, 6, 7), (0, 0, 1))}
        for key in faces:
            ids, nrm = sides[key]
            self.quad(*(c[i] for i in ids), mat, want=nrm, var=var)

    # -- swept and turned shapes ------------------------------------------------------------
    def sweep(self, path, runs, mat, b=(0, 0, 1), side=1, closed=False, smooth=False, var=0.5, flip=False, ghost=(None, None)):
        """Sweep profile runs along a polyline lying in a plane with normal b. Profile u is measured
        along side * cross(direction, b), profile v along b. Corners are mitred. For a plan path
        listed left to right as the viewer sees it, side=1 points out of the wall. flip=True is for
        a profile listed with its outside on the left. ghost = (point before the path, point after
        it): only sets the mitre at that end, for a path that is one piece of a longer line."""
        P = np.asarray(path, dtype=np.float64)
        b = _unit(b)
        k = len(P)
        nseg = k if closed else k - 1
        d = np.array([_unit(P[(i + 1) % k] - P[i]) for i in range(nseg)])
        s = side * np.cross(d, b)
        mit = np.zeros((k, 3))
        for i in range(k):
            if closed:
                sa, sb = s[(i - 1) % nseg], s[i % nseg]
            else:
                sa = s[i - 1] if i > 0 else s[0]
                sb = s[i] if i < nseg else s[-1]
                if i == 0 and ghost[0] is not None:
                    sa = side * np.cross(_unit(P[0] - np.asarray(ghost[0], dtype=np.float64)), b)
                if i == nseg and ghost[1] is not None:
                    sb = side * np.cross(_unit(np.asarray(ghost[1], dtype=np.float64) - P[-1]), b)
            mit[i] = (sa + sb) / (1.0 + float(np.dot(sa, sb)))
        dist = np.concatenate([[0.0], np.cumsum([np.linalg.norm(P[(i + 1) % k] - P[i]) for i in range(nseg)])])
        off = (self.rng.random(), self.rng.random())
        for run in runs:
            run = np.asarray(run, dtype=np.float64)
            plen = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(run, axis=0), axis=1))])
            du, dv = run[1] - run[0]
            for i in range(nseg):
                j = (i + 1) % k
                G = np.empty((2, len(run), 3))
                G[0] = P[i] + np.outer(run[:, 0], mit[i]) + np.outer(run[:, 1], b)
                G[1] = P[j] + np.outer(run[:, 0], mit[j]) + np.outer(run[:, 1], b)
                want = (dv * s[i] - du * b) * (-1.0 if flip else 1.0)      # outside is on the right of the profile
                first = np.cross(G[1, 0] - G[0, 0], G[0, 1] - G[0, 0])       # facing of the run's first segment as wound
                self.grid(G, mat, reverse=bool(np.dot(first, want) < 0), smooth=smooth, var=var,
                          su=[dist[i], dist[i + 1]], sv=plen, uv_off=off)

    def lathe(self, runs, mat, segs=32, center=(0.0, 0.0, 0.0), a0=0.0, a1=360.0, smooth=True, var=0.5, ucirc=None):
        """Turn (r, z) profile runs around a vertical axis through `center`."""
        ang = np.radians(np.linspace(a0, a1, segs + 1))
        ca, sa = np.cos(ang), np.sin(ang)
        off = (self.rng.random(), self.rng.random())
        for run in runs:
            run = np.asarray(run, dtype=np.float64)
            r, z = run[:, 0], run[:, 1]
            G = np.empty((len(ang), len(run), 3))
            G[:, :, 0] = center[0] + np.outer(ca, r)
            G[:, :, 1] = center[1] + np.outer(sa, r)
            G[:, :, 2] = center[2] + z[None, :]
            plen = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(run, axis=0), axis=1))])
            rr = max(float(r.max()), 1e-4) if ucirc is None else ucirc
            i = int(np.argmax(np.linalg.norm(np.diff(run, axis=0), axis=1)))
            dr, dz = run[i + 1] - run[i]
            mid = (ang[0] + ang[1]) / 2
            want = dz * np.array([math.cos(mid), math.sin(mid), 0.0]) - dr * np.array([0.0, 0.0, 1.0])
            nrm = np.cross(G[1, i] - G[0, i], G[0, i + 1] - G[0, i])
            n, m = G.shape[:2]
            idx = np.arange(n * m).reshape(n, m)
            quads = np.stack([idx[:-1, :-1], idx[1:, :-1], idx[1:, 1:], idx[:-1, 1:]], axis=-1).reshape(-1, 4)
            if np.dot(nrm, want) < 0:
                quads = quads[:, ::-1]
            uu, vv = np.meshgrid(ang * rr, plen, indexing='ij')
            guv = np.stack([uu, vv], axis=-1).reshape(-1, 2) / TILE
            self.add(G.reshape(-1, 3), quads, mat, uv=guv[quads] + np.asarray(off), smooth=smooth, var=var)

    def tube(self, pts, radius, mat, sides=8, closed=False, var=0.5, flat=1.0):
        """Round (or flattened) rod along 3D points; radius may be one number or one per point."""
        P = np.asarray(pts, dtype=np.float64)
        k = len(P)
        rad = np.full(k, radius) if np.isscalar(radius) else np.asarray(radius, dtype=np.float64)
        loops = np.empty((k + (1 if closed else 0), sides + 1, 3))
        prev = None
        a = np.linspace(0, 2 * math.pi, sides + 1)
        for i in range(k):
            t = _unit(P[(i + 1) % k] - P[i - 1]) if closed else _unit(P[min(i + 1, k - 1)] - P[max(i - 1, 0)])
            if prev is None:
                ref = np.array([0.0, 0.0, 1.0]) if abs(t[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
                nrm = _unit(np.cross(t, ref))
            else:
                nrm = _unit(prev - t * np.dot(prev, t))
            prev = nrm
            bi = np.cross(t, nrm)
            loops[i] = P[i] + rad[i] * (np.outer(np.cos(a), nrm) + flat * np.outer(np.sin(a), bi))
        if closed:
            loops[k] = loops[0]
        self.grid(loops, mat, want=('from', P.mean(axis=0)), smooth=True, var=var,
                  su=np.arange(len(loops)) * 0.05, sv=np.arange(sides + 1) * 0.02)

    # -- finishing --------------------------------------------------------------------------
    def triangles(self):
        return sum(len(c[1]) * (2 if c[1].shape[1] == 4 else 1) for c in self.chunks)

    def to_object(self, name, colors):
        """Write the mesh. `colors` maps material name -> viewport colour for previews."""
        nv = nloops = 0
        V, LV, LS, MAT, SM, UV, VAR = [], [], [], [], [], [], []
        for v, f, mat, uv, smooth, var in self.chunks:
            k = f.shape[1]
            V.append(v)
            LV.append((f + nv).ravel())
            LS.append(nloops + np.arange(len(f)) * k)
            MAT.append(np.full(len(f), mat, dtype=np.int32))
            SM.append(np.full(len(f), smooth, dtype=bool))
            UV.append(uv.reshape(-1, 2))
            VAR.append(var[f].ravel())
            nloops += len(f) * k
            nv += len(v)
        V = np.concatenate(V)
        LV = np.concatenate(LV).astype(np.int32)
        LS = np.concatenate(LS).astype(np.int32)
        mesh = bpy.data.meshes.new(name)
        mesh.vertices.add(len(V))
        mesh.vertices.foreach_set("co", V.ravel())
        mesh.loops.add(len(LV))
        mesh.loops.foreach_set("vertex_index", LV)
        mesh.polygons.add(len(LS))
        mesh.polygons.foreach_set("loop_start", LS)
        mesh.polygons.foreach_set("material_index", np.concatenate(MAT))
        mesh.polygons.foreach_set("use_smooth", np.concatenate(SM))
        uvl = mesh.uv_layers.new(name="UVMap")
        uvl.data.foreach_set("uv", np.concatenate(UV).ravel().astype(np.float32))
        var = np.concatenate(VAR)
        col = np.zeros((len(var), 4), dtype=np.float32)      # R tone, G dirt in crevices, B rain streaks, A large patches
        col[:, 0] = var
        col[:, 3] = 0.5
        ca = mesh.color_attributes.new("Col", 'BYTE_COLOR', 'CORNER')
        ca.data.foreach_set("color_srgb", col.ravel())
        for mname in self.materials:
            m = bpy.data.materials.get(mname) or bpy.data.materials.new(mname)
            m.diffuse_color = (*colors.get(mname, (0.7, 0.7, 0.7)), 1.0)
            mesh.materials.append(m)
        mesh.update(calc_edges=True)
        mesh.validate(verbose=False)
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def bake_cavity(obj, distance=0.07, samples=24, gain=1.6):
    """Bakes ambient occlusion into the green vertex-colour channel as dirt in crevices
    (0 = open surface, 1 = deep in a joint or under a ledge)."""
    scene = bpy.context.scene
    engine = scene.render.engine
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = samples
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.light_settings.distance = distance
    mesh = obj.data
    ao = mesh.color_attributes.new("AO", 'FLOAT_COLOR', 'CORNER')
    mesh.color_attributes.active_color = ao
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.bake(type='AO', target='VERTEX_COLORS')
    n = len(mesh.loops)
    a = np.empty(n * 4, dtype=np.float32)
    mesh.color_attributes["AO"].data.foreach_get("color", a)
    occl = np.clip((1.0 - a.reshape(-1, 4)[:, 0]) * gain, 0.0, 1.0)
    mesh.color_attributes.remove(mesh.color_attributes["AO"])
    col = mesh.color_attributes["Col"]
    c = np.empty(n * 4, dtype=np.float32)
    col.data.foreach_get("color_srgb", c)
    c = c.reshape(-1, 4)
    c[:, 1] = occl
    col.data.foreach_set("color_srgb", c.ravel())
    mesh.color_attributes.active_color = col
    scene.render.engine = engine
    return float(occl.mean())


def audit_see_through(obj, wall_y, x0, x1, z0, z1, eyes, step=0.015, cell=0.25):
    """Looks for holes the way a player would: from every eye point (name, (x, y, z) in the object's
    coordinates) a ray goes to each point of a grid on the wall plane y = wall_y. Back faces are
    skipped, as the renderer culls them. A ray that then hits nothing sees straight through the
    stonework. Prints where such rays cluster and returns how many there are."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    tree = BVHTree.FromObject(obj, bpy.context.evaluated_depsgraph_get())
    xs, zs = np.arange(x0, x1, step), np.arange(z0, z1, step)
    total = 0
    for name, eye in eyes:
        e = Vector(eye)
        through, behind, example = {}, 0, {}
        for x in xs:
            for z in zs:
                d = (Vector((x, wall_y, z)) - e).normalized()
                o, first, trail = e, True, []
                for _ in range(40):
                    loc, nrm, _idx, _dist = tree.ray_cast(o, d, 300.0)
                    if loc is None or nrm.dot(d) < 0.0:
                        break
                    trail.append(tuple(round(c, 3) for c in loc))
                    if first:
                        behind += 1
                        first = False
                    o = loc + d * 2e-4
                if loc is None:
                    key = (round(math.floor(x / cell) * cell, 2), round(math.floor(z / cell) * cell, 2))
                    through[key] = through.get(key, 0) + 1
                    example.setdefault(key, ((round(x, 3), round(z, 3)), trail[:5]))
        n = sum(through.values())
        total += n
        worst = sorted(through.items(), key=lambda kv: -kv[1])[:10]
        print(f"AUDIT eye '{name}': {n} of {len(xs) * len(zs)} rays see through, {behind} meet a back face first; "
              f"worst {cell} m cells (x, z): {[(k, v) for k, v in worst]}")
        for k, _v in worst[:4]:         # one ray per cell: its target on the wall plane and the back faces it passed, nearest first
            print(f"AUDIT   example towards (x, z) {example[k][0]}: back faces passed at {example[k][1]}")
    return total


# ---------------------------------------------------------------------------------------------
# Cut-stone walls
# ---------------------------------------------------------------------------------------------
def _minus(rects, hole):
    """Rectangles (x0, x1, z0, z1) minus one hole rectangle."""
    hx0, hx1, hz0, hz1 = hole
    out = []
    for x0, x1, z0, z1 in rects:
        if hx0 >= x1 or hx1 <= x0 or hz0 >= z1 or hz1 <= z0:
            out.append((x0, x1, z0, z1))
            continue
        if hx0 > x0:
            out.append((x0, hx0, z0, z1))
        if hx1 < x1:
            out.append((hx1, x1, z0, z1))
        cx0, cx1 = max(x0, hx0), min(x1, hx1)
        if hz0 > z0:
            out.append((cx0, cx1, z0, hz0))
        if hz1 < z1:
            out.append((cx0, cx1, hz1, z1))
    return out


def block_wall(B, x0, x1, z0, z1, mat, holes=(), y=0.0, course=0.31, length=(0.55, 0.95), chamfer=0.006,
               jitter=0.002, seed=1):
    """A wall face of individual stone blocks in running bond. Every block has its own chamfered
    edge, a face that sits up to `jitter` proud, its own piece of the texture and its own tone."""
    rng = random.Random(seed)
    rows = max(1, round((z1 - z0) / course))
    h = (z1 - z0) / rows
    V, F, UVS, VAR = [], [], [], []
    for r in range(rows):
        za, zb = z0 + r * h, z0 + (r + 1) * h
        x = x0 - rng.uniform(0.0, length[1])
        while x < x1:
            ln = rng.uniform(*length)
            xa, xb = max(x, x0), min(x + ln, x1)
            x += ln
            if xb - xa < 1e-6:
                continue
            if 0 < x1 - xb < 0.18:              # no slivers at the end of a course
                xb = x = x1
            pieces = [(xa, xb, za, zb)]
            for hole in holes:
                pieces = _minus(pieces, hole[:4])
            tone = min(1.0, max(0.0, rng.gauss(0.5, 0.13)))
            if rng.random() < 0.06:
                tone = min(1.0, max(0.0, tone + rng.choice((-0.3, 0.28))))
            uo, vo = rng.random() * TILE, rng.random() * TILE
            proud = rng.uniform(0.0, jitter)
            for px0, px1, pz0, pz1 in pieces:
                if px1 - px0 < 0.02 or pz1 - pz0 < 0.02:
                    continue
                c = min(chamfer, (px1 - px0) / 3, (pz1 - pz0) / 3)
                o = len(V)
                V += [(px0, y + c, pz0), (px1, y + c, pz0), (px1, y + c, pz1), (px0, y + c, pz1),
                      (px0 + c, y - proud, pz0 + c), (px1 - c, y - proud, pz0 + c),
                      (px1 - c, y - proud, pz1 - c), (px0 + c, y - proud, pz1 - c)]
                quads = [(o + 4, o + 5, o + 6, o + 7)] + [(o + i, o + (i + 1) % 4, o + 4 + (i + 1) % 4, o + 4 + i) for i in range(4)]
                F += quads
                for q in quads:
                    UVS.append([((V[i][0] + uo) / TILE, (V[i][2] + vo) / TILE) for i in q])
                VAR += [tone] * 8
    if V:
        B.add(np.array(V), np.array(F), mat, uv=np.array(UVS), var=np.array(VAR))
    # a plate behind the joints, so nothing shows through where a sliver was dropped
    back = [(x0, x1, z0, z1)]
    for hole in holes:
        back = _minus(back, hole[:4])
    yb = y + chamfer + 0.002
    for bx0, bx1, bz0, bz1 in back:
        B.quad((bx0, yb, bz0), (bx1, yb, bz0), (bx1, yb, bz1), (bx0, yb, bz1), mat, want=(0, -1, 0), var=0.2)
    # A return round every hole, facing the wall: the blocks beside a hole have chamfered edges, and
    # without it a slanting sight line slips behind whatever flat piece is set into the hole.
    # A hole may carry a fifth item naming edges without a return ('t' top, 'b' bottom): the strips that
    # follow an arch (stair_holes) are open towards each other, or a ribbon would cross the opening.
    for hole in holes:
        hx0, hx1, hz0, hz1 = hole[:4]
        skip = hole[4] if len(hole) > 4 else ""
        a, b, c, d = max(hx0, x0), min(hx1, x1), max(hz0, z0), min(hz1, z1)
        if b - a <= 0 or d - c <= 0:
            continue
        B.quad((a, y, c), (a, yb, c), (a, yb, d), (a, y, d), mat, want=(-1, 0, 0), var=0.2)
        B.quad((b, y, c), (b, yb, c), (b, yb, d), (b, y, d), mat, want=(1, 0, 0), var=0.2)
        if "b" not in skip:
            B.quad((a, y, c), (b, y, c), (b, yb, c), (a, yb, c), mat, want=(0, 0, -1), var=0.2)
        if "t" not in skip:
            B.quad((a, y, d), (b, y, d), (b, yb, d), (a, yb, d), mat, want=(0, 0, 1), var=0.2)


def seal_inside_corner(B, mat, c, na, nb, z0, z1, back=0.008, lap=0.05):
    """Hidden strips behind an inside corner of two block walls. The end blocks of both walls are
    chamfered, which leaves a slit a few millimetres wide where their back plates meet, and a slanting
    sight line gets through it. c: the corner (x, y); na, nb: the outward normals of the two wall
    faces. Each wall's back plate is carried on past the corner, into the solid behind the other wall."""
    c, na, nb = (np.array(v, dtype=np.float64) for v in (c, na, nb))
    for n, m in ((na, nb), (nb, na)):
        p0 = c - n * back + m * 0.02            # a little before the corner, on this wall's back plate
        p1 = c - n * back - m * lap             # past the corner, behind the other wall
        B.quad((p0[0], p0[1], z0), (p1[0], p1[1], z0), (p1[0], p1[1], z1), (p0[0], p0[1], z1), mat, want=(n[0], n[1], 0.0), var=0.2)


def slab_floor(B, x0, x1, y0, y1, z, mat, size=(0.9, 0.6), chamfer=0.005, seed=3):
    """Paving slabs lying flat, top at z."""
    rng = random.Random(seed)
    rows = max(1, round((y1 - y0) / size[1]))
    d = (y1 - y0) / rows
    c = chamfer
    V, F, VAR = [], [], []
    for r in range(rows):
        ya, yb = y0 + r * d, y0 + (r + 1) * d
        cuts = [x0]
        x = x0 - rng.uniform(0, size[0])
        while True:
            x += rng.uniform(size[0] * 0.7, size[0] * 1.3)
            if x >= x1 - 0.15:
                break
            if x > cuts[-1] + 0.15:             # no slivers: a short end piece joins its neighbour
                cuts.append(x)
        cuts.append(x1)
        for xa, xb in zip(cuts, cuts[1:]):
            o = len(V)
            V += [(xa, ya, z - c), (xb, ya, z - c), (xb, yb, z - c), (xa, yb, z - c),
                  (xa + c, ya + c, z), (xb - c, ya + c, z), (xb - c, yb - c, z), (xa + c, yb - c, z)]
            F += [(o + 4, o + 5, o + 6, o + 7)] + [(o + i, o + (i + 1) % 4, o + 4 + (i + 1) % 4, o + 4 + i) for i in range(4)]
            VAR += [min(1.0, max(0.0, rng.gauss(0.5, 0.12)))] * 8
    B.add(np.array(V), np.array(F), mat, var=np.array(VAR))


# ---------------------------------------------------------------------------------------------
# Classical parts
# ---------------------------------------------------------------------------------------------
def attic_base(B, mat, r=0.18, plinth=0.5, var=0.5):
    """Column base: square plinth, torus, scotia, torus. Stands on z=0; returns its height."""
    hp = 0.075
    B.box(-plinth / 2, plinth / 2, -plinth / 2, plinth / 2, 0.0, hp, mat, var=var, faces="xXyYZ")
    r1, r2, rs = r * 1.33, r * 1.2, r * 1.1 + 0.012      # lower torus, upper torus, the fillets of the scotia
    t1, t2, hs, f = 0.06, 0.045, 0.045, 0.008
    half = np.radians(np.linspace(-90, 90, 9))
    z = hp
    run = [(r1 - t1 / 2 + (t1 / 2) * math.cos(a), z + t1 / 2 + (t1 / 2) * math.sin(a)) for a in half]
    z += t1
    run += [(rs, z), (rs, z + f)]
    run += [(rs - 0.016 * math.sin(math.pi * i / 8), z + f + hs * i / 8) for i in range(1, 8)]
    z += f + hs
    run += [(rs, z), (rs, z + f), (r2 - t2 / 2, z + f)]
    z += f
    run += [(r2 - t2 / 2 + (t2 / 2) * math.cos(a), z + t2 / 2 + (t2 / 2) * math.sin(a)) for a in half[1:]]
    z += t2
    run += [(r + 0.012, z), (r + 0.012, z + 0.012)]
    z += 0.012
    B.lathe([run], mat, segs=40, var=var)
    return z


def column_shaft(B, mat, r0=0.18, r1=0.153, height=2.7, var=0.5):
    """Smooth shaft on z=0 with entasis: straight for the lower third, then a gentle taper."""
    run = [(r0 + 0.012, 0.0)]
    run += [(r0 + 0.012 * (1 - math.sin(math.pi / 2 * i / 4)), 0.03 * i / 4) for i in range(1, 5)]      # apophyge
    for i in range(1, 17):
        t = i / 16
        tt = max(0.0, (t - 0.33) / 0.67)
        run.append((r0 - (r0 - r1) * tt ** 1.6, 0.03 + (height - 0.07) * t))
    run += [(r1 + 0.01, height - 0.04), (r1 + 0.01, height - 0.03)]
    run += [(r1 + 0.01 + 0.012 * math.cos(a), height - 0.018 + 0.012 * math.sin(a)) for a in np.radians(np.linspace(-90, 90, 7))]   # astragal
    run += [(r1, height - 0.006), (r1, height)]
    B.lathe([run], mat, segs=40, var=var, ucirc=r0)


def _leaf(B, mat, r_bell, z0, h, width, ang, curl=0.035, var=0.5, nu=10, clear=0.014):
    """One acanthus leaf hugging the bell of a capital: three lobes a side, fluted, with a tip that
    rolls outward and down."""
    t = 0.006
    vs = [0.72 * i / 12 for i in range(13)] + [0.72 + 0.28 * i / 12 for i in range(1, 13)]
    rows_f, rows_b = [], []
    for v in vs:
        if v <= 0.72:
            zz = z0 + h * v / 0.72
            rad = r_bell(zz) + clear + 0.012 * (v / 0.72) ** 2
            tilt = 0.0
        else:
            a = (v - 0.72) / 0.28 * math.radians(230)
            zc = z0 + h
            rc = r_bell(zc) + clear + 0.012 + curl
            rad = rc - curl * math.cos(a)
            zz = zc + curl * math.sin(a)
            tilt = a
        body = min(v / 0.72, 1.0)
        w = width * (0.55 + 0.45 * math.sin(math.pi * min(body * 0.9, 1.0))) * (1.0 - 0.6 * max(0.0, (v - 0.72) / 0.28))
        notch = abs(math.sin(math.pi * 3 * body))                                   # three lobes with pointed notches between
        w *= 1.0 - 0.24 * (1.0 - notch ** 0.5) * (1.0 if v <= 0.72 else 0.0)
        rf, rb = [], []
        for i in range(nu + 1):
            u = -1 + 2 * i / nu
            flute = 0.0035 * math.cos(5 * math.pi * u) * (0.35 + 0.65 * body) * (1.0 - 0.5 * max(0.0, (v - 0.72) / 0.28))
            rr = rad + 0.008 * u * u + flute + 0.004 * max(0.0, 1.0 - abs(u) * 5)     # edges lift, flutes, a raised midrib
            a2 = ang + (u * w / 2) / max(rad, 0.05)
            rf.append((rr * math.cos(a2), rr * math.sin(a2), zz))
            ri = rr - t * math.cos(tilt)
            rb.append((ri * math.cos(a2), ri * math.sin(a2), zz - t * math.sin(tilt)))
        rows_f.append(rf)
        rows_b.append(rb)
    axis = np.array([0.0, 0.0, z0 + h / 2])
    su, sv = np.array(vs) * h * 1.4, np.arange(nu + 1) * (width / nu)
    F, Bk = np.array(rows_f), np.array(rows_b)
    B.grid(F, mat, want=('from', axis), smooth=True, var=var, su=su, sv=sv)
    B.grid(Bk, mat, want=('to', axis), smooth=True, var=var, su=su, sv=sv)
    # the thickness of the leaf along both sides and the tip, each strip facing away from the leaf
    for ef, eb, out in ((F[:, 0], Bk[:, 0], F[:, 0] - F[:, 1]), (F[:, -1], Bk[:, -1], F[:, -1] - F[:, -2]), (F[-1], Bk[-1], F[-1] - F[-2])):
        B.grid(np.stack([ef, eb], axis=1), mat, want=out.mean(axis=0), var=var)


def corinthian_capital(B, mat, r=0.153, height=0.44, abacus=0.52, var=0.5):
    """Bell, two rows of eight acanthus leaves, corner volutes and a concave-sided abacus.
    Stands on z=0 (top of the shaft); returns its height."""
    hb = height * 0.86

    def r_bell(z):
        t = min(max(z / hb, 0.0), 1.0)
        return r + 0.004 + (abacus * 0.36 - r) * t ** 2.2

    run = [(r_bell(hb * i / 12), hb * i / 12) for i in range(13)]
    run += [(r_bell(hb) + 0.012, hb + 0.004), (r_bell(hb) + 0.012, hb + 0.014), (r_bell(hb), hb + 0.014)]
    B.lathe([run], mat, segs=32, var=var)
    for k in range(8):
        _leaf(B, mat, r_bell, 0.012, height * 0.30, 0.15 * r / 0.153, math.radians(k * 45 + 22.5), curl=0.026, var=var, clear=0.026)
    for k in range(8):
        _leaf(B, mat, r_bell, 0.02, height * 0.56, 0.15 * r / 0.153, math.radians(k * 45), curl=0.032, var=var, clear=0.012)
    # corner volutes: a stalk rising to the abacus corner and curling in on itself
    for k in range(4):
        a = math.radians(45 + 90 * k)
        e = np.array([math.cos(a), math.sin(a), 0.0])
        up = np.array([0.0, 0.0, 1.0])
        pts, rad = [], []
        rb = r_bell(height * 0.42)
        for i in range(9):
            t = i / 8
            pts.append(e * (rb + 0.014 + (abacus * 0.56 - rb) * t ** 1.5) + up * height * (0.42 + 0.36 * t))
            rad.append(0.009 + 0.004 * t)
        cx, cz, R = abacus * 0.56, height * 0.78 - 0.036, 0.036
        for i in range(1, 29):
            th = math.radians(90 - i * 17)
            rr = R * (1 - i / 34)
            pts.append(e * (cx + rr * math.cos(th)) + up * (cz + rr * math.sin(th)))
            rad.append(max(0.003, 0.013 * (1 - i / 32)))
        B.tube(pts, rad, mat, sides=8, var=var, flat=1.4)
    # small helices and a fleuron in the middle of each side
    for k in range(4):
        a = math.radians(90 * k)
        e = np.array([math.cos(a), math.sin(a), 0.0])
        s = np.array([-math.sin(a), math.cos(a), 0.0])
        for sgn in (-1, 1):
            pts, rad = [], []
            for i in range(22):
                th = math.radians(-90 + i * 20)
                rr = 0.026 * (1 - i / 26)
                pts.append(e * (r_bell(height * 0.72) + 0.02) + s * sgn * (0.03 + rr * math.cos(th)) + np.array([0, 0, height * 0.7 + rr * math.sin(th)]))
                rad.append(max(0.003, 0.01 * (1 - i / 26)))
            B.tube(pts, rad, mat, sides=6, var=var)
        # rosette built along +z, turned to face outwards on this side of the abacus
        turn = np.array([[0, 0, 1, 0], [0, 1, 0, 0], [-1, 0, 0, 0], [0, 0, 0, 1.0]])
        B.push(translate(0, 0, height * 0.925) @ rot_z(math.degrees(a)) @ translate(abacus * 0.428, 0, 0) @ turn)
        rosette(B, mat, 0.04, 0.02, petals=6, var=var)
        B.pop()
    # abacus: four concave sides with cut corners
    ha, half, sag, cut = height * 0.14, abacus / 2, abacus * 0.075, abacus * 0.07
    plan = []
    for k in range(4):
        a = math.radians(90 * k)
        ca, sa = math.cos(a), math.sin(a)
        for i in range(9):
            t = -1 + 2 * i / 8
            xx, yy = half - sag * (1 - t * t), t * (half - cut)
            plan.append((xx * ca - yy * sa, xx * sa + yy * ca, 0.0))
    z = height - ha
    prof = [[(0.0, 0.0), (0.0, ha * 0.35)],
            [(0.0, ha * 0.35)] + [(0.012 * math.sin(math.pi / 2 * i / 4), ha * 0.35 + ha * 0.3 * i / 4) for i in range(1, 5)],
            [(0.012, ha * 0.65), (0.02, ha * 0.65), (0.02, ha), (-0.03, ha)]]
    B.sweep(np.array(plan) + np.array([0, 0, z]), prof, mat, closed=True, var=var)
    B.fan(np.array(plan) * 0.93 + np.array([0, 0, height]), mat, (0, 0, 1), var=var)
    B.fan(np.array(plan) + np.array([0, 0, z]), mat, (0, 0, -1), var=var)
    return height


def rosette(B, mat, radius, relief, petals=8, var=0.5, layers=2, boss=True):
    """Flower boss lying in the x-y plane, rising along +z. Petals as a height field on a polar grid."""
    for layer in range(layers):
        R = radius * (1.0 if layer == 0 else 0.62)
        H = relief * (0.55 if layer == 0 else 0.95)
        rot = 0.0 if layer == 0 else math.pi / petals
        nr, na = 7, petals * 8
        rho = np.linspace(0, 1, nr + 1)[:, None]
        phi = np.linspace(0, 2 * math.pi, na + 1)[None, :]
        lobe = np.abs(np.cos(petals * (phi - rot) / 2)) ** 0.7
        rr = R * rho * (0.74 + 0.26 * lobe)
        hh = H * (np.sin(math.pi * np.minimum(rho * 1.08, 1.0)) ** 0.8) * (0.35 + 0.65 * lobe) + (relief * 0.25 if layer else 0.0) * (1 - rho)
        G = np.stack([rr * np.cos(phi), rr * np.sin(phi), hh], axis=-1)
        B.grid(G, mat, want=(0, 0, 1), smooth=True, var=var, su=np.arange(nr + 1) * 0.01, sv=np.arange(na + 1) * 0.01)
    if boss:
        run = [(radius * 0.2 * math.cos(a), relief * 0.75 + radius * 0.2 * math.sin(a)) for a in np.radians(np.linspace(0, 90, 6))]
        B.lathe([run], mat, segs=12, var=var)


def stud(B, mat, size, height, var=0.5):
    """Pyramid-head nail, base in the x-y plane, point along +z."""
    h = size / 2
    v = np.array([(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0), (0, 0, height)])
    B.add(v, np.array([(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]), mat, var=var)


def pedestal(B, mat, width, depth, height, base_h=0.16, cap_h=0.12, var=0.5):
    """Pedestal against the wall: centred on x=0, from the wall face (y=0) out to y=-depth, on z=0."""
    w = width / 2
    path = [(-w, 0.0, 0.0), (-w, -depth, 0.0), (w, -depth, 0.0), (w, 0.0, 0.0)]
    base = [[(0.055, 0.0), (0.055, base_h * 0.5)],
            [(0.055, base_h * 0.5)] + [(0.055 - 0.022 * (1 - math.cos(a)), base_h * (0.5 + 0.2 * math.sin(a))) for a in QUARTER],
            [(0.033, base_h * 0.7), (0.026, base_h * 0.7), (0.026, base_h * 0.78)],
            [(0.026, base_h * 0.78)] + [(0.026 * (1 - math.sin(a)), base_h * (0.78 + 0.22 * (1 - math.cos(a)))) for a in QUARTER]]
    B.sweep(path, base, mat, var=var)
    zc = height - cap_h
    cap = [[(0.0, 0.0)] + [(0.03 * (1 - math.cos(a)), cap_h * 0.3 * math.sin(a)) for a in QUARTER],
           [(0.03, cap_h * 0.3), (0.045, cap_h * 0.3), (0.045, cap_h * 0.42)],
           [(0.045, cap_h * 0.42), (0.065, cap_h * 0.42), (0.065, cap_h * 0.8)],
           [(0.065, cap_h * 0.8)] + [(0.065 + 0.012 * math.sin(a), cap_h * (0.8 + 0.2 * (1 - math.cos(a)))) for a in QUARTER]]
    B.sweep(path, [[(0.0, base_h), (0.0, zc)]], mat, var=var)
    B.sweep([(p[0], p[1], zc) for p in path], cap, mat, var=var)
    e = 0.077
    B.quad((-w - e, -depth - e, height), (w + e, -depth - e, height), (w + e, 0.0, height), (-w - e, 0.0, height), mat, want=(0, 0, 1), var=var)


ENTABLATURE_H = 0.92
CORNICE_OUT = 0.285


def entablature_profile():
    """Architrave, frieze and cornice as profile runs; u = out from the path, v = up from 0 to ENTABLATURE_H."""
    return [
        [(0.0, 0.0), (0.0, 0.09)], [(0.0, 0.09), (0.012, 0.09), (0.012, 0.19)],
        [(0.012, 0.19)] + [(0.012 + 0.03 * (1 - math.cos(a)), 0.19 + 0.05 * math.sin(a)) for a in QUARTER],
        [(0.042, 0.24), (0.05, 0.24), (0.05, 0.265), (0.0, 0.265)],
        [(0.0, 0.265), (0.0, 0.555)],                                                        # frieze
        [(0.0, 0.555)] + [(0.045 * math.sin(a), 0.555 + 0.045 * (1 - math.cos(a))) for a in QUARTER],
        [(0.045, 0.6), (0.055, 0.6), (0.055, 0.69)],                                          # dentil band
        [(0.055, 0.69), (0.2, 0.705), (0.2, 0.78)],                                           # corona
        [(0.2, 0.78)] + [(0.2 + 0.075 * i / 6, 0.78 + 0.1 * (0.5 - 0.5 * math.cos(math.pi * i / 6))) for i in range(1, 7)],
        [(0.275, 0.88), (CORNICE_OUT, 0.88), (CORNICE_OUT, ENTABLATURE_H)],
    ]


def entablature(B, mat, path, z, dentils=True, var=0.5, end_caps=True):
    """Entablature swept along a plan path of (x, y) points listed left to right; it breaks forward
    wherever the path steps out. Adds dentils, a soffit under every part that stands proud of the
    wall, a top face and, with end_caps, a flat end at both ends of the path."""
    profile = entablature_profile()
    B.sweep([(x, y, z) for x, y in path], profile, mat, var=var)
    zt = z + ENTABLATURE_H
    if end_caps:
        chain = [p for run in profile for p in run]
        for (xe, ye), nx in ((path[0], -1), (path[-1], 1)):
            for (u1, v1), (u2, v2) in zip(chain, chain[1:]):
                if v2 - v1 > 1e-9:
                    B.quad((xe, 0.0, z + v1), (xe, ye - u1, z + v1), (xe, ye - u2, z + v2), (xe, 0.0, z + v2), mat, want=(nx, 0, 0), var=var)
    for (xa, ya), (xb, yb) in zip(path, path[1:]):
        along_x = abs(xb - xa) > 1e-6
        if along_x and ya < -1e-6:
            B.quad((xa, ya, z), (xb, ya, z), (xb, 0.0, z), (xa, 0.0, z), mat, want=(0, 0, -1), var=var)
        if along_x:
            B.quad((xa - CORNICE_OUT, ya - CORNICE_OUT, zt), (xb + CORNICE_OUT, ya - CORNICE_OUT, zt), (xb + CORNICE_OUT, 0.0, zt),
                   (xa - CORNICE_OUT, 0.0, zt), mat, want=(0, 0, 1), var=var)
        if not dentils:
            continue
        if along_x:
            n = max(1, int(abs(xb - xa) / 0.085))
            for i in range(n):
                xc = xa + (i + 0.5) * (xb - xa) / n
                B.box(xc - 0.026, xc + 0.026, ya - 0.105, ya - 0.05, z + 0.605, z + 0.685, mat, var=var, faces="xXyz")
        else:
            n = max(1, int(abs(yb - ya) / 0.085))
            sgn = 1 if yb < ya else -1              # stepping out: this return faces -x; stepping back: +x
            for i in range(n):
                yc = ya + (i + 0.5) * (yb - ya) / n
                xo = xa - sgn * 0.05
                B.box(min(xo, xo - sgn * 0.055), max(xo, xo - sgn * 0.055), yc - 0.026, yc + 0.026, z + 0.605, z + 0.685, mat, var=var, faces="xXyYz")


def frame_profile(width, proud):
    """Architrave band around an opening: u = away from the opening edge, v = out of the wall. Listed
    from the outer edge to the opening edge, so the outside is on the right."""
    w, p = width, proud
    return [[(w, 0.0), (w, p), (w * 0.86, p)],
            [(w * 0.86, p)] + [(w * (0.86 - 0.3 * i / 5), p - p * 0.42 * math.sin(math.pi / 2 * i / 5)) for i in range(1, 6)],
            [(w * 0.56, p * 0.58), (w * 0.5, p * 0.58), (w * 0.5, p * 0.4), (w * 0.26, p * 0.4)],
            [(w * 0.26, p * 0.4), (w * 0.26, p * 0.24), (0.0, p * 0.24), (0.0, 0.0)]]


def shell_niche(B, mat, radius, z0, z_spring, frame_w=0.09, ribs=11, var=0.5):
    """Half-round niche sunk into the wall, centred on x=0: floor at z0, a scallop-shell half dome
    above z_spring, a moulded frame on the wall face, an impost band, a keystone and a sill."""
    r = radius
    nb, nt = ribs * 8, 14                                     # points round the front rim / from hinge to rim
    na = 2 * nt                                               # the back wall meets the shell point for point
    ang = np.radians(np.linspace(180, 0, na + 1))             # left to right around the back
    zs = np.linspace(z0, z_spring, 6)
    G = np.empty((na + 1, len(zs), 3))
    G[:, :, 0] = (r * np.cos(ang))[:, None]
    G[:, :, 1] = (r * np.sin(ang))[:, None]
    G[:, :, 2] = zs[None, :]
    B.grid(G, mat, want=(0, -1, 0), smooth=True, var=var, su=np.linspace(0, math.pi * r, na + 1), sv=zs - z0)
    B.fan([(r * math.cos(a), r * math.sin(a), z0) for a in ang], mat, (0, 0, 1), var=var)
    band = [[(0.0, -0.05), (0.018, -0.05), (0.018, -0.012)],
            [(0.018, -0.012)] + [(0.018 - 0.018 * math.sin(math.pi / 2 * i / 4), -0.012 + 0.012 * i / 4) for i in range(1, 5)]]
    B.sweep([(r * math.cos(a), r * math.sin(a), z_spring) for a in ang], band, mat, var=var)
    # shell: ribs fan out from the hinge at the back of the springing line to the front rim
    S = np.empty((nb + 1, nt + 1, 3))
    for i in range(nb + 1):
        al = math.pi - math.pi * i / nb                         # around the front rim, left to right
        flute = 0.5 - 0.5 * math.cos(2 * math.pi * ribs * i / nb)        # 0 on the rib crests, 1 in the flutes
        for j in range(nt + 1):
            t = (math.pi / 2) * j / nt
            rr = r * (1.0 - 0.085 * (math.sin(t) ** 1.2) * (flute ** 0.8))
            S[i, j] = (rr * math.sin(t) * math.cos(al), rr * math.cos(t), z_spring + rr * math.sin(t) * math.sin(al))
    B.grid(S, mat, want=(0, -0.6, -0.8), smooth=True, var=var, su=np.linspace(0, math.pi * r, nb + 1), sv=np.linspace(0, r * 1.5, nt + 1))
    arch = [(r * math.cos(math.pi - math.pi * i / nb), 0.0, z_spring + r * math.sin(math.pi - math.pi * i / nb)) for i in range(nb + 1)]
    B.grid(np.stack([S[:, nt], np.array(arch)], axis=1), mat, want=(0, -1, 0), var=var)     # closes the gap behind the scallops
    B.push(translate(0.0, r * 0.93, z_spring + 0.012) @ FACE_OUT)       # hinge boss of the shell
    rosette(B, mat, r * 0.17, r * 0.1, petals=5, var=var, layers=1)
    B.pop()
    # frame on the wall face: up the left jamb, round the arch, down the right jamb
    B.sweep([(-r, 0.0, z0)] + arch + [(r, 0.0, z0)], frame_profile(frame_w, 0.038), mat, b=(0, -1, 0), side=-1, var=var)
    # the wall face between the round head and the square hole left in the block wall
    for sx in (-1, 1):
        pts = [(sx * r * math.cos(a), 0.0, z_spring + r * math.sin(a)) for a in np.radians(np.linspace(0, 90, 13))]
        B.fan(pts, mat, (0, -1, 0), var=var, hub=(sx * r, 0.0, z_spring + r))
    # keystone
    kz = z_spring + r
    kv = np.array([(-0.05, -0.05, kz - 0.035), (0.05, -0.05, kz - 0.035), (0.07, -0.055, kz + frame_w + 0.03), (-0.07, -0.055, kz + frame_w + 0.03),
                   (-0.05, 0.0, kz - 0.035), (0.05, 0.0, kz - 0.035), (0.07, 0.0, kz + frame_w + 0.03), (-0.07, 0.0, kz + frame_w + 0.03)])
    B.add(kv, np.array([(0, 1, 2, 3), (4, 0, 3, 7), (1, 5, 6, 2), (3, 2, 6, 7), (4, 5, 1, 0)]), mat, var=var)
    # sill: a moulded ledge under the opening with returns
    sw = r + frame_w + 0.03
    sill = [[(0.0, -0.11), (0.03, -0.11), (0.03, -0.075)],
            [(0.03, -0.075)] + [(0.03 + 0.03 * (1 - math.cos(a)), -0.075 + 0.04 * math.sin(a)) for a in QUARTER],
            [(0.06, -0.035), (0.075, -0.035), (0.075, 0.0), (0.0, 0.0)]]
    B.sweep([(-sw, 0.0, z0), (-sw, -0.001, z0), (sw, -0.001, z0), (sw, 0.0, z0)], sill, mat, var=var)


def statue_plinth(B, mat, width, height, depth, var=0.5):
    """Low moulded block a statue stands on, centred on x=0, back at y=depth, front at y=0, on z=0."""
    w = width / 2
    path = [(-w, depth, 0.0), (-w * 0.72, 0.0, 0.0), (w * 0.72, 0.0, 0.0), (w, depth, 0.0)]
    prof = [[(0.012, 0.0), (0.012, height * 0.28)],
            [(0.012, height * 0.28)] + [(0.012 * (1 - math.sin(math.pi / 2 * i / 4)), height * (0.28 + 0.1 * i / 4)) for i in range(1, 5)],
            [(0.0, height * 0.38), (0.0, height * 0.78)],
            [(0.0, height * 0.78), (0.014, height * 0.82), (0.014, height)]]
    B.sweep(path, prof, mat, var=var)
    B.fan([(-w - 0.014, depth, height), (-w * 0.72 - 0.01, -0.014, height), (w * 0.72 + 0.01, -0.014, height), (w + 0.014, depth, height)], mat, (0, 0, 1), var=var)


# ---------------------------------------------------------------------------------------------
# Bronze door leaf
# ---------------------------------------------------------------------------------------------
def bronze_leaf(B, width, height, bronze, rust, seed=5):
    """One leaf of the main door, front in the plane y=0 looking towards -y, from x=0 to x=width,
    on z=0. Studded border, a lower panel and a tall upper panel with an ogee head, both sunk and
    filled with rosettes in a lattice of rings, as on the photos of the bronze door."""
    rng = random.Random(seed)
    t, sink = 0.07, 0.014
    bw = 0.105                                           # border band
    fx0, fx1 = bw, width - bw
    lower = (bw, bw + height * 0.27)
    upper = (lower[1] + 0.12, height - bw)
    cx, hw = (fx0 + fx1) / 2, (fx1 - fx0) / 2
    zs = upper[1] - hw * 0.62                            # where the ogee head starts

    def on_face(x, z, sunk=False):
        return translate(x, sink if sunk else 0.0, z) @ FACE_OUT

    B.box(0.0, width, sink, t, 0.0, height, bronze, var=0.45, faces="xXyzZ")       # slab; its front is the sunk field
    for x0, x1, z0, z1 in ((0.0, bw, 0.0, height), (fx1, width, 0.0, height), (fx0, fx1, 0.0, lower[0]),
                           (fx0, fx1, lower[1], upper[0]), (fx0, fx1, upper[1], height)):
        B.box(x0, x1, 0.0, sink, z0, z1, bronze, var=0.5, faces="xXyzZ")           # raised stiles and rails
    head = [(cx - hw + hw * i / 16, zs + hw * 0.62 * (0.5 - 0.5 * math.cos(math.pi * i / 16)) ** 0.75) for i in range(17)]
    for sx in (-1, 1):                                    # raised corners outside the ogee head
        B.fan([(cx + sx * (x - cx), 0.0, z) for x, z in head], bronze, (0, -1, 0), var=0.5, hub=(cx - sx * hw, 0.0, upper[1]))

    def border_piece(xx, zz, flower):
        B.push(on_face(xx, zz))
        if flower:
            rosette(B, rust, 0.03, 0.016, petals=6, var=rng.uniform(0.3, 0.7), layers=1)
        else:
            stud(B, rust, 0.024, 0.014, var=rng.uniform(0.3, 0.7))
        B.pop()

    n = int((height - 0.1) / 0.105)
    for i in range(n + 1):
        for xx in (bw / 2, width - bw / 2):
            border_piece(xx, 0.05 + (height - 0.1) * i / n, i % 2 == 0)
    m = int((width - 2 * bw) / 0.105)
    for i in range(1, m):
        for zz in (bw / 2, height - bw / 2, (lower[1] + upper[0]) / 2):
            border_piece(bw + (width - 2 * bw) * i / m, zz, i % 2 == 0)

    # moulding around each sunk panel: u runs into the panel, v out of the door
    mould = [[(0.0, 0.0), (0.0, 0.016)], [(0.0, 0.016)] + [(0.022 * i / 5, 0.016 + 0.008 * math.sin(math.pi * i / 5)) for i in range(1, 6)],
             [(0.022, 0.016), (0.03, 0.004), (0.03, -sink)]]
    B.sweep([(fx0, 0.0, lower[0]), (fx0, 0.0, lower[1]), (fx1, 0.0, lower[1]), (fx1, 0.0, lower[0])], mould, bronze,
            b=(0, -1, 0), closed=True, var=0.5, flip=True)
    full = head + [(2 * cx - x, z) for x, z in reversed(head[:-1])]
    B.sweep([(fx0, 0.0, upper[0])] + [(x, 0.0, z) for x, z in full] + [(fx1, 0.0, upper[0])], mould, bronze,
            b=(0, -1, 0), closed=True, var=0.5, flip=True)

    def lattice(z0, z1):
        cols = max(2, round((fx1 - fx0 - 0.08) / 0.27))
        rows = max(2, round((z1 - z0 - 0.08) / 0.27))
        dx, dz = (fx1 - fx0 - 0.08) / cols, (z1 - z0 - 0.08) / rows
        rr = min(dx, dz) * 0.5
        for i in range(cols):
            for j in range(rows):
                px, pz = fx0 + 0.04 + (i + 0.5) * dx, z0 + 0.04 + (j + 0.5) * dz
                ring = [(px + rr * math.cos(a), sink - 0.005, pz + rr * math.sin(a)) for a in np.radians(np.linspace(0, 360, 33)[:-1])]
                B.tube(ring, 0.011, bronze, sides=6, closed=True, var=0.5, flat=0.55)
                B.push(on_face(px, pz, sunk=True) @ rot_z(rng.uniform(0, 45)))
                rosette(B, rust, rr * 0.62, 0.034, petals=8, var=rng.uniform(0.25, 0.75))
                B.pop()
        for i in range(cols + 1):
            for j in range(rows + 1):
                B.push(on_face(fx0 + 0.04 + i * dx, z0 + 0.04 + j * dz, sunk=True))
                stud(B, rust, 0.03, 0.02, var=rng.uniform(0.3, 0.7))
                B.pop()

    lattice(lower[0], lower[1])
    lattice(upper[0], zs)
    B.push(on_face(cx, zs + (upper[1] - zs) * 0.42, sunk=True))                     # cartouche in the ogee head
    rosette(B, rust, 0.11, 0.04, petals=10, var=0.5)
    B.pop()
    for sx in (-1, 1):
        B.push(on_face(cx + sx * 0.2, zs + (upper[1] - zs) * 0.3, sunk=True))
        rosette(B, rust, 0.05, 0.022, petals=6, var=0.5, layers=1)
        B.pop()


# ---------------------------------------------------------------------------------------------
# Parts for the upper front: round window, openings, pediments, turned pieces, balustrades
# ---------------------------------------------------------------------------------------------
def half_round(u0, u1, v, n=8):
    """Profile points of a half-round bead from (u0, v) over to (u1, v), bulging towards +v."""
    c, r = (u0 + u1) / 2, abs(u0 - u1) / 2
    sgn = 1.0 if u0 > u1 else -1.0
    return [(c + sgn * r * math.cos(math.pi * i / n), v + r * math.sin(math.pi * i / n)) for i in range(n + 1)]


def ring(B, mat, cx, cz, radius, runs, segs=96, var=0.5, smooth=False):
    """Moulding around a full circle in the wall plane, centre (cx, cz). Profile u is added to the
    radius, v points out of the wall; list each run from the outer edge inwards."""
    ang = np.radians(np.linspace(0.0, 360.0, segs + 1))
    er = np.stack([np.cos(ang), np.zeros(len(ang)), np.sin(ang)], axis=1)
    off = (B.rng.random(), B.rng.random())
    for run in runs:
        run = np.asarray(run, dtype=np.float64)
        G = np.empty((len(ang), len(run), 3))
        for k, (u, v) in enumerate(run):
            G[:, k] = np.array([cx, -v, cz]) + er * (radius + u)
        du, dv = run[1] - run[0]
        want = dv * er[0] + du * np.array([0.0, 1.0, 0.0])
        first = np.cross(G[1, 0] - G[0, 0], G[0, 1] - G[0, 0])
        plen = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(run, axis=0), axis=1))])
        B.grid(G, mat, reverse=bool(np.dot(first, want) < 0), smooth=smooth, var=var, su=ang * (radius + 0.5), sv=plen, uv_off=off)


def rose_window(B, cx, cz, r_glass, width, stone, glass, iron, recess=0.45, bosses=24):
    """Round window: glass behind an iron grid, a deep reveal and a carved ring `width` wide around it.
    Returns the square hole to leave in the wall behind it."""
    W = width
    runs = [[(W, 0.0), (W, 0.06), (W - 0.05, 0.06)],
            half_round(W - 0.05, W - 0.25, 0.06),
            [(W - 0.25, 0.06), (W - 0.25, 0.03), (0.47, 0.03)],
            [(0.47 - 0.17 * i / 6, 0.03 + 0.13 * (0.5 - 0.5 * math.cos(math.pi * i / 6))) for i in range(7)],
            [(0.30, 0.16), (0.24, 0.16), (0.24, 0.20)],
            half_round(0.24, 0.0, 0.20),
            [(0.0, 0.20), (0.0, -recess)]]
    ring(B, stone, cx, cz, r_glass, runs, segs=128)
    B.fan([(cx + r_glass * 1.02 * math.cos(a), recess, cz + r_glass * 1.02 * math.sin(a)) for a in np.radians(np.linspace(0, 360, 65)[:-1])], glass, (0, -1, 0))
    for k in range(-4, 5):                                # iron grid in front of the glass
        o = k * r_glass / 4.6
        half = math.sqrt(r_glass ** 2 - o ** 2)
        B.box(cx + o - 0.025, cx + o + 0.025, recess - 0.07, recess - 0.02, cz - half, cz + half, iron, faces="xXy")
        B.box(cx - half, cx + half, recess - 0.10, recess - 0.05, cz + o - 0.025, cz + o + 0.025, iron, faces="yzZ")
    R = r_glass + (0.47 + W - 0.25) / 2                    # flowers and beads on the flat band of the ring
    for i in range(bosses * 2):
        a = math.pi * i / bosses
        B.push(translate(cx + R * math.cos(a), -0.03, cz + R * math.sin(a)) @ FACE_OUT @ rot_z(math.degrees(a)))
        if i % 2 == 0:
            rosette(B, stone, 0.19, 0.07, petals=8)
        else:
            rosette(B, stone, 0.07, 0.04, petals=6, layers=1)
        B.pop()
    return (cx - r_glass, cx + r_glass, cz - r_glass, cz + r_glass)


def opening(B, x0, x1, z0, z_spring, stone, back_mat, arched=True, recess=0.3, frame_w=0.14, proud=0.05, var=0.5):
    """Window or arcade opening in the wall plane with a moulded frame, a reveal and a back plate of
    `back_mat`. Round-headed above z_spring when arched. Returns the hole to leave in the wall."""
    r, cx = (x1 - x0) / 2, (x0 + x1) / 2
    if arched:
        outline = [(x0, z0)] + [(cx + r * math.cos(a), z_spring + r * math.sin(a)) for a in np.radians(np.linspace(180, 0, 25))] + [(x1, z0)]
        top = z_spring + r
    else:
        outline = [(x0, z0), (x0, z_spring), (x1, z_spring), (x1, z0)]
        top = z_spring
    path = [(x, 0.0, z) for x, z in outline]
    B.sweep(path, frame_profile(frame_w, proud), stone, b=(0, -1, 0), side=-1, var=var)
    B.sweep(path, [[(0.0, 0.0), (0.0, -recess)]], stone, b=(0, -1, 0), side=-1, var=var)
    B.fan([(x, recess, z) for x, z in outline], back_mat, (0, -1, 0), hub=(cx, recess, z0))
    B.quad((x0, 0.0, z0), (x1, 0.0, z0), (x1, recess, z0), (x0, recess, z0), stone, want=(0, 0, 1), var=var)
    if arched:
        for sx in (-1, 1):
            pts = [(cx + sx * r * math.cos(a), 0.0, z_spring + r * math.sin(a)) for a in np.radians(np.linspace(0, 90, 13))]
            B.fan(pts, stone, (0, -1, 0), var=var, hub=(cx + sx * r, 0.0, top))
    return (x0, x1, z0, top)


def raking_profile(s=1.0):
    """Cornice of a pediment or string course swept in the wall plane: u = up, v = out of the wall;
    listed from the top at the wall, over the front, back to the wall underneath."""
    p = [[(0.30, 0.0), (0.30, 0.34)], [(0.30, 0.34), (0.24, 0.34)],
         [(0.24 - 0.10 * i / 6, 0.34 - 0.12 * (0.5 - 0.5 * math.cos(math.pi * i / 6))) for i in range(7)],
         [(0.14, 0.22), (0.12, 0.22), (0.12, 0.14)],
         [(0.12, 0.14), (0.04, 0.10), (0.0, 0.10), (0.0, 0.0)]]
    return [[(u * s, v * s) for u, v in run] for run in p]


def course(B, mat, pts, s=1.0, var=0.5, caps=True):
    """Cornice with raking_profile along (x, z) points in the wall plane, listed left to right; flat ends."""
    prof = raking_profile(s)
    B.sweep([(x, 0.0, z) for x, z in pts], prof, mat, b=(0, -1, 0), side=-1, var=var)
    if caps:
        chain = [p for run in prof for p in run]
        for (xe, ze), (xn, zn) in ((pts[0], pts[1]), (pts[-1], pts[-2])):
            d = _unit((xe - xn, 0.0, ze - zn))                       # pointing out of the end
            up = np.array([-d[2], 0.0, d[0]])                        # in the wall plane, square to the run
            if up[2] < 0:
                up = -up
            end = [np.array([xe, 0.0, ze]) + up * u + np.array([0.0, -v, 0.0]) for u, v in chain]
            B.fan(end, mat, d, var=var, hub=np.array([xe, 0.0, ze]) + up * 0.15 * s)       # hub on the wall: the fan closes the end fully


URN = [(0.16, 0.0), (0.16, 0.05), (0.10, 0.08), (0.07, 0.16), (0.10, 0.22), (0.22, 0.36), (0.26, 0.50), (0.24, 0.62), (0.14, 0.70),
       (0.11, 0.76), (0.15, 0.80), (0.15, 0.84), (0.08, 0.90), (0.05, 1.0), (0.07, 1.06), (0.001, 1.14)]
BALUSTER = [(0.058, 0.0), (0.058, 0.035), (0.040, 0.05), (0.034, 0.09), (0.052, 0.16), (0.074, 0.25), (0.068, 0.34), (0.040, 0.46),
            (0.032, 0.52), (0.046, 0.55), (0.046, 0.58), (0.034, 0.60), (0.058, 0.62), (0.058, 0.66)]


def urn(B, mat, s=1.0, var=0.5):
    """Lidded urn standing on z=0, 1.14 * s tall."""
    B.lathe([[(r * s, z * s) for r, z in URN]], mat, segs=24, var=var)


def ball(B, mat, r, var=0.5):
    """Ball finial sitting on z=0."""
    B.lathe([[(max(r * math.cos(a), 0.001), r + r * math.sin(a)) for a in np.radians(np.linspace(-90, 90, 13))]], mat, segs=20, var=var)


def obelisk(B, mat, height, half=0.2, var=0.5):
    """Four-sided pinnacle on a small base, standing on z=0, with a ball on top."""
    r = half * math.sqrt(2)
    B.lathe([[(r, 0.0), (r, 0.10), (r * 0.74, 0.14), (0.045, height - 0.1), (0.001, height - 0.05)]], mat, segs=4, a0=45.0, a1=405.0, smooth=False, var=var)
    B.push(translate(0, 0, height - 0.12))
    ball(B, mat, 0.075, var=var)
    B.pop()


def baluster(B, mat, height, var=0.5):
    k = height / 0.66
    B.lathe([[(r * min(k, 1.15), z * k) for r, z in BALUSTER]], mat, segs=12, var=var)


def balustrade(B, mat, p0, p1, z0, h=0.9, pitch=0.21, posts=(True, True), var=0.5):
    """Level balustrade from p0 to p1 (x, y): plinth rail, turned balusters, handrail and end posts with balls."""
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    B.push(translate(p0[0], p0[1], z0) @ rot_z(math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0]))))
    B.box(0.0, L, -0.08, 0.08, 0.0, 0.09, mat, var=var, faces="yYZ")
    B.box(0.0, L, -0.10, 0.10, h - 0.10, h, mat, var=var, faces="yYzZ")
    a, b = (0.11 if posts[0] else 0.0), (L - 0.11 if posts[1] else L)
    for use, xc in zip(posts, (0.0, L)):
        if use:
            B.box(xc - 0.11, xc + 0.11, -0.11, 0.11, 0.0, h + 0.05, mat, var=var, faces="xXyYZ")
            B.push(translate(xc, 0.0, h + 0.05))
            ball(B, mat, 0.09, var=var)
            B.pop()
    n = max(1, int(round((b - a) / pitch)))
    for i in range(n):
        B.push(translate(a + (i + 0.5) * (b - a) / n, 0.0, 0.09))
        baluster(B, mat, h - 0.19, var=var)
        B.pop()
    B.pop()


def scroll(B, mat, c0, r0, c1, r1, sx=1, y=-0.12, width=0.22, thick=0.07, var=0.5):
    """S-shaped console scroll in the wall plane: a large volute around c0 = (x, z), a small one
    around c1, joined by a band. Drawn for the right of a centre line; sx = -1 mirrors it about the x of c0."""
    def volute(c, R, turns, a_end):                      # from the eye outwards, anticlockwise, ending at a_end
        n = int(36 * turns)
        return [(c[0] + R * (0.1 + 0.9 * (i / n) ** 1.3) * math.cos(a_end - (1 - i / n) * turns * 2 * math.pi),
                 c[1] + R * (0.1 + 0.9 * (i / n) ** 1.3) * math.sin(a_end - (1 - i / n) * turns * 2 * math.pi)) for i in range(n + 1)]
    c1r = (c0[0] + sx * (c1[0] - c0[0]), c1[1])           # work on the right-hand side, mirror at the end
    A = volute(c0, r0, 2.0, math.pi / 2)                  # ends on top of the big volute, heading left
    Bv = volute(c1r, r1, 1.75, -math.pi / 2)              # ends under the small volute, heading right
    p0, p3 = np.array(A[-1]), np.array(Bv[-1])
    k = 0.45 * np.linalg.norm(p3 - p0)
    q1, q2 = p0 + np.array([-k, 0.0]), p3 + np.array([k, 0.0])
    mid = [tuple((1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * q1 + 3 * (1 - t) * t ** 2 * q2 + t ** 3 * p3) for t in np.linspace(0, 1, 24)[1:-1]]
    pts = A + mid + Bv[::-1]
    B.tube([(c0[0] + sx * (x - c0[0]), y, z) for x, z in pts], width / 2, mat, sides=10, var=var, flat=thick / width)


# ---------------------------------------------------------------------------------------------
# Towers: polygon plans, corner strips, openings that go right through a wall, clock, bells, dome
# ---------------------------------------------------------------------------------------------
T8 = math.tan(math.radians(22.5))


def corner_post(B, mat, c, a, na, b, nb, z0, z1, leg=0.16, proud=0.02, var=0.5):
    """Raised strip wrapping an outside corner of two wall faces, so their chamfered end blocks cannot
    leave a slit. c: the corner (x, y); a, b: unit vectors along each face away from the corner;
    na, nb: the faces' outward normals."""
    c, a, na, b, nb = (np.array(v, dtype=np.float64) for v in (c, a, na, b, nb))
    s, _t = np.linalg.solve(np.array([[a[0], -b[0]], [a[1], -b[1]]]), proud * (nb - na))
    pts = [c + a * leg, c + a * leg + na * proud, c + a * s + na * proud, c + b * leg + nb * proud, c + b * leg]
    for (p, q), w in zip(zip(pts, pts[1:]), (a, na, nb, b)):
        B.quad((p[0], p[1], z0), (q[0], q[1], z0), (q[0], q[1], z1), (p[0], p[1], z1), mat, want=(w[0], w[1], 0.0), var=var)


def face_matrix(c, apothem, theta):
    """Wall coordinates for the face of a tower whose outward normal points at `theta` degrees (0 = +x,
    90 = +y): x runs along the face to the viewer's right, y into the wall, the face is y = 0."""
    a = math.radians(theta)
    return translate(c[0] + apothem * math.cos(a), c[1] + apothem * math.sin(a), 0.0) @ rot_z(theta + 90.0)


def polygon_corner(c, apothem, theta):
    """(x, y) of the corner at the anticlockwise end of the face with normal angle `theta` of an octagon."""
    a = math.radians(theta + 22.5)
    r = apothem / math.cos(math.radians(22.5))
    return (c[0] + r * math.cos(a), c[1] + r * math.sin(a))


def octagon_post(B, mat, c, apothem, theta, z0, z1, leg=0.16, proud=0.02):
    """corner_post for the corner at the anticlockwise end of the octagon face with normal angle `theta`."""
    a0, a1 = math.radians(theta), math.radians(theta + 45.0)
    corner_post(B, mat, polygon_corner(c, apothem, theta), (math.sin(a0), -math.cos(a0)), (math.cos(a0), math.sin(a0)),
                (-math.sin(a1), math.cos(a1)), (math.cos(a1), math.sin(a1)), z0, z1, leg=leg, proud=proud)


def bar(B, mat, p0, p1, width, y_back, y_front, var=0.5):
    """Flat bar lying on the wall plane between (x, z) points p0 and p1: front face and four sides."""
    p0, p1 = np.array(p0, dtype=np.float64), np.array(p1, dtype=np.float64)
    d = _unit(p1 - p0)
    n = np.array([-d[1], d[0]]) * width / 2
    c = [p0 - n, p1 - n, p1 + n, p0 + n]
    F = [(x, y_front, z) for x, z in c]
    K = [(x, y_back, z) for x, z in c]
    B.quad(F[0], F[1], F[2], F[3], mat, want=(0, -1, 0), var=var)
    mid = (p0 + p1) / 2
    for i in range(4):
        j = (i + 1) % 4
        e = (c[i] + c[j]) / 2 - mid
        B.quad(F[i], F[j], K[j], K[i], mat, want=(e[0], 0.0, e[1]), var=var)


ROMAN = {1: "I", 2: "II", 3: "III", 4: "IIII", 5: "V", 6: "VI", 7: "VII", 8: "VIII", 9: "IX", 10: "X", 11: "XI", 12: "XII"}


def clock_face(B, cx, cz, r, stone, dial, iron, hour=10.0, minute=9.0):
    """Tower clock on the wall plane: square moulded stone frame, round dial inside a moulded ring,
    Roman numerals standing radially and two hands. Returns the square hole to leave in the wall."""
    s = r + 0.30
    sq = [(cx - s, 0.0, cz - s), (cx - s, 0.0, cz + s), (cx + s, 0.0, cz + s), (cx + s, 0.0, cz - s)]
    B.sweep(sq, frame_profile(0.2, 0.075), stone, b=(0, -1, 0), side=-1, closed=True)
    B.quad(sq[0], sq[3], sq[2], sq[1], stone, want=(0, -1, 0))                    # the square field behind the dial
    ang = np.radians(np.linspace(0.0, 360.0, 97)[:-1])
    B.fan([(cx + r * math.cos(a), -0.012, cz + r * math.sin(a)) for a in ang], dial, (0, -1, 0))
    ring(B, stone, cx, cz, r, [[(0.13, 0.0), (0.13, 0.05), (0.10, 0.05)], half_round(0.10, 0.03, 0.05), [(0.03, 0.05), (0.0, 0.05), (0.0, 0.012)]], segs=96)
    ring(B, iron, cx, cz, r * 0.56, [[(0.012, 0.012), (0.012, 0.022), (-0.012, 0.022), (-0.012, 0.012)]], segs=72)   # inner chapter ring
    ring(B, iron, cx, cz, r * 0.96, [[(0.012, 0.012), (0.012, 0.022), (-0.012, 0.022), (-0.012, 0.012)]], segs=96)   # outer chapter ring
    h = r * 0.30                                           # numeral height
    w, adv = h * 0.105, {"I": h * 0.30, "V": h * 0.60, "X": h * 0.62}
    for hr, text in ROMAN.items():
        a = math.radians(90.0 - 30.0 * hr)
        out = np.array([math.cos(a), math.sin(a)])         # numerals stand with their feet towards the centre
        right = np.array([math.sin(a), -math.cos(a)])
        base = np.array([cx, cz]) + out * (r * 0.61)
        x = -sum(adv[ch] for ch in text) / 2
        for ch in text:
            o = base + right * (x + adv[ch] / 2)
            g = adv[ch] * 0.36
            if ch == "I":
                strokes = [((0.0, 0.0), (0.0, h))]
            elif ch == "V":
                strokes = [((-g, h), (0.0, 0.0)), ((g, h), (0.0, 0.0))]
            else:
                strokes = [((-g, 0.0), (g, h)), ((-g, h), (g, 0.0))]
            for (u0, v0), (u1, v1) in strokes:
                bar(B, iron, o + right * u0 + out * v0, o + right * u1 + out * v1, w, -0.012, -0.03)
            x += adv[ch]
        for v in (0.0, h):                                 # serifs: a fine bar under and over each numeral
            half = sum(adv[ch] for ch in text) / 2
            bar(B, iron, base + out * v - right * half, base + out * v + right * half, w * 0.55, -0.012, -0.028)
    for k in range(60):                                    # minute ticks
        a = math.radians(6.0 * k)
        e = np.array([math.cos(a), math.sin(a)])
        bar(B, iron, np.array([cx, cz]) + e * r * 0.915, np.array([cx, cz]) + e * r * 0.95, 0.012 if k % 5 else 0.028, -0.012, -0.024)
    for value, length, width in (((hour % 12) + minute / 60.0, r * 0.5, 0.075), (minute / 5.0, r * 0.82, 0.05)):
        a = math.radians(90.0 - 30.0 * value)
        e = np.array([math.cos(a), math.sin(a)])
        bar(B, iron, np.array([cx, cz]) - e * r * 0.14, np.array([cx, cz]) + e * length, width, -0.03, -0.05)
    B.push(translate(cx, -0.05, cz) @ FACE_OUT)
    rosette(B, iron, 0.09, 0.03, petals=8, layers=1)
    B.pop()
    return (cx - s, cx + s, cz - s, cz + s)


def glazed_window(B, x0, x1, z0, z_spring, stone, glass, iron, recess=0.42, frame_w=0.18, proud=0.045, pane=(0.3, 0.42), sill=True):
    """Round-headed window with small panes: moulded frame, deep reveal, glass and a grid of iron
    glazing bars in front of it, and a moulded sill. Returns the hole to leave in the wall."""
    hole = opening(B, x0, x1, z0, z_spring, stone, glass, arched=True, recess=recess, frame_w=frame_w, proud=proud)
    cx, r = (x0 + x1) / 2, (x1 - x0) / 2
    yb = recess - 0.035
    n = max(1, int(round((x1 - x0) / pane[0])))
    for i in range(1, n):
        x = x0 + (x1 - x0) * i / n
        top = z_spring + math.sqrt(max(r * r - (x - cx) ** 2, 0.0))
        B.box(x - 0.014, x + 0.014, yb - 0.03, yb, z0, top, iron, faces="xXy")
    z = z0 + pane[1]
    while z < z_spring + r - 0.12:
        half = r if z <= z_spring else math.sqrt(max(r * r - (z - z_spring) ** 2, 0.0))
        B.box(cx - half, cx + half, yb - 0.04, yb - 0.01, z - 0.014, z + 0.014, iron, faces="yzZ")
        z += pane[1]
    if sill:
        sw = r + frame_w + 0.06
        prof = [[(0.0, -0.16), (0.04, -0.16), (0.04, -0.11)],
                [(0.04, -0.11)] + [(0.04 + 0.05 * (1 - math.cos(a)), -0.11 + 0.06 * math.sin(a)) for a in QUARTER],
                [(0.09, -0.05), (0.11, -0.05), (0.11, 0.0), (0.0, 0.0)]]
        B.sweep([(cx - sw, 0.0, z0), (cx - sw, -0.001, z0), (cx + sw, -0.001, z0), (cx + sw, 0.0, z0)], prof, stone)
    return hole


def arch_outline(x0, x1, z0, z_spring, n=24):
    r, cx = (x1 - x0) / 2, (x0 + x1) / 2
    return [(x0, z0)] + [(cx + r * math.cos(a), z_spring + r * math.sin(a)) for a in np.radians(np.linspace(180, 0, n + 1))] + [(x1, z0)]


def arch_through(B, x0, x1, z0, z_spring, depth, stone, frame_w=0.16, proud=0.05, var=0.5):
    """Round-headed opening that goes right through a wall `depth` thick: moulded frame on the outer
    face, the reveal all the way through and the floor of the opening. Returns the hole to leave in the wall."""
    r, cx = (x1 - x0) / 2, (x0 + x1) / 2
    outline = arch_outline(x0, x1, z0, z_spring)
    path = [(x, 0.0, z) for x, z in outline]
    B.sweep(path, frame_profile(frame_w, proud), stone, b=(0, -1, 0), side=-1, var=var)
    B.sweep(path, [[(0.0, 0.0), (0.0, -depth)]], stone, b=(0, -1, 0), side=-1, var=var)
    B.quad((x0, 0.0, z0), (x1, 0.0, z0), (x1, depth, z0), (x0, depth, z0), stone, want=(0, 0, 1), var=var)
    for sx in (-1, 1):
        pts = [(cx + sx * r * math.cos(a), 0.0, z_spring + r * math.sin(a)) for a in np.radians(np.linspace(0, 90, 13))]
        B.fan(pts, stone, (0, -1, 0), var=var, hub=(cx + sx * r, 0.0, z_spring + r))
    return (x0, x1, z0, z_spring + r)


def pierced_panel(B, x0, x1, z0, z1, hole, mat, y, var=0.4):
    """Plain wall seen from inside (it faces +y, away from the outer face) with one round-headed hole
    (hx0, hx1, hz0, spring)."""
    hx0, hx1, hz0, spring = hole
    r, cx = (hx1 - hx0) / 2, (hx0 + hx1) / 2
    top = spring + r
    for a, b, c, d in ((x0, hx0, z0, z1), (hx1, x1, z0, z1), (hx0, hx1, top, z1), (hx0, hx1, z0, hz0)):
        if b - a > 1e-6 and d - c > 1e-6:
            B.quad((a, y, c), (b, y, c), (b, y, d), (a, y, d), mat, want=(0, 1, 0), var=var)
    for sx in (-1, 1):
        pts = [(cx + sx * r * math.cos(a), y, spring + r * math.sin(a)) for a in np.radians(np.linspace(0, 90, 13))]
        B.fan(pts, mat, (0, 1, 0), var=var, hub=(cx + sx * r, y, top))


def bell(B, bronze, iron, r=0.5, h=0.85):
    """Church bell hanging from z = 0 downwards (its crown is at the origin), with a clapper."""
    out = [(r, 0.0), (r * 0.985, h * 0.03), (r * 0.80, h * 0.17), (r * 0.66, h * 0.36), (r * 0.58, h * 0.58), (r * 0.55, h * 0.78),
           (r * 0.50, h * 0.88), (r * 0.36, h * 0.96), (r * 0.16, h), (0.001, h * 1.01)]
    inn = [(0.001, h * 0.90), (r * 0.44, h * 0.82), (r * 0.50, h * 0.55), (r * 0.60, h * 0.30), (r * 0.76, h * 0.12), (r * 0.92, 0.0)]
    B.push(translate(0.0, 0.0, -h * 1.01))
    B.lathe([out], bronze, segs=32, var=0.5)
    B.lathe([inn], bronze, segs=32, var=0.3)
    B.lathe([[(r * 0.92, 0.0), (r, 0.0)]], bronze, segs=32, var=0.4)
    for zz in (h * 0.10, h * 0.14, h * 0.80):               # moulding wires
        rr = float(np.interp(zz, [p[1] for p in out], [p[0] for p in out]))
        B.tube([((rr + 0.004) * math.cos(a), (rr + 0.004) * math.sin(a), zz) for a in np.radians(np.linspace(0, 360, 33)[:-1])], 0.012, bronze, sides=6, closed=True)
    B.tube([(0.0, 0.0, h * 0.88), (0.0, 0.0, h * 0.16)], 0.018, iron, sides=6)
    B.push(translate(0.0, 0.0, h * 0.02))
    ball(B, iron, r * 0.13)
    B.pop()
    B.pop()


def stepped_dome(B, mat, r0, r1, height, courses=14, ledge=0.035, segs=72, bulge=0.9, var=0.5):
    """Dome of stone courses on z = 0: each course is a ring with a small ledge, so the surface reads
    as cut stone and not as a smooth shell. Radius r0 at the foot, r1 at the top."""
    def rad(t):
        return r1 + (r0 - r1) * max(math.cos(t * math.pi / 2), 0.0) ** bulge
    runs = []
    for i in range(courses):
        t0, t1 = i / courses, (i + 1) / courses
        ra, rb = rad(t0), rad(t1)
        runs.append([(ra, height * t0), (rb + ledge, height * t1), (rb, height * t1)])
    B.lathe(runs, mat, segs=segs, var=var)
    return rad


def ngon_ring(c, apothem, z, n=8, first=-90.0):
    """Corners of a regular polygon as (x, y, z), anticlockwise, starting at the anticlockwise end of
    the face whose normal points at `first` degrees."""
    return [polygon_corner(c, apothem, first + 360.0 / n * k) + (z,) for k in range(n)]


def audit_plane(objs, origin, udir, u0, u1, z0, z1, eyes, step=0.02, cell=0.25):
    """The see-through check of audit_see_through for any upright plane and for several objects at once.
    Grid points are origin + udir * u + (0, 0, z)."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    V, F, n = [], [], 0
    for ob in objs:
        me = ob.data
        co = np.empty(len(me.vertices) * 3, dtype=np.float32)
        me.vertices.foreach_get("co", co)
        V.append(co.reshape(-1, 3))
        for p in me.polygons:
            F.append([v + n for v in p.vertices])
        n += len(me.vertices)
    tree = BVHTree.FromPolygons(np.concatenate(V).tolist(), F, all_triangles=False)
    o3, d3 = Vector(origin), Vector(udir).normalized()
    us, zs = np.arange(u0, u1, step), np.arange(z0, z1, step)
    total = 0
    for name, eye in eyes:
        e = Vector(eye)
        through, example = {}, {}
        for u in us:
            for z in zs:
                d = (o3 + d3 * float(u) + Vector((0.0, 0.0, float(z))) - e).normalized()
                o, trail = e, []
                for _ in range(40):
                    loc, nrm, _idx, _dist = tree.ray_cast(o, d, 400.0)
                    if loc is None or nrm.dot(d) < 0.0:
                        break
                    trail.append(tuple(round(c, 3) for c in loc))
                    o = loc + d * 2e-4
                if loc is None:
                    key = (round(math.floor(u / cell) * cell, 2), round(math.floor(z / cell) * cell, 2))
                    through[key] = through.get(key, 0) + 1
                    example.setdefault(key, ((round(float(u), 3), round(float(z), 3)), trail[:5]))
        k = sum(through.values())
        total += k
        worst = sorted(through.items(), key=lambda kv: -kv[1])[:8]
        print(f"AUDIT eye '{name}': {k} of {len(us) * len(zs)} rays see through; worst {cell} m cells (u, z): {worst}")
        for key, _v in worst[:3]:
            print(f"AUDIT   example towards (u, z) {example[key][0]}: back faces passed at {example[key][1]}")
    return total


# ---------------------------------------------------------------------------------------------
# Gothic parts: pointed arches, tracery windows, moulded portals, buttress pieces, gargoyles
# ---------------------------------------------------------------------------------------------
def wall_frame(p0, p1):
    """Wall coordinates for a wall standing on the plan line p0 -> p1, seen with p0 on the left: local x
    runs from 0 at p0 to the returned length at p1, y into the wall. Returns (matrix, length)."""
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    return translate(p0[0], p0[1], 0.0) @ rot_z(math.degrees(math.atan2(dy, dx))), math.hypot(dx, dy)


def pointed_arch(x0, x1, z_spring, rise, n=16):
    """Two-centred pointed arch over the span x0..x1. Returns (points from the left springing over the
    apex to the right springing, half(z) = half-width of the opening at height z, top(x) = its height at x)."""
    w, cx = x1 - x0, (x0 + x1) / 2
    c = (rise * rise - w * w / 4) / w                 # the centres lie this far beyond the axis
    R = w / 2 + c
    a_apex = math.atan2(rise, -c)
    left = [(cx + c + R * math.cos(t), z_spring + R * math.sin(t)) for t in np.linspace(math.pi, a_apex, n + 1)]
    pts = left + [(2 * cx - x, z) for x, z in reversed(left[:-1])]

    def half(z):
        dz = z - z_spring
        if dz <= 0.0:
            return w / 2
        return 0.0 if dz >= rise else max(math.sqrt(R * R - dz * dz) - c, 0.0)

    def top(x):
        d = min(abs(x - cx), w / 2)
        return z_spring + math.sqrt(max(R * R - (d + c) ** 2, 0.0))
    return pts, half, top


def stair_holes(cx, half, z0, z1, step=0.1):
    """Thin rectangular holes that together follow an arched opening, for block_wall: the blocks then
    end in fine steps along the arch, which the opening's frame has to cover."""
    out, z = [], z0
    while z < z1 - 1e-9:
        zt = min(z + step, z1)
        h = half(z)
        if h > 0.01:
            out.append((cx - h, cx + h, z, zt, "tb"))
        z = zt
    return out


# cross-section of a tracery bar, anticlockwise: u across the bar, v out of the wall
MULLION = [(-0.045, 0.0), (-0.045, -0.1), (0.045, -0.1), (0.045, 0.0), (0.02, 0.045), (-0.02, 0.045), (-0.045, 0.0)]


def _bar_path(B, pts, y, mat, closed=False, section=MULLION, s=1.0):
    B.sweep([(x, y, z) for x, z in pts], [[(u * s, v * s) for u, v in section]], mat, b=(0, -1, 0), side=-1, closed=closed)


def tracery_window(B, x0, x1, z0, z_spring, stone, glass, lead, rise=None, lights=3, recess=0.5, frame_w=0.22, pane=(0.2, 0.3), sill=True):
    """Pointed window with stone tracery: moulded and splayed frame, mullions, a pointed head over every
    light, a circle with a quatrefoil in the main head, and leaded glass behind. Returns the holes to
    leave in the block wall (the frame covers their stepped edge)."""
    w, cx = x1 - x0, (x0 + x1) / 2
    rise = 0.85 * w if rise is None else rise
    pts, half, top = pointed_arch(x0, x1, z_spring, rise)
    outline = [(x0, z0)] + pts + [(x1, z0)]
    fw = frame_w
    prof = [[(fw, 0.0), (fw, 0.055), (fw - 0.05, 0.055)], half_round(fw - 0.05, fw - 0.13, 0.055),
            [(fw - 0.13, 0.055), (fw - 0.13, 0.02), (0.11, 0.02)], [(0.11, 0.02), (0.0, -0.2)], [(0.0, -0.2), (0.0, -recess)]]
    B.sweep([(x, 0.0, z) for x, z in outline], prof, stone, b=(0, -1, 0), side=-1)
    B.quad((x0, -0.02, z0), (x1, -0.02, z0), (x1, recess, z0), (x0, recess, z0), stone, want=(0, 0, 1))
    B.fan([(x, recess, z) for x, z in outline], glass, (0, -1, 0), hub=(cx, recess, z0))
    yl = recess - 0.02                                    # leading: thin bars right in front of the glass
    n = max(1, int(round(w / pane[0])))
    for i in range(1, n):
        x = x0 + w * i / n
        B.box(x - 0.007, x + 0.007, yl - 0.012, yl, z0, top(x), lead, faces="xXy")
    z = z0 + pane[1]
    while z < z_spring + rise - 0.1:
        h = half(z)
        B.box(cx - h, cx + h, yl - 0.016, yl - 0.004, z - 0.007, z + 0.007, lead, faces="yzZ")
        z += pane[1]
    ym = recess - 0.2                                     # the tracery stands here; its bars are 0.1 m deep
    lw = w / lights
    z_sub = z_spring - 0.25 * lw
    for i in range(1, lights):
        xm = x0 + lw * i
        _bar_path(B, [(xm, z0), (xm, z_sub)], ym, stone)
    for i in range(lights):
        sub, _h, _t = pointed_arch(x0 + lw * i, x0 + lw * (i + 1), z_sub, 0.8 * lw, n=8)
        _bar_path(B, sub, ym, stone)
    zc, rc = (z_spring + 0.44 * w, 0.215 * w) if lights >= 3 else (z_spring + 0.40 * w, 0.24 * w)
    circle = [(cx + rc * math.cos(a), zc + rc * math.sin(a)) for a in np.radians(np.linspace(0, 360, 41)[:-1])]
    _bar_path(B, circle, ym, stone, closed=True)
    for k in range(4):                                    # quatrefoil: four small circles inside the big one
        a = math.radians(45 + 90 * k)
        fx, fz, fr = cx + rc * 0.5 * math.cos(a), zc + rc * 0.5 * math.sin(a), rc * 0.43
        foil = [(fx + fr * math.cos(t), fz + fr * math.sin(t)) for t in np.radians(np.linspace(0, 360, 25)[:-1])]
        _bar_path(B, foil, ym + 0.01, stone, closed=True, s=0.7)
    if sill:
        sw = w / 2 + fw + 0.05
        sp = [[(0.0, -0.2), (0.05, -0.2), (0.05, -0.14)],
              [(0.05, -0.14)] + [(0.05 + 0.06 * (1 - math.cos(a)), -0.14 + 0.08 * math.sin(a)) for a in QUARTER],
              [(0.11, -0.06), (0.14, -0.06), (0.14, 0.0), (0.0, 0.0)]]
        B.sweep([(cx - sw, 0.0, z0), (cx - sw, -0.001, z0), (cx + sw, -0.001, z0), (cx + sw, 0.0, z0)], sp, stone)
    # The hole in the wall is 0.12 m larger than the opening all round: the frame's splay lies behind the wall
    # face there, and wall blocks reaching the opening's edge would stand in front of it.
    g = 0.12
    c = (rise * rise - w * w / 4) / w
    _p, half_g, _t = pointed_arch(x0 - g, x1 + g, z_spring, math.sqrt((w / 2 + c + g) ** 2 - c * c))
    return [(x0 - g, x1 + g, z0, z_spring, "t")] + stair_holes(cx, half_g, z_spring, z_spring + rise + 2 * g, step=0.05)


def splay_profile(u_out, depth, orders, roll=0.075, hollow=0.07, hood=0.26, hood_proud=0.07, reveal=0.25, fat=0.0):
    """Cross-section of a deeply moulded Gothic doorway: `orders` steps going in and back, each with a
    roll on its outer corner and a hollow in its inner corner, a hood mould round the outside (hood = 0
    leaves it out) and a plain reveal at the door. u = out from the door opening's edge, v = out of the
    wall (0 = wall face, -depth - reveal = the door). Listed from the outside in; every flat face is a
    run of its own, so the section can be shaded smooth along the arch. fat thickens the rolls."""
    a, b = u_out / orders, depth / orders
    rr, hh = roll + fat, hollow
    runs = []
    if hood > 0.0:
        e = u_out + hood
        runs += [[(e, 0.0), (e, hood_proud)], [(e, hood_proud), (e - 0.06, hood_proud)], half_round(e - 0.06, e - 0.16, hood_proud),
                 [(e - 0.16, hood_proud), (e - 0.16, 0.03)], [(e - 0.16, 0.03), (u_out, 0.03)], [(u_out, 0.03), (u_out, 0.0)]]
    u, v = u_out, 0.0
    for k in range(orders):
        last = k == orders - 1
        nu = 0.0 if last else u - a                        # the nosing of this step
        runs.append([(u - (hh if k else 0.0), v), (nu + rr, v)])                                                # tread, facing the street
        runs.append([(nu + rr * math.cos(t), v + rr * math.sin(t)) for t in np.radians(np.linspace(0, 270, 13))])   # roll
        if last:
            runs.append([(nu, v - rr), (nu, v - b)])                                                            # the last riser is the door jamb
        else:
            runs.append([(nu, v - rr), (nu, v - b + hh)])                                                       # riser, facing the axis
            runs.append([(nu - hh + hh * math.cos(t), v - b + hh + hh * math.sin(t)) for t in np.radians(np.linspace(0, -90, 6))])   # hollow
        u, v = nu, v - b
    runs.append([(0.0, v), (0.0, v - reveal)])
    return runs


def pointed_portal(B, half_in, half_out, depth, z0, z_spring, rise_in, stone, trim, orders=8, reveal=0.25, bands=(), jambs=True):
    """Splayed Gothic doorway centred on x = 0: the moulded orders run up both jambs from z0 and over the
    pointed arch. bands: (z, thickness) of capital bands across the jambs. The wall face is y = 0 and
    the door plane y = depth + reveal. With jambs=False the orders only span the arch, from the springing:
    the jambs are then built separately (portal_jamb) and a band up to z_spring closes the orders from
    below. Returns (points of the inner arch, half(z) of the outer arch for the holes in the wall, top z
    of the hood mould)."""
    u_out = half_out - half_in
    pts, _half, _top = pointed_arch(-half_in, half_in, z_spring, rise_in, n=18)
    apex_i = len(pts) // 2
    prof = splay_profile(u_out, depth, orders, reveal=reveal)
    fat = splay_profile(u_out, depth, orders, reveal=reveal, fat=0.035, hood=0.0)
    flat = [q for run in fat for q in run]
    for sx in (-1, 1):                                    # two halves, so the apex stays a sharp line
        if sx < 0:
            path = ([(-half_in, z0)] if jambs else []) + pts[:apex_i + 1]
            ghost = (None if jambs else (-half_in, 0.0, z_spring - 1.0), (pts[apex_i + 1][0], 0.0, pts[apex_i + 1][1]))
        else:
            path = pts[apex_i:] + ([(half_in, z0)] if jambs else [])
            ghost = ((pts[apex_i - 1][0], 0.0, pts[apex_i - 1][1]), None if jambs else (half_in, 0.0, z_spring - 1.0))
        B.sweep([(x, 0.0, z) for x, z in path], prof, stone, b=(0, -1, 0), side=-1, smooth=True, ghost=ghost)
        for zb, tb in bands:                              # capital bands: the same section with fatter rolls
            seg = [(sx * half_in, 0.0, zb), (sx * half_in, 0.0, zb + tb)]
            B.sweep(seg if sx < 0 else seg[::-1], fat, trim, b=(0, -1, 0), side=-1)
            for zz, up in ((zb, -1), (zb + tb, 1)):       # close the band below and above
                B.fan([(sx * (half_in + u), -v, zz) for u, v in flat], trim, (0, 0, up), hub=(sx * (half_out + 0.1), depth, zz))
    # the outer edge of the mouldings, for the holes in the wall: the inner arch pushed out by u_out
    c = (rise_in ** 2 - half_in ** 2) / (2 * half_in)
    rise_out = rise_in + u_out * (half_in + c) / rise_in  # as high as the mitre at the apex reaches
    _p, half_o, _t = pointed_arch(-half_out, half_out, z_spring, rise_out)
    return pts, half_o, z_spring + rise_out


def buttress_cap(B, mat, x_wall, x_front, half_w, z_front, z_back, over=0.07, thick=0.16):
    """Sloping top of a buttress standing against a wall that faces +x here (local frame: x out from the
    wall at x_wall to x_front, y across, centred on y = 0): a weathered slab with a drip edge."""
    xa, xb, hw = x_wall, x_front + over, half_w + over
    top = [(xa, -hw, z_back), (xb, -hw, z_front), (xb, hw, z_front), (xa, hw, z_back)]
    bot = [(x, y, z - thick) for x, y, z in top]
    B.quad(top[0], top[1], top[2], top[3], mat, want=(0.3, 0, 1))
    B.quad(bot[0], bot[1], bot[2], bot[3], mat, want=(0, 0, -1))
    B.quad(top[1], top[2], bot[2], bot[1], mat, want=(1, 0, 0))
    B.quad(top[0], top[1], bot[1], bot[0], mat, want=(0, -1, 0))
    B.quad(top[3], top[2], bot[2], bot[3], mat, want=(0, 1, 0))


# ---------------------------------------------------------------------------------------------
# Romanesque parts for the old tower: corbel tables of small arches, twin windows
# ---------------------------------------------------------------------------------------------
def lombard_band(B, x0, x1, z_top, depth, mat, arch=0.62, var=0.5):
    """Row of small round arches hanging from z_top across x0..x1 (a Lombard band): the wall plane y = 0
    comes down between the arch heads, on little corbels; the sunk field behind lies `depth` back.
    Returns the height of the corbels' undersides: below it the field is open."""
    n = max(2, int(round((x1 - x0) / arch)))
    w = (x1 - x0) / n
    r = w / 2 - 0.045
    z_s = z_top - 0.1 - r
    for i in range(n):
        xa, xb = x0 + i * w, x0 + (i + 1) * w
        cx = (xa + xb) / 2
        arc = [(cx + r * math.cos(a), z_s + r * math.sin(a)) for a in np.radians(np.linspace(180, 0, 13))]
        B.sweep([(x, 0.0, z) for x, z in arc], [[(0.0, 0.0), (0.0, -depth)]], mat, b=(0, -1, 0), side=-1, var=var)     # soffit
        for sx in (-1, 1):
            hub = (cx + sx * w / 2, 0.0, z_s + r)
            pts = [(cx + sx * r * math.cos(a), 0.0, z_s + r * math.sin(a)) for a in np.radians(np.linspace(0, 90, 7))]
            B.fan(pts, mat, (0, -1, 0), var=var, hub=hub)
            B.fan([(cx + sx * r, 0.0, z_s), (cx + sx * w / 2, 0.0, z_s)], mat, (0, -1, 0), var=var, hub=hub)
        B.quad((xa, 0.0, z_s + r), (xb, 0.0, z_s + r), (xb, 0.0, z_top), (xa, 0.0, z_top), mat, want=(0, -1, 0), var=var)
    for i in range(n + 1):                                 # corbels under the feet of the arches
        xc = x0 + i * w
        a, b = max(xc - 0.05, x0), min(xc + 0.05, x1)
        B.box(a, b, -0.025, depth, z_s - 0.12, z_s, mat, var=var, faces="xXyz")
    return z_s - 0.12


def bifora(B, cx, z0, stone, dark, light=0.4, height=1.5, depth=0.3):
    """Romanesque twin window on the wall plane y = 0: two narrow round-headed lights and a colonnette
    standing in front of the pier between them. Returns the holes to leave in the wall."""
    gap = 0.16
    spring = z0 + height - light / 2
    holes = []
    for sx in (-1, 1):
        xa = cx + sx * (gap / 2 + light / 2) - light / 2
        holes.append(opening(B, xa, xa + light, z0, spring, stone, dark, arched=True, recess=depth, frame_w=0.05, proud=0.015))
    B.push(translate(cx, -0.035, z0))
    B.box(-0.075, 0.075, -0.075, 0.075, 0.0, 0.08, stone, faces="xXyYZ")
    h = spring - z0
    B.lathe([[(0.05, 0.08), (0.062, 0.1), (0.05, 0.13), (0.045, h - 0.2), (0.07, h - 0.1), (0.085, h - 0.02)]], stone, segs=12)
    B.box(-0.1, 0.1, -0.09, 0.09, h - 0.02, h + 0.06, stone, faces="xXyYzZ")
    B.pop()
    return holes


# ---------------------------------------------------------------------------------------------
# Gothic roofscape: pinnacles, pierced parapets, tiled roof facets
# ---------------------------------------------------------------------------------------------
def pinnacle(B, mat, half=0.36, shaft=1.7, spire=3.0, crockets=6, var=0.5):
    """Pinnacle standing on z = 0, centred on the origin: square shaft with a small gable on every side,
    a four-sided spire with crockets up its edges and a finial. Returns its height."""
    h = half
    B.box(-h, h, -h, h, 0.0, shaft, mat, var=var, faces="xXyY")
    B.box(-h - 0.05, h + 0.05, -h - 0.05, h + 0.05, 0.0, 0.14, mat, var=var, faces="xXyYZ")
    z0g, ztg, g = shaft - 0.2, shaft + 0.55, 0.05
    for k in range(4):                                     # gablets
        B.push(rot_z(90.0 * k))
        a, b, c = (-h - 0.05, -h - g, z0g), (h + 0.05, -h - g, z0g), (0.0, -h - g, ztg)
        B.fan([a, b, c], mat, (0, -1, 0), var=var)
        B.quad(a, c, (0.0, 0.0, ztg), (-h - 0.05, 0.0, z0g), mat, want=(-1, 0, 1), var=var)
        B.quad(b, c, (0.0, 0.0, ztg), (h + 0.05, 0.0, z0g), mat, want=(1, 0, 1), var=var)
        B.quad(a, b, (h + 0.05, -h, z0g), (-h - 0.05, -h, z0g), mat, want=(0, 0, -1), var=var)
        B.pop()
    hb, apex = h * 0.92, shaft + spire
    corners = [(-hb, -hb), (hb, -hb), (hb, hb), (-hb, hb)]
    for i in range(4):                                     # spire
        (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 4]
        B.fan([(ax, ay, shaft), (bx, by, shaft), (0.0, 0.0, apex)], mat, ((ax + bx) / 2, (ay + by) / 2, 0.3), var=var)
    for cx, cy in corners:                                 # crockets: small hooked leaves up every edge
        d = _unit((cx, cy, 0.0))
        for j in range(1, crockets + 1):
            t = j / (crockets + 1.0)
            p = np.array([cx * (1 - t), cy * (1 - t), shaft + spire * t])
            s = 0.13 * (1.0 - 0.45 * t)
            B.tube([p - d * 0.02, p + d * s * 0.8 + (0, 0, s * 0.3), p + d * s + (0, 0, s * 1.0), p + d * s * 0.5 + (0, 0, s * 1.45)],
                   [s * 0.36, s * 0.42, s * 0.3, s * 0.1], mat, sides=5, var=var)
    B.push(translate(0.0, 0.0, apex - 0.14))               # finial
    B.lathe([[(0.04, 0.0), (0.1, 0.06), (0.13, 0.13), (0.07, 0.21), (0.045, 0.25), (0.09, 0.31), (0.055, 0.39), (0.001, 0.48)]], mat, segs=8, var=var)
    B.pop()
    return apex + 0.34


def pierced_parapet(B, mat, p0, p1, z0, h=1.05, pitch=0.5, thick=0.16, var=0.5):
    """Gothic parapet from p0 to p1 (x, y), standing on z0: base, coping and between them a row of small
    pointed openings, cut right through."""
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
    t = thick / 2
    B.push(translate(p0[0], p0[1], z0) @ rot_z(math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0]))))
    B.box(0.0, L, -t - 0.02, t + 0.02, 0.0, 0.14, mat, var=var, faces="xXyYZ")
    B.box(0.0, L, -t - 0.045, t + 0.045, h - 0.13, h, mat, var=var, faces="xXyYzZ")
    n = max(1, int(round(L / pitch)))
    w = L / n
    zb, zt = 0.14, h - 0.13
    for i in range(n + 1):                                 # mullions
        a, b = max(i * w - 0.06, 0.0), min(i * w + 0.06, L)
        B.box(a, b, -t, t, zb, zt, mat, var=var, faces="xXyY")
    for i in range(n):                                     # pointed heads
        xa, xb = i * w + 0.06, (i + 1) * w - 0.06
        xm, zs = (xa + xb) / 2, zt - (xb - xa) * 0.8
        for y, ny in ((-t, -1), (t, 1)):
            B.fan([(xa, y, zs), (xa, y, zt), (xm, y, zt)], mat, (0, ny, 0), var=var)
            B.fan([(xb, y, zs), (xm, y, zt), (xb, y, zt)], mat, (0, ny, 0), var=var)
        B.quad((xa, -t, zs), (xm, -t, zt), (xm, t, zt), (xa, t, zs), mat, want=(1, 0, -1), var=var)
        B.quad((xb, -t, zs), (xm, -t, zt), (xm, t, zt), (xb, t, zs), mat, want=(-1, 0, -1), var=var)
    B.pop()


def tile_facet(B, mat, a, b, c, d, pitch=0.4, step=0.03, seed=1):
    """Flat roof facet laid in courses. a, b: the eave; d, c: the top edge above a and above b (c == d
    for a triangle). The lower edge of every course stands `step` proud of the one below, so the roof
    reads as rows of tiles, and every course has its own tone."""
    a, b, c, d = (np.array(p, dtype=np.float64) for p in (a, b, c, d))
    rng = random.Random(seed)
    nrm = _unit(np.cross(b - a, (d + c) / 2 - a))
    if nrm[2] < 0:
        nrm = -nrm
    down = _unit((a + b) / 2 - (d + c) / 2)
    n = max(1, int(round(np.linalg.norm((d + c) / 2 - (a + b) / 2) / pitch)))
    up = nrm * step
    for i in range(n):
        t0, t1 = i / n, (i + 1) / n
        l0, r0, l1, r1 = a + (d - a) * t0, b + (c - b) * t0, a + (d - a) * t1, b + (c - b) * t1
        tone = min(1.0, max(0.0, rng.gauss(0.5, 0.11)))
        if np.linalg.norm(l1 - r1) < 1e-6:
            B.fan([l0 + up, r0 + up, l1], mat, nrm, var=tone)
        else:
            B.quad(l0 + up, r0 + up, r1, l1, mat, want=nrm, var=tone)
        B.quad(l0, r0, r0 + up, l0 + up, mat, want=down, var=tone * 0.7)


# ---------------------------------------------------------------------------------------------
# Jambs of a Gothic doorway as on Girona's Portal of the Apostles: a dado of blind tracery, a ledge,
# then a row of niches, each with a carved corbel for a figure and a canopy over it
# ---------------------------------------------------------------------------------------------
def _arc(cx, cz, r, a0, a1, n=10):
    return [(cx + r * math.cos(a), cz + r * math.sin(a)) for a in np.radians(np.linspace(a0, a1, n + 1))]


def crocket(B, mat, p, out, size, var=0.5):
    """Small hooked leaf growing from the point p in the direction `out`, curling upwards."""
    p, d = np.asarray(p, dtype=np.float64), _unit(out)
    up, s = np.array([0.0, 0.0, 1.0]), size
    B.tube([p - d * 0.2 * s, p + d * 0.8 * s + up * 0.3 * s, p + d * s + up * s, p + d * 0.5 * s + up * 1.45 * s],
           [s * 0.36, s * 0.42, s * 0.3, s * 0.1], mat, sides=5, var=var)


def blind_panel(B, cx, w, z0, z1, mat, sunk=0.06, var=0.5):
    """The tracery of one bay of a blind arcade, standing on a field that lies `sunk` behind the wall plane
    y = 0: a trefoiled pointed arch, a crocketed gablet with a finial over it, and two small lights
    behind the gablet. The bay is w wide, centred on cx; the caller makes the field itself."""
    s = 0.42
    yb = sunk - 0.1 * s + 0.004                         # the bars stand on the field
    a, H = w / 2 - 0.085, z1 - z0
    z_s, rise = z0 + 0.60 * H, 0.9 * (w - 0.17)
    pts, _half, _top = pointed_arch(cx - a, cx + a, z_s, rise, n=10)
    apex = z_s + rise
    _bar_path(B, [(cx - a, z0)] + pts + [(cx + a, z0)], yb, mat, s=s)
    f = a * 0.47                                        # the three foils of the head
    for foil in (_arc(cx - a * 0.40, z_s + 0.03, f, 255, 60, 9), _arc(cx + a * 0.40, z_s + 0.03, f, -75, 120, 9),
                 _arc(cx, z_s + rise * 0.50, f * 0.95, -35, 215, 10)):
        _bar_path(B, foil, yb + 0.006, mat, s=0.3)
    ga, feet, yg = apex + 0.50, a + 0.02, yb - 0.012    # the gablet stands a little proud of the arch
    _bar_path(B, [(cx - feet, z_s + 0.04), (cx, ga), (cx + feet, z_s + 0.04)], yg, mat, s=s * 0.9)
    yf = yg - 0.045 * s * 0.9
    for sx in (-1, 1):
        dx, dz = feet, ga - z_s - 0.04
        ln = math.hypot(dx, dz)
        nx, nz = -sx * dz / ln, dx / ln                 # square to the rake, pointing outwards and up
        for t in (0.34, 0.58, 0.82):
            px, pz = cx + sx * feet * (1 - t), z_s + 0.04 + dz * t
            crocket(B, mat, (px + nx * 0.015, yf, pz + nz * 0.015), (nx, -0.3, nz * 0.3), 0.042, var=var)
        zt = z1 - 0.30                                  # the two small lights behind the gablet
        _bar_path(B, [(cx + sx * a, z_s), (cx + sx * a, zt)], yb + 0.004, mat, s=0.34)
        sub, _h, _t = pointed_arch(min(cx, cx + sx * a), max(cx, cx + sx * a), zt, 0.22, n=5)
        _bar_path(B, sub, yb + 0.004, mat, s=0.34)
    B.lathe([[(0.012, 0.0), (0.03, 0.02), (0.036, 0.045), (0.02, 0.07), (0.014, 0.085), (0.026, 0.105), (0.016, 0.135), (0.001, 0.165)]],
            mat, segs=8, center=(cx, yf + 0.008, ga + 0.005), var=var)


def figure_corbel(B, mat, r=0.27, var=0.5):
    """Carved bracket for a statue, growing out of the wall plane y = 0 (the wall is behind it, y > 0), its
    foot at z = 0: a pendant, a bell wrapped in three leaves and a moulded top of radius r. Returns its height."""
    k = r / 0.27

    def r_bell(z):
        t = min(max((z - 0.09 * k) / (0.22 * k), 0.0), 1.0)
        return (0.07 + 0.15 * t ** 1.25) * k

    runs = [[(0.001, -0.05 * k), (0.045 * k, 0.0), (0.07 * k, 0.03 * k), (0.07 * k, 0.09 * k)],
            [(r_bell(0.09 * k + 0.22 * k * i / 8), 0.09 * k + 0.22 * k * i / 8) for i in range(9)],
            [(0.22 * k, 0.31 * k), (0.27 * k, 0.335 * k), (0.27 * k, 0.39 * k)],
            [(0.27 * k, 0.39 * k), (0.25 * k, 0.42 * k), (0.001, 0.42 * k)]]
    B.lathe(runs, mat, segs=14, a0=180.0, a1=360.0, var=var)
    for ang in (215.0, 270.0, 325.0):
        _leaf(B, mat, r_bell, 0.10 * k, 0.19 * k, 0.16 * k, math.radians(ang), curl=0.024 * k, var=var, clear=0.01, nu=8)
    return 0.42 * k


def niche_canopy(B, mat, r=0.29, h=0.8, var=0.5):
    """Canopy over a statue niche: half of an eight-sided tabernacle growing out of the wall plane y = 0, its
    underside at z = 0. Moulded rim, four faces each with a small arch under a gablet, a cornice with a leaf
    on every corner and a low spirelet. Returns its height."""
    k = r / 0.29
    rb = 0.255 * k
    runs = [[(0.001, 0.13 * k), (0.21 * k, 0.0)],                                       # hollow underside
            [(0.21 * k, 0.0), (0.27 * k, 0.0), (0.29 * k, 0.025 * k), (0.29 * k, 0.085 * k)],
            [(0.29 * k, 0.085 * k), (rb, 0.115 * k), (rb, 0.47 * k)],
            [(rb, 0.47 * k), (0.29 * k, 0.50 * k), (0.29 * k, 0.56 * k)],
            [(0.29 * k, 0.56 * k), (0.25 * k, 0.60 * k)],
            [(0.25 * k, 0.60 * k), (0.12 * k, h * 0.93), (0.001, h)]]
    B.lathe(runs, mat, segs=4, a0=180.0, a1=360.0, smooth=False, var=var)
    rf, a2 = rb * math.cos(math.radians(22.5)), 0.075 * k
    for j in range(4):
        th = math.radians(202.5 + 45.0 * j)
        B.push(translate(rf * math.cos(th), rf * math.sin(th), 0.0) @ rot_z(math.degrees(th) + 90.0))
        arch, _h, _t = pointed_arch(-a2, a2, 0.27 * k, 0.11 * k, n=5)
        _bar_path(B, [(-a2, 0.135 * k)] + arch + [(a2, 0.135 * k)], -0.004, mat, s=0.2)
        _bar_path(B, [(-0.098 * k, 0.30 * k), (0.0, 0.455 * k), (0.098 * k, 0.30 * k)], -0.007, mat, s=0.2)
        B.pop()
    for j in range(5):
        ph = math.radians(180.0 + 45.0 * j)
        d = (math.cos(ph), math.sin(ph), 0.0)
        crocket(B, mat, (0.285 * k * d[0], 0.285 * k * d[1], 0.555 * k), d, 0.05 * k, var=var)
    return h


def portal_jamb(B, length, z0, z1, stone, bays=6, base_h=0.34, dado_top=3.5, ledge_h=0.2, canopy_h=0.83, dado_sunk=0.06, niche=0.20):
    """One flat jamb of a Gothic doorway, in wall coordinates: x from 0 to `length`, y into the wall, the
    wall's own plane at y = 0. From z0 up to z1: a moulded base, `bays` panels of blind tracery (up to
    dado_top above z0), a weathered ledge, then a row of niches divided by slender shafts, each with a
    corbel for a figure and a canopy. Both ends are closed, so it can stand between any two walls.
    Returns where figures stand: [(x of the niche axis, z of the corbel's top, clear height under the canopy), ...]."""
    w = length / bays
    zb, zl = z0 + base_h, z0 + dado_top
    zn, zc = zl + ledge_h, z1 - canopy_h
    base = [[(0.10, 0.0), (0.10, 0.2)], [(0.10, 0.2), (0.075, 0.25), (0.04, 0.285)], [(0.04, 0.285), (-dado_sunk, base_h)]]
    ledge = [[(-dado_sunk, 0.0), (0.05, 0.0)], [(0.05, 0.0), (0.09, 0.03), (0.09, 0.075)], [(0.09, 0.075), (-niche, ledge_h)]]
    for prof, z, back in ((base, z0, dado_sunk), (ledge, zl, niche)):
        B.sweep([(0.0, 0.0, z), (length, 0.0, z)], prof, stone)
        chain = [p for run in prof for p in run]
        for xe, nx in ((0.0, -1.0), (length, 1.0)):                # flat ends
            B.fan([(xe, -u, z + v) for u, v in chain] + [(xe, back, z + chain[0][1])], stone, (nx, 0.0, 0.0))
    spots = []
    for i in range(bays):
        xa, xb, cx = i * w, (i + 1) * w, (i + 0.5) * w
        tone = 0.5 + 0.05 * ((i * 7) % 5 - 2)                       # every bay cut from a slightly different stone
        B.quad((xa, dado_sunk, zb - 0.05), (xb, dado_sunk, zb - 0.05), (xb, dado_sunk, zl + 0.02), (xa, dado_sunk, zl + 0.02), stone, want=(0, -1, 0), var=tone)
        blind_panel(B, cx, w, zb, zl, stone, sunk=dado_sunk)
        B.quad((xa, niche, zn - 0.3), (xb, niche, zn - 0.3), (xb, niche, z1), (xa, niche, z1), stone, want=(0, -1, 0), var=tone)
        B.push(translate(cx, niche, zn))
        hc = figure_corbel(B, stone)
        B.pop()
        B.push(translate(cx, niche, zc))
        niche_canopy(B, stone, h=canopy_h)
        B.pop()
        spots.append((cx, zn + hc, zc - zn - hc))
    ys = -0.012                                                     # axis of the shafts
    for i in range(bays + 1):
        x = i * w
        for za, zt, dep in ((zb - 0.05, zl + 0.02, dado_sunk), (zn - 0.3, z1, niche)):
            if i == 0:                                              # the two ends: one face each, looking into the jamb
                B.quad((x, 0.0, za), (x, dep, za), (x, dep, zt), (x, 0.0, zt), stone, want=(1, 0, 0), var=0.45)
            elif i == bays:
                B.quad((x, 0.0, za), (x, dep, za), (x, dep, zt), (x, 0.0, zt), stone, want=(-1, 0, 0), var=0.45)
            else:
                B.box(x - 0.028, x + 0.028, 0.0, dep, za, zt, stone, var=0.45, faces="xXy")
        B.tube([(x, ys, zb - 0.03), (x, ys, z1)], 0.04, stone, sides=10)
        for zr in (zb - 0.03, zl - 0.08, zn + 0.02, zc - 0.10):     # base, band under the ledge, band over it, capital
            B.lathe([[(0.04, 0.0), (0.06, 0.012), (0.06, 0.04), (0.04, 0.052)]], stone, segs=10, center=(x, ys, zr))
    return spots


# ---------------------------------------------------------------------------------------------
# Gargoyle: a carved beast, smooth where the stone is rounded and sharp along jaws, horns, wings and spine
# ---------------------------------------------------------------------------------------------
MIRROR_Y = np.diag([1.0, -1.0, 1.0, 1.0])


def _loft(B, mat, sections, sides=12, var=0.5, caps=(True, True)):
    """Smooth body through elliptical sections [(x, z, half width, half height), ...] along a spine that
    lies in the x-z plane; the width is measured along y."""
    S = np.asarray(sections, dtype=np.float64)
    k = len(S)
    a = np.linspace(0.0, 2 * math.pi, sides + 1)
    loops = np.empty((k, sides + 1, 3))
    for i in range(k):
        t = S[min(i + 1, k - 1), :2] - S[max(i - 1, 0), :2]
        t = t / np.linalg.norm(t)
        n = np.array([-t[1], t[0]])                                 # in the x-z plane, square to the spine
        loops[i, :, 0] = S[i, 0] + n[0] * S[i, 3] * np.sin(a)
        loops[i, :, 1] = S[i, 2] * np.cos(a)
        loops[i, :, 2] = S[i, 1] + n[1] * S[i, 3] * np.sin(a)
    centre = np.array([S[:, 0].mean(), 0.0, S[:, 1].mean()])
    B.grid(loops, mat, want=('from', centre), smooth=True, var=var, su=np.arange(k) * 0.1, sv=np.arange(sides + 1) * 0.05)
    for end, on in ((0, caps[0]), (k - 1, caps[1])):
        if on:
            B.fan(loops[end, :-1], mat, np.array([S[end, 0], 0.0, S[end, 1]]) - centre, var=var)


def _two_sided(B, mat, pts, var=0.5):
    """A thin flat piece seen from both sides (a wing membrane, an ear, a fin): a fan round its first point."""
    v = np.asarray(pts, dtype=np.float64)
    f = [(0, i, i + 1) for i in range(1, len(v) - 1)]
    B.add(v, np.array(f + [t[::-1] for t in f]), mat, var=var)


def _spike(B, mat, p, d, size, var=0.5):
    """A small four-sided point (tooth, claw tip) on p, reaching along d."""
    p, d = np.asarray(p, dtype=np.float64), np.asarray(d, dtype=np.float64)
    a = _unit(np.cross(d, (0.0, 1.0, 0.0)) if abs(_unit(d)[1]) < 0.9 else np.cross(d, (1.0, 0.0, 0.0)))
    b = _unit(np.cross(d, a))
    h = size / 2
    v = np.array([p - a * h - b * h, p + a * h - b * h, p + a * h + b * h, p - a * h + b * h, p + d])
    f = np.array([(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)])
    if np.dot(np.cross(v[1] - v[0], v[4] - v[0]), (v[0] + v[1]) / 2 - (p + d * 0.3)) < 0:
        f = f[:, ::-1]
    B.add(v, f, mat, var=var)


def gargoyle(B, mat, length=1.35, var=0.45):
    """Water spout carved as a winged beast: built along +x from the wall (x = 0), z up. It crouches on a
    moulded block, neck stretched out and jaws open; horns swept back, folded bat wings, clawed legs
    drawn up under the body and a ridge of plates down its back."""
    k = length / 1.35
    B.push(np.diag([k, k, k, 1.0]))
    # the block it grows from
    B.box(-0.02, 0.14, -0.34, 0.34, -0.40, 0.34, mat, var=var, faces="XyYzZ")
    B.box(-0.02, 0.08, -0.29, 0.29, -0.54, -0.40, mat, var=var, faces="XyYz")
    body = [(0.02, 0.00, 0.21, 0.24), (0.16, 0.01, 0.23, 0.25), (0.34, 0.02, 0.21, 0.22), (0.52, 0.04, 0.22, 0.23),
            (0.66, 0.07, 0.17, 0.18), (0.80, 0.08, 0.125, 0.135), (0.93, 0.06, 0.11, 0.12), (1.04, 0.03, 0.105, 0.115)]
    _loft(B, mat, body, sides=14, var=var, caps=(False, False))
    head = [(0.98, 0.03, 0.10, 0.11), (1.06, 0.045, 0.155, 0.15), (1.14, 0.04, 0.15, 0.135), (1.21, 0.015, 0.115, 0.09),
            (1.30, -0.005, 0.09, 0.065), (1.38, -0.02, 0.07, 0.048), (1.44, -0.03, 0.045, 0.03), (1.47, -0.035, 0.012, 0.01)]
    _loft(B, mat, head, sides=14, var=var, caps=(False, True))
    jaw = [(1.04, -0.08, 0.11, 0.05), (1.14, -0.15, 0.105, 0.045), (1.24, -0.20, 0.085, 0.036), (1.33, -0.245, 0.055, 0.026),
           (1.385, -0.265, 0.014, 0.01)]
    _loft(B, mat, jaw, sides=12, var=var, caps=(True, True))
    for i, x in enumerate((0.14, 0.27, 0.40, 0.53, 0.66, 0.79, 0.91)):       # plates down the spine
        zt = float(np.interp(x, [s[0] for s in body], [s[1] + s[3] for s in body]))
        hgt = 0.10 - 0.006 * i
        _two_sided(B, mat, [(x - 0.06, 0.0, zt - 0.03), (x + 0.035, 0.0, zt - 0.03), (x - 0.045, 0.0, zt + hgt)], var=var)
    for side in (None, MIRROR_Y):
        if side is not None:
            B.push(side)
        # head: eye under a heavy brow, horn, ear, teeth
        B.lathe([[(max(0.032 * math.cos(a), 0.001), 0.032 * math.sin(a)) for a in np.radians(np.linspace(-90, 90, 9))]], mat, segs=10,
                center=(1.155, 0.112, 0.085), var=var)
        B.tube([(1.07, 0.09, 0.15), (1.15, 0.132, 0.152), (1.22, 0.085, 0.07)], [0.022, 0.03, 0.02], mat, sides=6, var=var)
        B.tube([(1.07, 0.09, 0.15), (0.99, 0.14, 0.29), (0.88, 0.165, 0.37), (0.76, 0.15, 0.40)], [0.05, 0.04, 0.026, 0.006], mat, sides=8, var=var)
        _two_sided(B, mat, [(1.05, 0.14, 0.05), (1.00, 0.15, 0.14), (0.91, 0.25, 0.13)], var=var)
        for x, y, z in ((1.26, 0.078, -0.068), (1.33, 0.064, -0.066), (1.40, 0.045, -0.06)):
            _spike(B, mat, (x, y, z), (0.0, 0.0, -0.045), 0.026, var=var)
        for x, y, z in ((1.19, 0.085, -0.142), (1.27, 0.062, -0.185), (1.33, 0.04, -0.222)):
            _spike(B, mat, (x, y, z), (0.012, 0.0, 0.042), 0.024, var=var)
        # folded wing: arm, three fingers and the skin between them
        sh, el, wr = np.array((0.50, 0.20, 0.16)), np.array((0.44, 0.265, 0.40)), np.array((0.36, 0.28, 0.62))
        tips = [np.array((0.00, 0.30, 0.66)), np.array((0.02, 0.32, 0.40)), np.array((0.10, 0.30, 0.16))]
        B.tube([sh, el, wr], [0.045, 0.035, 0.03], mat, sides=7, var=var)
        for tip in tips:
            B.tube([wr, (wr + tip) / 2 + (0.0, 0.012, 0.0), tip], [0.024, 0.016, 0.006], mat, sides=6, var=var)
        for ta, tb in zip(tips, tips[1:]):
            mid = (ta + tb) / 2
            _two_sided(B, mat, [wr, ta, mid + (wr - mid) * 0.28, tb], var=var)
        _two_sided(B, mat, [wr, tips[2], (0.30, 0.215, 0.10), sh], var=var)
        # legs drawn up under the body, three claws each
        for pts, rad in (([(0.54, 0.19, -0.02), (0.42, 0.26, -0.17), (0.56, 0.20, -0.27), (0.66, 0.17, -0.25)], [0.075, 0.06, 0.045, 0.05]),
                         ([(0.14, 0.20, 0.02), (0.20, 0.27, -0.10), (0.10, 0.27, -0.27), (0.20, 0.25, -0.33)], [0.10, 0.09, 0.055, 0.05])):
            B.tube(pts, rad, mat, sides=8, var=var)
            end = np.array(pts[-1])
            for dy in (-0.036, 0.0, 0.036):
                B.tube([end + (-0.02, dy * 0.6, 0.0), end + (0.05, dy, -0.02), end + (0.10, dy * 1.15, -0.065)], [0.024, 0.018, 0.004], mat, sides=5, var=var)
        if side is not None:
            B.pop()
    B.pop()


# ---------------------------------------------------------------------------------------------
# Weathering: the two vertex-colour channels the stone material reads besides tone and cavity dirt
# ---------------------------------------------------------------------------------------------
def _value_noise(p, period, seed):
    """Smooth value noise in -1..1 at the points p (N, 3), repeating its character every `period` metres."""
    q = np.asarray(p, dtype=np.float64) / period
    i0 = np.floor(q).astype(np.int64)
    f = q - i0
    f = f * f * (3.0 - 2.0 * f)

    def lattice(ix, iy, iz):
        n = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
        n = (n ^ (n >> 13)) * 1274126177
        return ((n ^ (n >> 16)) & 0xFFFF) / 32767.5 - 1.0

    out = np.zeros(len(q))
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                wgt = (f[:, 0] if dx else 1 - f[:, 0]) * (f[:, 1] if dy else 1 - f[:, 1]) * (f[:, 2] if dz else 1 - f[:, 2])
                out += wgt * lattice(i0[:, 0] + dx, i0[:, 1] + dy, i0[:, 2] + dz)
    return out


def bake_weather(obj, stone=("Ashlar", "Limestone", "Marble"), reach=2.4, seed=11):
    """Writes the weathering into the vertex colours:
      blue   rain streaks. On upright stone under anything that juts out above it (a ledge, a sill, a cornice,
             a buttress cap): darkest right under it, fading out downwards, broken into runs. The tops of
             ledges get a little as well, where dirt settles.
      alpha  large light and dark patches (0.5 = neutral): slow noise through the whole building, a little
             darker in the splash zone near the ground.
    Returns (mean streak on upright stone, share of upright stone that has any)."""
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    mesh = obj.data
    n = len(mesh.loops)
    co = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3).astype(np.float64)
    lv = np.empty(n, dtype=np.int32)
    mesh.loops.foreach_get("vertex_index", lv)
    npoly = len(mesh.polygons)
    pn = np.empty(npoly * 3, dtype=np.float32)
    mesh.polygons.foreach_get("normal", pn)
    pn = pn.reshape(-1, 3).astype(np.float64)
    lt = np.empty(npoly, dtype=np.int32)
    mesh.polygons.foreach_get("loop_total", lt)
    pm = np.empty(npoly, dtype=np.int32)
    mesh.polygons.foreach_get("material_index", pm)
    lp = np.repeat(np.arange(npoly), lt)                              # the polygon of every loop
    P, N = co[lv], pn[lp]
    is_stone = np.isin(pm, [i for i, m in enumerate(mesh.materials) if m.name.split(".")[0] in stone])[lp]

    macro = 0.6 * _value_noise(P, 7.0, seed) + 0.4 * _value_noise(P, 2.6, seed + 1)
    splash = np.clip(1.0 - P[:, 2] / 1.3, 0.0, 1.0)
    alpha = np.clip(0.5 + 0.3 * macro - 0.12 * splash, 0.0, 1.0)

    streak = np.zeros(n)
    wall = is_stone & (np.abs(N[:, 2]) < 0.35)
    idx = np.nonzero(wall)[0]
    if len(idx):
        key = np.concatenate([np.round(P[idx] * 500.0), np.round(N[idx] * 50.0)], axis=1).astype(np.int64)
        _u, first, inverse = np.unique(key, axis=0, return_index=True, return_inverse=True)
        bvh = BVHTree.FromPolygons(co.tolist(), [tuple(p.vertices) for p in mesh.polygons], all_triangles=False, epsilon=0.0)
        up = Vector((0.0, 0.0, 1.0))
        dist = np.full(len(first), np.inf)
        for j, i in enumerate(idx[first]):
            p, nn = P[i], N[i]
            hit = bvh.ray_cast(Vector((p[0] + nn[0] * 0.04, p[1] + nn[1] * 0.04, p[2] + 0.02)), up, reach)
            if hit[0] is not None:
                dist[j] = hit[3]
        d = dist[inverse.ravel()]
        fall = np.clip(1.0 - d / reach, 0.0, 1.0) ** 1.5
        tang = np.stack([-N[idx, 1], N[idx, 0], np.zeros(len(idx))], axis=1)
        tang /= np.maximum(np.linalg.norm(tang, axis=1, keepdims=True), 1e-9)
        along = (P[idx] * tang).sum(axis=1)
        runs = _value_noise(np.stack([along, np.zeros(len(idx)), np.zeros(len(idx))], axis=1), 0.45, seed + 2)
        streak[idx] = fall * np.clip(0.15 + 1.6 * runs, 0.0, 1.0)
    top = is_stone & (N[:, 2] > 0.8)
    streak[top] = 0.3 + 0.12 * _value_noise(P[top], 1.5, seed + 3)

    col = mesh.color_attributes["Col"]
    c = np.empty(n * 4, dtype=np.float32)
    col.data.foreach_get("color_srgb", c)
    c = c.reshape(-1, 4)
    c[:, 2] = np.clip(streak, 0.0, 1.0)
    c[:, 3] = alpha
    col.data.foreach_set("color_srgb", c.ravel())
    return (round(float(streak[wall].mean()), 3), round(float((streak[wall] > 0.02).mean()), 3)) if wall.any() else (0.0, 0.0)
