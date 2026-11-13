"""Runs a Python file inside the open Unreal editor (needs Project Settings > Plugins > Python >
Enable Remote Execution). Usage: python ue_run.py <script.py>"""
import sys, time
sys.path.insert(0, r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx

script = sys.argv[1]
r = rx.RemoteExecution()
r.start()
try:
    deadline = time.time() + 8
    while time.time() < deadline and not r.remote_nodes:
        time.sleep(0.2)
    if not r.remote_nodes:
        print("NO_EDITOR_FOUND")
        sys.exit(2)
    node = r.remote_nodes[0]
    print("editor:", node.get("project_name"), node.get("engine_version"))
    r.open_command_connection(node["node_id"])
    res = r.run_command(f'exec(open(r"{script}", encoding="utf-8").read())', unattended=True,
                        exec_mode=rx.MODE_EXEC_FILE, raise_on_failure=False)
    print("success:", res.get("success"), "result:", res.get("result"))
    for line in res.get("output", []):
        print(f'{line.get("type")}: {line.get("output")}'.rstrip())
finally:
    r.stop()
