"""Average colour of named facade areas in base-colour pictures, and the ratio between two pictures.

Usage (Blender's Python):
  "$PY" -I color_match.py <rects.json> <picture A> <map A> [<picture B> <map B>] [--overlay <out.png>]

rects.json   {"stone": [[x0, x1, z0, z1], ...], "brick": [...], ...}   facade metres: x from the left face of the
             wall (as a viewer facing the facade sees it), z from the street. A surface may list several areas.
map          "col0,row0,ppm,sign": the picture column of facade x = 0, the row of z = 0, pixels per metre,
             and +1 / -1 for whether x grows to the right / to the left of the picture.
             For measure_meshy.py front pictures: col0 = (wall offset - u_at_col0) * ppm, row0 = z_at_row0 * ppm.
             For an orthographic Unreal capture: see ledger section 4.
Pictures are sRGB PNGs of base colour (Blender unlit render, or SCS_BASE_COLOR into an sRGB render target).
Prints mean linear RGB per surface, and with two pictures B / A per channel and the luminance ratio.
--overlay draws the areas on picture A (or B when given), to check that they sit on the right surface.
"""
import json
import sys

import numpy as np
import OpenImageIO as oiio

args = sys.argv[1:]
overlay = None
if "--overlay" in args:
    i = args.index("--overlay")
    overlay = args[i + 1]
    del args[i:i + 2]
rects = json.load(open(args[0], encoding="utf-8"))
pics = [(args[i], [float(v) for v in args[i + 1].split(",")]) for i in range(1, len(args), 2)]


def load(path):
    # Unreal's SCS_BASE_COLOR captures carry alpha 0; OIIO would premultiply the colour by it unless told not to
    cfg = oiio.ImageSpec()
    cfg.attribute("oiio:UnassociatedAlpha", 1)
    buf = oiio.ImageBuf(path, 0, 0, cfg)
    px = buf.get_pixels(oiio.FLOAT)
    return px[:, :, :3]


def to_linear(c):
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def box(m, r, shape):
    col0, row0, ppm, sign = m
    x0, x1, z0, z1 = r
    ca, cb = sorted((col0 + sign * x0 * ppm, col0 + sign * x1 * ppm))
    ra, rb = row0 - z1 * ppm, row0 - z0 * ppm
    ca, cb, ra, rb = (int(round(v)) for v in (ca, cb, ra, rb))
    return max(0, ra), min(shape[0], rb), max(0, ca), min(shape[1], cb)


results = []
for path, m in pics:
    img = load(path)
    lin = to_linear(img)
    res = {}
    for name, areas in rects.items():
        acc = []
        for r in areas:
            a, b, c, d = box(m, r, img.shape)
            if b > a and d > c:
                acc.append(lin[a:b, c:d].reshape(-1, 3))
        if acc:
            res[name] = np.concatenate(acc).mean(axis=0)
    results.append(res)

lum = np.array([0.2126, 0.7152, 0.0722])
for name in rects:
    line = f"{name:10s}"
    for k, res in enumerate(results):
        if name in res:
            line += f"  {'AB'[k]} ({res[name][0]:.3f}, {res[name][1]:.3f}, {res[name][2]:.3f})"
    if len(results) == 2 and name in results[0] and name in results[1]:
        ratio = results[1][name] / np.maximum(results[0][name], 1e-4)
        lr = float(results[1][name] @ lum / max(results[0][name] @ lum, 1e-4))
        line += f"  B/A ({ratio[0]:.2f}, {ratio[1]:.2f}, {ratio[2]:.2f}) lum {lr:.2f}"
    print("COLOR", line)

if overlay:
    path, m = pics[-1]
    img = load(path).copy()
    colors = [(1, 0, 0), (0, 1, 0), (0, 0.4, 1), (1, 1, 0), (1, 0, 1), (0, 1, 1), (1, 0.5, 0), (0.6, 0.3, 1)]
    for k, (name, areas) in enumerate(rects.items()):
        for r in areas:
            a, b, c, d = box(m, r, img.shape)
            col = colors[k % len(colors)]
            img[a:b, c:min(c + 2, d)] = col
            img[a:b, max(d - 2, c):d] = col
            img[a:min(a + 2, b), c:d] = col
            img[max(b - 2, a):b, c:d] = col
    buf = oiio.ImageBuf(oiio.ImageSpec(img.shape[1], img.shape[0], 3, oiio.UINT8))
    buf.set_pixels(oiio.ROI(0, img.shape[1], 0, img.shape[0]), img.astype(np.float32))
    buf.write(overlay)
    print("OVERLAY", overlay)
