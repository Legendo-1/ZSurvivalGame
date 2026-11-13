"""Runs one of the house editor scripts in the open Unreal editor for one house, and prints its log.

Usage (Git Bash, Blender's Python; the editor must be open with remote execution on):
  "$PY" -I SourceArt/Houses/tools/ue_house.py <script_ue.py> <Folder> [NAME=value ...]
It sets HOUSE_NAME = "<Folder>" (and any NAME=value pairs, as Python literals where they parse, else strings) as
globals in the editor, then runs the script there. The editor keeps globals between runs, so every switch the
scripts read is set here each time.
"""
import ast
import os
import sys
import time

sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx  # noqa: E402

script, folder = os.path.abspath(sys.argv[1]), sys.argv[2]
lines = [f"HOUSE_NAME = {folder!r}"]
for kv in sys.argv[3:]:
    k, v = kv.split("=", 1)
    try:
        val = ast.literal_eval(v)
    except (ValueError, SyntaxError):
        val = v
    lines.append(f"{k} = {val!r}")
lines.append(f'exec(open(r"{script}", encoding="utf-8").read())')
code = "\n".join(lines)

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
    res = r.run_command(code, unattended=True, exec_mode=rx.MODE_EXEC_FILE, raise_on_failure=False)
    print("success:", res.get("success"), "result:", res.get("result"))
    for line in res.get("output", []):
        print(f'{line.get("type")}: {line.get("output")}'.rstrip())
    if not res.get("success"):
        sys.exit(3)
finally:
    r.stop()
