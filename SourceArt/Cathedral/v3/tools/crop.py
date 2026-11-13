"""usage: crop.py <in> <out> x0 y0 x1 y1 [scale]   (pixels, top-left origin; scale enlarges or shrinks the crop)"""
import sys
import OpenImageIO as oiio
src, dst = sys.argv[1], sys.argv[2]
x0, y0, x1, y1 = (int(v) for v in sys.argv[3:7])
scale = float(sys.argv[7]) if len(sys.argv) > 7 else 1.0
buf = oiio.ImageBuf(src)
spec = buf.spec()
x0, y0, x1, y1 = max(0, x0), max(0, y0), min(spec.width, x1), min(spec.height, y1)
cut = oiio.ImageBufAlgo.cut(buf, oiio.ROI(x0, x1, y0, y1))
if scale != 1.0:
    w, h = max(1, int((x1 - x0) * scale)), max(1, int((y1 - y0) * scale))
    cut = oiio.ImageBufAlgo.resize(cut, roi=oiio.ROI(0, w, 0, h))
if cut.spec().nchannels > 3 and dst.lower().endswith((".jpg", ".jpeg")):
    cut = oiio.ImageBufAlgo.channels(cut, (0, 1, 2))
cut.write(dst)
print("CROP", src.split("/")[-1], (spec.width, spec.height), "->", (cut.spec().width, cut.spec().height))
