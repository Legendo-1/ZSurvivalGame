"""Runs of equal depth in a raycast_meshy.py depth map (added by house #4).

Usage (Blender's Python):  "$PY" -I depth_runs.py <depth.npy> col|row <value m> [tol 0.02] [minlen 0.05]
  col: down the column at x = value (house metres), printing z ranges;  row: along the row at z = value, printing x ranges.
  Each run prints "a-b:depth" (the median hit y); depths within tol are one run, runs shorter than minlen are dropped.
Assumes the map was made with --res 0.02 and --z1 16.0 (change res / z1 below for other maps); nan = no hit.
"""
import sys
import numpy as np
dm = np.load(sys.argv[1])
res, z1 = 0.02, 16.0
mode, v = sys.argv[2], float(sys.argv[3])
tol = float(sys.argv[4]) if len(sys.argv) > 4 else 0.02
minlen = float(sys.argv[5]) if len(sys.argv) > 5 else 0.05
if mode == "col":
    c = int(round(v / res))
    vals = dm[:, c]
    coord = z1 - np.arange(len(vals)) * res
else:
    r = int(round((z1 - v) / res))
    vals = dm[r, :]
    coord = np.arange(len(vals)) * res
out = []
start = 0
for i in range(1, len(vals) + 1):
    same = i < len(vals) and ((np.isnan(vals[i]) and np.isnan(vals[start])) or abs(vals[i] - vals[start]) <= tol)
    if not same:
        a, b = coord[start], coord[i - 1]
        if abs(b - a) + 1e-6 >= minlen:
            out.append(f"{min(a,b):.2f}-{max(a,b):.2f}:{np.nanmedian(vals[start:i]) if not np.all(np.isnan(vals[start:i])) else float('nan'):.2f}")
        start = i
print("  ".join(out))
