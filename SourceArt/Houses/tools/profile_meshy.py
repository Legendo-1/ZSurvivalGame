"""Silhouette profile of a measure_meshy.py side picture: for every z step, where the model starts and ends along
the picture's u axis (house metres). Shows how far cornices, surrounds, hoods and lanterns stand out of a facade.

Usage (Blender's Python):  "$PY" -I profile_meshy.py <m_info.json> <view: left|right|front|back> [--step 0.05]
      [--z0 Z --z1 Z] [--u0 U --u1 U]   (only count pixels with u in u0..u1, e.g. to skip a lantern)
Background = the flat world colour of measure_meshy.py; anything else counts as the model.
"""
import json
import sys

import numpy as np
import OpenImageIO as oiio

info = json.load(open(sys.argv[1], encoding="utf-8"))
view = sys.argv[2]
opt = {sys.argv[i]: float(sys.argv[i + 1]) for i in range(3, len(sys.argv), 2)}
v = info["views"][view]
ppm = info["ppm"]
img = oiio.ImageBuf(v["file"]).get_pixels(oiio.FLOAT)[:, :, :3]
bg = img[2, 2]
mask = np.abs(img - bg).sum(axis=2) > 0.03
H, W = mask.shape
cols = np.arange(W)
u = v["u_at_col0"] + v["u_sign"] * cols / ppm
if "--u0" in opt:
    mask &= (u >= opt["--u0"])[None, :] & (u <= opt["--u1"])[None, :]
step = opt.get("--step", 0.05)
ztop = v["z_at_row0"]
z0, z1 = opt.get("--z0", 0.0), opt.get("--z1", ztop)
z = z1
while z >= z0 - 1e-9:
    r0, r1 = int(round((ztop - z - step) * ppm)), int(round((ztop - z) * ppm))
    band = mask[max(r0, 0):max(r1, 1)].any(axis=0)
    if band.any():
        us = u[band]
        print(f"z {z - step:6.2f}..{z:6.2f}  u {us.min():6.3f} .. {us.max():6.3f}")
    else:
        print(f"z {z - step:6.2f}..{z:6.2f}  -")
    z -= step
