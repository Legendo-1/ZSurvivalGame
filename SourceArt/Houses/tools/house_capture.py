"""Scene captures in the open Unreal editor, ticked over many frames so Nanite pages stream in.

Usage (Git Bash, Blender's Python; the editor must be open with remote execution on):
  "$PY" -I SourceArt/Houses/tools/house_capture.py <views.json> <out folder> [--ticks 30] [--keep]

views.json:
  {"views": [{"name": "front", "pos": [x, y, z], "at": [x, y, z], "fov": 70, "w": 1600, "h": 1000, "base": false}, ...],
   "fills": [[x, y, z, candela, r, g, b], ...]}            (fills: optional temporary point lights)
World centimetres. "base": true captures SCS_BASE_COLOR (unlit albedo) instead of the final image.

It fills the template SourceArt/Cathedral/v3/tools/ncap.py and runs it in four steps over one connection:
spawn (capture actors and fills, labelled TMP_N*), --ticks capture calls with a pause between them (the editor
ticks in between, so Nanite and textures stream in), export (one PNG per view: <out folder>/<name>.png) and done
(the temporary actors are destroyed; --keep leaves them for another run). The filled scripts are written to
Saved/HouseCheck/_capture/.
"""
import json
import os
import sys
import time

sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
TEMPLATE = os.path.join(PROJECT, "SourceArt", "Cathedral", "v3", "tools", "ncap.py")
WORK = os.path.join(PROJECT, "Saved", "HouseCheck", "_capture")

args = sys.argv[1:]
views_file, out = args[0], os.path.abspath(args[1])
ticks = int(args[args.index("--ticks") + 1]) if "--ticks" in args else 30
keep = "--keep" in args
spec = json.load(open(views_file, encoding="utf-8"))
os.makedirs(out, exist_ok=True)
os.makedirs(WORK, exist_ok=True)
template = open(TEMPLATE, encoding="utf-8").read()


def script(mode):
    code = (template.replace("__OUT__", out.replace("\\", "/")).replace("__MODE__", mode)
            .replace("__VIEWS__", json.dumps(spec["views"])).replace("__FILLS__", json.dumps(spec.get("fills", []))))
    path = os.path.join(WORK, f"ncap_{mode}.py")
    open(path, "w", encoding="utf-8").write(code)
    return path


r = rx.RemoteExecution()
r.start()
try:
    deadline = time.time() + 8
    while time.time() < deadline and not r.remote_nodes:
        time.sleep(0.2)
    if not r.remote_nodes:
        print("NO_EDITOR_FOUND")
        sys.exit(2)
    r.open_command_connection(r.remote_nodes[0]["node_id"])

    def run(mode):
        res = r.run_command(f'exec(open(r"{script(mode)}", encoding="utf-8").read())', unattended=True,
                            exec_mode=rx.MODE_EXEC_FILE, raise_on_failure=False)
        lines = [str(x.get("output")).rstrip() for x in res.get("output", [])]
        if not res.get("success"):
            print(f"CAPTURE {mode} FAILED:", res.get("result"), *lines, sep="\n  ")
            sys.exit(3)
        return lines

    print(*run("spawn"))
    for _ in range(ticks):
        time.sleep(0.35)
        run("tick")
    time.sleep(0.5)
    print(*run("export"))
    if not keep:
        print(*run("done"))
finally:
    r.stop()
print("CAPTURED", [v["name"] for v in spec["views"]], "->", out)
