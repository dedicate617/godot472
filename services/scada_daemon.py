#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Daemon & Service Supervisor for Linux / Raspberry Pi
==============================================================
Monitors MindSCADA backend services silently in the background with auto-restart,
health checking, CLI control, and systemd compatibility.

Commands:
    python3 scada_daemon.py start     # Run supervisor in background (daemon mode)
    python3 scada_daemon.py stop      # Stop supervisor and all managed services
    python3 scada_daemon.py restart   # Restart supervisor
    python3 scada_daemon.py status    # Show real-time health table of all services
    python3 scada_daemon.py run       # Run in foreground (for systemd / Docker)
"""

import os
import sys
import time
import signal
import socket
import argparse
import subprocess
import logging
from pathlib import Path

# Paths discovery
SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent
LOG_DIR = SCRIPT_DIR / "logs"
LOG_DIR.mkdir(parents=True, exist_ok=True)

PID_FILE = SCRIPT_DIR / ".scada_daemon.pid"
LOG_FILE = LOG_DIR / "scada_daemon.log"

# Logging setup
logging.basicConfig(
    filename=str(LOG_FILE),
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [Daemon] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("scada_daemon")

# Stream handler for console output when in foreground
console_handler = logging.StreamHandler(sys.stdout)
console_handler.setLevel(logging.INFO)
console_handler.setFormatter(logging.Formatter("%(asctime)s [%(levelname)s] %(message)s", datefmt="%H:%M:%S"))
logger.addHandler(console_handler)

BIN_DIR = SCRIPT_DIR / "bin"
if not BIN_DIR.is_dir():
    BIN_DIR = SCRIPT_DIR

WEB_DIR = SCRIPT_DIR / "web"
if not WEB_DIR.is_dir():
    if (SCRIPT_DIR.parent / "web").is_dir():
        WEB_DIR = SCRIPT_DIR.parent / "web"
    else:
        cand_web = ROOT_DIR.parent / "luxiangyi472" / "scada4" / "build_web"
        if cand_web.is_dir():
            WEB_DIR = cand_web
        else:
            WEB_DIR = SCRIPT_DIR

PYTHON_BIN = sys.executable or "python3"

# Services Configuration
SERVICES_CONFIG = [
    {
        "id": "opcua",
        "name": "OPC-UA Server",
        "port": 4840,
        "cmd_type": "python",
        "script": SCRIPT_DIR / "run_mindscada_svc.py",
        "fallback_script": SCRIPT_DIR / "uaserver_service.py",
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "nanomq",
        "name": "NanoMQ Broker",
        "port": 1883,
        "cmd_type": "binary",
        "binary_name": "nanomq",
        "args": ["start", "--daemon"],
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "tunnel",
        "name": "SCADA Tunnel",
        "port": 4841,
        "cmd_type": "binary",
        "binary_name": "scada_tunnel",
        "source_dir": ROOT_DIR / "tools" / "scada_tunnel",
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "ws_gateway",
        "name": "WS Gateway",
        "port": 8088,
        "cmd_type": "python",
        "script": SCRIPT_DIR / "scada_ws_gateway.py",
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "dev_server",
        "name": "Web Server",
        "port": 8090,
        "cmd_type": "python",
        "script": SCRIPT_DIR / "dev_server.py",
        "args": ["--port", "8090", "--dir", str(WEB_DIR)],
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "stream_gateway",
        "name": "Stream Gateway",
        "port": 8095,
        "cmd_type": "python",
        "script": SCRIPT_DIR / "scada_stream_gateway.py",
        "cwd": SCRIPT_DIR,
    },
]


def is_port_listening(port: int, host: str = "127.0.0.1", timeout: float = 0.35) -> bool:
    """Check if target TCP port is open and listening across IPv4/IPv6 dual stacks."""
    hosts = [host]
    if host in ("127.0.0.1", "localhost"):
        hosts = ["localhost", "127.0.0.1", "::1"]
    for h in hosts:
        try:
            with socket.create_connection((h, port), timeout=timeout):
                return True
        except Exception:
            pass
    return False


def find_binary(name: str, candidate_dir: Path) -> Path:
    """Find executable in bin/ or system PATH."""
    local_bin = candidate_dir / name
    if local_bin.is_file() and os.access(str(local_bin), os.X_OK):
        return local_bin

    # Search PATH
    path_env = os.environ.get("PATH", "")
    for p in path_env.split(os.pathsep):
        cand = Path(p) / name
        if cand.is_file() and os.access(str(cand), os.X_OK):
            return cand

    # Default fallback
    return local_bin


def build_tunnel_if_needed(source_dir: Path, target_bin: Path) -> bool:
    """Attempt compiling scada_tunnel using gcc if source exists and binary is missing."""
    if target_bin.is_file() and os.access(str(target_bin), os.X_OK):
        return True

    main_c = source_dir / "main.c"
    mongoose_c = source_dir / "mongoose.c"
    if not (main_c.is_file() and mongoose_c.is_file()):
        return False

    logger.info(f"Building scada_tunnel from source ({source_dir})...")
    target_bin.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "gcc", "-std=gnu99", "-O2", "-Wall",
        "-I", str(source_dir),
        str(main_c), str(mongoose_c),
        "-lpthread",
        "-o", str(target_bin)
    ]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
        if res.returncode == 0:
            target_bin.chmod(0o755)
            logger.info(f"Successfully compiled {target_bin}")
            return True
        else:
            logger.warning(f"gcc build failed: {res.stderr.decode('utf-8', errors='ignore')}")
    except Exception as e:
        logger.warning(f"Could not build scada_tunnel: {e}")
    return False


class ServiceSupervisor:
    """Manages service processes, health monitoring, and self-healing restarts."""

    def __init__(self, check_interval: float = 2.5):
        self.check_interval = check_interval
        self.running = False
        self.processes = {}  # service_id -> subprocess.Popen
        self.auto_heal = True

    def start_service(self, svc: dict) -> bool:
        sid = svc["id"]
        port = svc["port"]
        name = svc["name"]

        if is_port_listening(port):
            logger.info(f"Service '{name}' [:{port}] is already listening.")
            return True

        cmd = []
        cwd = svc.get("cwd", SCRIPT_DIR)
        cmd_type = svc.get("cmd_type", "python")

        if cmd_type == "python":
            script = svc.get("script")
            if not script or not script.is_file():
                fallback = svc.get("fallback_script")
                if fallback and fallback.is_file():
                    script = fallback
                else:
                    logger.error(f"Cannot start {name}: Python script {script} not found.")
                    return False
            cmd = [PYTHON_BIN, str(script)] + svc.get("args", [])

        elif cmd_type == "binary":
            bname = svc["binary_name"]
            bin_path = find_binary(bname, BIN_DIR)
            if not bin_path.is_file() and "source_dir" in svc:
                if build_tunnel_if_needed(svc["source_dir"], BIN_DIR / bname):
                    bin_path = BIN_DIR / bname

            if not bin_path.is_file():
                logger.warning(f"Binary '{bname}' for service '{name}' not found. Skipping.")
                return False

            cmd = [str(bin_path)] + svc.get("args", [])

        log_out = LOG_DIR / f"{sid}.log"
        logger.info(f"Starting {name} [:{port}]: {' '.join(cmd)}")

        try:
            with open(log_out, "a", encoding="utf-8") as f_log:
                proc = subprocess.Popen(
                    cmd,
                    cwd=str(cwd),
                    stdout=f_log,
                    stderr=subprocess.STDOUT,
                    start_new_session=True,  # Detach process group
                )
            self.processes[sid] = proc
            return True
        except Exception as e:
            logger.error(f"Failed to start {name}: {e}")
            return False

    def stop_service(self, sid: str, timeout: float = 2.0):
        proc = self.processes.get(sid)
        if proc and proc.poll() is None:
            try:
                proc.terminate()
                proc.wait(timeout=timeout)
            except Exception:
                try:
                    proc.kill()
                except Exception:
                    pass
        self.processes.pop(sid, None)

    def start_all(self):
        logger.info("Starting all backend services...")
        for svc in SERVICES_CONFIG:
            self.start_service(svc)
            time.sleep(0.3)

    def stop_all(self):
        logger.info("Stopping all backend services...")
        for sid in list(self.processes.keys()):
            self.stop_service(sid)

        # Fallback port check & kill for Linux
        if sys.platform != "win32":
            for svc in SERVICES_CONFIG:
                port = svc["port"]
                try:
                    subprocess.run(
                        f"fuser -k -n tcp {port} >/dev/null 2>&1 || true",
                        shell=True,
                        timeout=2,
                    )
                except Exception:
                    pass

    def run_monitor_loop(self):
        """Continuous health-checking and auto-recovery loop."""
        self.running = True
        logger.info(f"Service supervisor monitor loop started (interval: {self.check_interval}s).")
        self.start_all()

        while self.running:
            try:
                for svc in SERVICES_CONFIG:
                    if not self.running:
                        break
                    sid = svc["id"]
                    port = svc["port"]
                    name = svc["name"]

                    alive = is_port_listening(port)
                    proc = self.processes.get(sid)
                    proc_alive = (proc is not None) and (proc.poll() is None)

                    if not alive and self.auto_heal:
                        logger.warning(f"Service '{name}' [:{port}] is DOWN! Auto-healing restart...")
                        self.stop_service(sid)
                        self.start_service(svc)

                time.sleep(self.check_interval)
            except (KeyboardInterrupt, SystemExit):
                break
            except Exception as e:
                logger.error(f"Monitor loop error: {e}")
                time.sleep(self.check_interval)

        self.stop_all()
        logger.info("Supervisor monitor loop finished.")


def daemonize():
    """Double-fork to detach process as UNIX daemon."""
    if sys.platform == "win32":
        return  # Double-fork is not supported on Windows

    try:
        pid = os.fork()
        if pid > 0:
            sys.exit(0)  # Exit first parent
    except OSError as e:
        sys.stderr.write(f"Fork #1 failed: {e}\n")
        sys.exit(1)

    os.setsid()
    os.umask(0)

    try:
        pid = os.fork()
        if pid > 0:
            sys.exit(0)  # Exit second parent
    except OSError as e:
        sys.stderr.write(f"Fork #2 failed: {e}\n")
        sys.exit(1)

    # Redirect standard file descriptors to log file
    sys.stdout.flush()
    sys.stderr.flush()
    with open(str(LOG_FILE), "ab") as log_f:
        os.dup2(log_f.fileno(), sys.stdout.fileno())
        os.dup2(log_f.fileno(), sys.stderr.fileno())


def write_pid_file():
    PID_FILE.write_text(str(os.getpid()), encoding="utf-8")


def read_pid_file() -> int:
    if PID_FILE.is_file():
        try:
            return int(PID_FILE.read_text(encoding="utf-8").strip())
        except Exception:
            pass
    return -1


def is_pid_running(pid: int) -> bool:
    if pid <= 0:
        return False
    if sys.platform == "win32":
        return True
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def cmd_start():
    old_pid = read_pid_file()
    if old_pid > 0 and is_pid_running(old_pid):
        print(f"[INFO] MindSCADA Daemon is already running (PID: {old_pid}).")
        return 0

    print("[INFO] Starting MindSCADA Daemon supervisor in background...")
    daemonize()
    write_pid_file()

    supervisor = ServiceSupervisor()

    def sig_handler(signum, frame):
        logger.info(f"Caught signal {signum}. Terminating daemon...")
        supervisor.running = False

    signal.signal(signal.SIGTERM, sig_handler)
    signal.signal(signal.SIGINT, sig_handler)

    try:
        supervisor.run_monitor_loop()
    finally:
        if PID_FILE.is_file():
            try:
                PID_FILE.unlink()
            except OSError:
                pass
    return 0


def cmd_run():
    """Run in foreground (for systemd / Docker)."""
    write_pid_file()
    supervisor = ServiceSupervisor()

    def sig_handler(signum, frame):
        logger.info(f"Caught signal {signum}. Exiting...")
        supervisor.running = False

    signal.signal(signal.SIGTERM, sig_handler)
    signal.signal(signal.SIGINT, sig_handler)

    try:
        supervisor.run_monitor_loop()
    finally:
        if PID_FILE.is_file():
            try:
                PID_FILE.unlink()
            except OSError:
                pass
    return 0


def cmd_stop():
    pid = read_pid_file()
    if pid <= 0 or not is_pid_running(pid):
        print("[INFO] MindSCADA Daemon is not running.")
    else:
        print(f"[INFO] Stopping MindSCADA Daemon (PID: {pid})...")
        try:
            os.kill(pid, signal.SIGTERM)
            for _ in range(20):
                if not is_pid_running(pid):
                    break
                time.sleep(0.2)
            if is_pid_running(pid):
                os.kill(pid, signal.SIGKILL)
        except Exception as e:
            print(f"[WARN] Error killing daemon PID {pid}: {e}")

        if PID_FILE.is_file():
            try:
                PID_FILE.unlink()
            except OSError:
                pass

    # Stop child services
    print("[INFO] Cleaning up service ports...")
    supervisor = ServiceSupervisor()
    supervisor.stop_all()
    print("[SUCCESS] All services and daemon stopped.")
    return 0


def cmd_status():
    daemon_pid = read_pid_file()
    daemon_alive = is_pid_running(daemon_pid)

    c_green = "\033[92m" if sys.stdout.isatty() else ""
    c_red = "\033[91m" if sys.stdout.isatty() else ""
    c_bold = "\033[1m" if sys.stdout.isatty() else ""
    c_reset = "\033[0m" if sys.stdout.isatty() else ""

    print("=" * 65)
    print(f" {c_bold}MindSCADA Service Supervisor Status (Linux/Pi){c_reset}")
    print("=" * 65)
    daemon_str = f"{c_green}RUNNING (PID: {daemon_pid}){c_reset}" if daemon_alive else f"{c_red}STOPPED{c_reset}"
    print(f" Supervisor Daemon : {daemon_str}")
    print(f" Log File Location : {LOG_FILE}")
    print("-" * 65)
    print(f" {'ID':<15} {'Service Name':<20} {'Port':<8} {'Status':<15}")
    print("-" * 65)

    all_ok = True
    for svc in SERVICES_CONFIG:
        sid = svc["id"]
        name = svc["name"]
        port = svc["port"]
        listening = is_port_listening(port)
        if listening:
            stat_str = f"{c_green}[OK] ACTIVE{c_reset}"
        else:
            stat_str = f"{c_red}[DOWN] INACTIVE{c_reset}"
            all_ok = False
        print(f" {sid:<15} {name:<20} {port:<8} {stat_str:<15}")

    print("=" * 65)
    if daemon_alive and all_ok:
        print(f" {c_green}All 6 core services are healthy and active.{c_reset}")
    else:
        print(f" {c_red}Some services are inactive. Run 'scada_daemon.py start' to launch.{c_reset}")
    print("=" * 65)
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="MindSCADA Daemon & Supervisor for Linux / Raspberry Pi",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "action",
        choices=["start", "stop", "restart", "status", "run"],
        default="status",
        nargs="?",
        help="Action to perform: start, stop, restart, status, or run (default: status)",
    )
    args = parser.parse_args()

    if args.action == "start":
        return cmd_start()
    elif args.action == "stop":
        return cmd_stop()
    elif args.action == "restart":
        cmd_stop()
        time.sleep(1)
        return cmd_start()
    elif args.action == "status":
        return cmd_status()
    elif args.action == "run":
        return cmd_run()
    return 0


if __name__ == "__main__":
    sys.exit(main())
