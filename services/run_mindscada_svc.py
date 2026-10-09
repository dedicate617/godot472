import subprocess
import time
import sys
import os

CMD = [
    r"C:\MindSCADA\build\build-Make_MindSCADA-Desktop_Qt_5_13_1_MSVC2017_64bit-Release\bin\release\MindSCADASvc.windows.opt.64.exe",
    "-M", "-V", "-p", r"C:\prj\luxiangyi472\server\server.devx"
]
CWD = r"C:\MindSCADA\build\build-Make_MindSCADA-Desktop_Qt_5_13_1_MSVC2017_64bit-Release\bin\release"

print("[Supervisor] MindSCADASvc Auto-Restart Supervisor started.")
sys.stdout.flush()

while True:
    try:
        print("[Supervisor] Starting MindSCADASvc...", flush=True)
        # Use DEVNULL to prevent broken pipe issues on high log volume
        p = subprocess.Popen(CMD, cwd=CWD, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        print(f"[Supervisor] MindSCADASvc running with PID: {p.pid}", flush=True)
        p.wait()
        print(f"[Supervisor] MindSCADASvc exited with code {p.returncode}, restarting in 1s...", flush=True)
    except Exception as e:
        print(f"[Supervisor] Error: {e}, retrying in 2s...", flush=True)
        time.sleep(2)
    time.sleep(1)
