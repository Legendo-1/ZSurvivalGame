"""Prints what a GLB holds (meshes, triangle count, materials, embedded images and their size),
reading only the JSON chunk and image headers. Usage: glb_info.py <file.glb> [...]"""
import json, struct, sys

def info(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, ver, total = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF", "not a GLB"
    jlen, jtype = struct.unpack_from("<I4s", data, 12)
    gj = json.loads(data[20:20 + jlen])
    bin_off = 20 + jlen + 8
    tris = 0
    for m in gj.get("meshes", []):
        for p in m["primitives"]:
            if "indices" in p:
                tris += gj["accessors"][p["indices"]]["count"] // 3
    imgs = []
    for im in gj.get("images", []):
        bv = gj["bufferViews"][im["bufferView"]]
        b = data[bin_off + bv.get("byteOffset", 0): bin_off + bv.get("byteOffset", 0) + 32]
        w = h = None
        if b[:8] == b"\x89PNG\r\n\x1a\n":
            w, h = struct.unpack(">II", b[16:24])
        imgs.append((im.get("mimeType"), bv["byteLength"], w, h))
    mats = []
    for m in gj.get("materials", []):
        pbr = m.get("pbrMetallicRoughness", {})
        mats.append({k: ("tex" if k.endswith("Texture") else v) for k, v in pbr.items()}
                    | ({"normal": "tex"} if "normalTexture" in m else {}))
    print(f"{path}\n  meshes={len(gj.get('meshes', []))} tris={tris} materials={mats}\n  images={imgs}")

for p in sys.argv[1:]:
    info(p)
