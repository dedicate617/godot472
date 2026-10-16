#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Daemon & System Tray Supervisor
==========================================
Monitors MindSCADA backend services silently in the background with a Windows System Tray icon.

Features:
- Windows System Tray icon with dynamic status menu
- Periodic TCP port health check (4840, 1883, 4841, 8088, 8090, 8095)
- Auto-restart crashed services without console windows (CREATE_NO_WINDOW)
- One-click browser launch for Web SCADA and Stream Gateway
- Start / Stop / Restart all services from tray menu
"""

import os
import sys
import time
import socket
import threading
import subprocess
import webbrowser
import logging
from pathlib import Path

# Setup logging
LOG_FILE = Path(__file__).resolve().parent / "daemon_tray.log"
logging.basicConfig(
    filename=str(LOG_FILE),
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [Daemon] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("scada_daemon_tray")

try:
    import win32api
    import win32con
    import win32gui
except ImportError:
    logger.error("pywin32 is not installed! System tray requires pywin32.")
    sys.exit(1)

CREATE_NO_WINDOW = 0x08000000
WM_TRAY_NOTIFY = win32con.WM_USER + 20

SCRIPT_DIR = Path(__file__).resolve().parent
script_str = str(SCRIPT_DIR)
if script_str.startswith("\\\\vmware-host\\Shared Folders\\D\\"):
    script_str = script_str.replace("\\\\vmware-host\\Shared Folders\\D\\", "Z:\\D\\")
elif script_str.startswith("\\\\vmware-host\\Shared Folders\\"):
    script_str = script_str.replace("\\\\vmware-host\\Shared Folders\\", "Z:\\")
SCRIPT_DIR = Path(script_str)
ROOT_DIR = SCRIPT_DIR.parent
BIN_DIR = SCRIPT_DIR / "bin"
if not BIN_DIR.is_dir():
    BIN_DIR = SCRIPT_DIR

WEB_DIR = SCRIPT_DIR / "web"
if not WEB_DIR.is_dir():
    WEB_DIR = Path(r"C:\prj\luxiangyi472\scada4\build_web")
if not WEB_DIR.is_dir():
    WEB_DIR = SCRIPT_DIR

# Define the 6 core services
SERVICES_CONFIG = [
    {
        "id": "opcua",
        "name": "OPC-UA 服务",
        "port": 4840,
        "exe": BIN_DIR / "run_mindscada_svc.exe",
        "py": SCRIPT_DIR / "uaserver_service.py",
        "fallback_py": SCRIPT_DIR / "run_mindscada_svc.py",
        "native_fallback": Path(r"C:\MindSCADA\build\build-Make_MindSCADA-Desktop_Qt_5_13_1_MSVC2017_64bit-Release\bin\release\MindSCADASvc.windows.opt.64.exe"),
        "native_args": ["-M", "-V", "-p", r"C:\prj\luxiangyi472\server\server.devx"],
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "nanomq",
        "name": "NanoMQ 消息代理",
        "port": 1883,
        "exe": BIN_DIR / "nanomq.exe",
        "args": ["start", "--daemon"],
        "fallback_exe": Path(r"C:\nanomq-0.21.8-windows-x86_64\bin\nanomq.exe"),
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "tunnel",
        "name": "SCADA 隧道服务",
        "port": 4841,
        "exe": BIN_DIR / "scada_tunnel.exe",
        "fallback_exe": ROOT_DIR / "tools" / "scada_tunnel" / "scada_tunnel.exe",
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "ws_gateway",
        "name": "WS 数据网关",
        "port": 8088,
        "exe": BIN_DIR / "scada_ws_gateway.exe",
        "py": SCRIPT_DIR / "scada_ws_gateway.py",
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "dev_server",
        "name": "Web 发布服务",
        "port": 8090,
        "exe": BIN_DIR / "dev_server.exe",
        "py": SCRIPT_DIR / "dev_server.py",
        "args": ["--port", "8090", "--dir", str(WEB_DIR)],
        "cwd": SCRIPT_DIR,
    },
    {
        "id": "stream_gateway",
        "name": "画面串流网关",
        "port": 8095,
        "exe": BIN_DIR / "scada_stream_gateway.exe",
        "py": SCRIPT_DIR / "scada_stream_gateway.py",
        "cwd": SCRIPT_DIR,
    },
]

def is_port_listening(port: int, host: str = "127.0.0.1", timeout: float = 0.35) -> bool:
    """Check if target TCP port is open and listening."""
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except Exception:
        return False


class ServiceSupervisor:
    def __init__(self):
        self.auto_restart = True
        self.running = True
        self.service_status = {s["id"]: False for s in SERVICES_CONFIG}
        self.subprocesses = {}
        self.lock = threading.Lock()

    def check_all(self):
        """Update listening status for all monitored services."""
        with self.lock:
            for s in SERVICES_CONFIG:
                self.service_status[s["id"]] = is_port_listening(s["port"])

    def start_service(self, s: dict):
        """Launch an individual service quietly without console window."""
        svc_id = s["id"]
        if is_port_listening(s["port"]):
            return

        cmd = None
        cwd = s.get("cwd", SCRIPT_DIR)

        # 1. Prefer Python script if specified (instant startup & always latest code)
        if s.get("py") and Path(s["py"]).is_file():
            cmd = [sys.executable, str(s["py"])] + s.get("args", [])
        # 2. Check standalone .exe (for native C/C++ binaries: nanomq, scada_tunnel)
        elif s.get("exe") and Path(s["exe"]).is_file():
            cmd = [str(s["exe"])] + s.get("args", [])
        # 3. Check fallback exe
        elif s.get("fallback_exe") and Path(s["fallback_exe"]).is_file():
            cmd = [str(s["fallback_exe"])] + s.get("args", [])
        # 4. Check fallback python script
        elif s.get("fallback_py") and Path(s["fallback_py"]).is_file():
            cmd = [sys.executable, str(s["fallback_py"])] + s.get("args", [])
        # 5. Check native fallback (e.g. MindSCADASvc)
        elif s.get("native_fallback") and Path(s["native_fallback"]).is_file():
            cmd = [str(s["native_fallback"])] + s.get("native_args", [])
            cwd = s["native_fallback"].parent

        if not cmd:
            logger.warning(f"No executable or script found for {s['name']}")
            return

        try:
            logger.info(f"Starting {s['name']} silently: {cmd}")
            proc = subprocess.Popen(
                cmd,
                cwd=str(cwd),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                stdin=subprocess.DEVNULL,
                creationflags=CREATE_NO_WINDOW,
            )
            with self.lock:
                self.subprocesses[svc_id] = proc
        except Exception as e:
            logger.error(f"Failed to start {s['name']}: {e}")

    def start_all(self):
        """Start all services that are currently not running."""
        for s in SERVICES_CONFIG:
            if not is_port_listening(s["port"]):
                self.start_service(s)
                time.sleep(0.5)
        self.check_all()

    def stop_all(self):
        """Stop all background services quietly."""
        logger.info("Stopping all services...")
        # 1. Terminate tracked subprocesses
        with self.lock:
            for svc_id, proc in list(self.subprocesses.items()):
                try:
                    proc.terminate()
                except Exception:
                    pass
            self.subprocesses.clear()

        # 2. Kill by process name
        exe_names = [
            "dev_server.exe",
            "scada_ws_gateway.exe",
            "scada_stream_gateway.exe",
            "run_mindscada_svc.exe",
            "scada_tunnel.exe",
            "nanomq.exe",
            "MindSCADASvc.windows.opt.64.exe",
        ]
        for name in exe_names:
            try:
                subprocess.run(
                    ["taskkill", "/F", "/IM", name],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    creationflags=CREATE_NO_WINDOW,
                )
            except Exception:
                pass

        # 3. Free ports (4840, 8088, 8090, 8095)
        for s in SERVICES_CONFIG:
            port = s["port"]
            try:
                out = subprocess.check_output(
                    f'netstat -ano | findstr ":{port}" | findstr "LISTENING"',
                    shell=True,
                    creationflags=CREATE_NO_WINDOW,
                    text=True,
                )
                for line in out.strip().splitlines():
                    parts = line.split()
                    if len(parts) >= 5:
                        pid = parts[-1]
                        if pid.isdigit() and int(pid) > 4:
                            subprocess.run(
                                ["taskkill", "/F", "/PID", pid],
                                stdout=subprocess.DEVNULL,
                                stderr=subprocess.DEVNULL,
                                creationflags=CREATE_NO_WINDOW,
                            )
            except Exception:
                pass

        time.sleep(1.0)
        self.check_all()

    def restart_all(self):
        """Stop then start all services."""
        self.stop_all()
        time.sleep(1.0)
        self.start_all()

    def monitor_loop(self, on_update_callback=None):
        """Background daemon loop checking health and auto-restarting."""
        while self.running:
            self.check_all()
            if self.auto_restart:
                for s in SERVICES_CONFIG:
                    if not self.service_status.get(s["id"], False):
                        logger.info(f"{s['name']} (Port {s['port']}) is down, auto-restarting...")
                        self.start_service(s)
            if on_update_callback:
                on_update_callback()
            time.sleep(2.5)


class MindScadaTrayApp:
    def __init__(self, supervisor: ServiceSupervisor):
        self.supervisor = supervisor
        self.hwnd = None
        self.hicon = None

        # Menu command IDs
        self.CMD_FIRST_SERVICE = 1000
        self.CMD_OPEN_WEB = 2001
        self.CMD_OPEN_STREAM = 2002
        self.CMD_TOGGLE_AUTORESTART = 2003
        self.CMD_START_ALL = 2004
        self.CMD_STOP_ALL = 2005
        self.CMD_RESTART_ALL = 2006
        self.CMD_EXIT_KEEP = 2007
        self.CMD_EXIT_STOP = 2008

        self._init_window()

    def _init_window(self):
        msg_map = {
            win32con.WM_DESTROY: self.on_destroy,
            win32con.WM_COMMAND: self.on_command,
            WM_TRAY_NOTIFY: self.on_tray_notify,
        }
        wc = win32gui.WNDCLASS()
        wc.hInstance = win32api.GetModuleHandle(None)
        wc.lpszClassName = "MindSCADA_Daemon_Tray_Class"
        wc.lpfnWndProc = msg_map
        self.class_atom = win32gui.RegisterClass(wc)
        self.hwnd = win32gui.CreateWindow(
            self.class_atom,
            "MindSCADA_Daemon_Tray",
            0, 0, 0,
            win32con.CW_USEDEFAULT,
            win32con.CW_USEDEFAULT,
            0, 0,
            wc.hInstance,
            None,
        )

        # Load icon: try godot.ico or stock application icon
        icon_path = ROOT_DIR / "patches" / "assets" / "godot.ico"
        if icon_path.is_file():
            try:
                self.hicon = win32gui.LoadImage(
                    0,
                    str(icon_path),
                    win32con.IMAGE_ICON,
                    0, 0,
                    win32con.LR_LOADFROMFILE | win32con.LR_DEFAULTSIZE,
                )
            except Exception:
                self.hicon = win32gui.LoadIcon(0, win32con.IDI_APPLICATION)
        else:
            self.hicon = win32gui.LoadIcon(0, win32con.IDI_APPLICATION)

        # Add tray icon (with graceful fallback if running in non-desktop window station)
        self.tray_installed = False
        try:
            flags = win32gui.NIF_ICON | win32gui.NIF_MESSAGE | win32gui.NIF_TIP
            nid = (self.hwnd, 0, flags, WM_TRAY_NOTIFY, self.hicon, "MindSCADA 正在初始化...")
            win32gui.Shell_NotifyIcon(win32gui.NIM_ADD, nid)
            self.tray_installed = True
            logger.info("Windows System Tray icon created successfully.")
            # Show balloon tooltip on start
            self.show_balloon("MindSCADA 后台守护已启动", "所有后台服务已就绪并在托盘静默运行。双击托盘打开 Web SCADA。")
        except Exception as e:
            logger.warning(f"Windows Shell Tray notification area not available in current window station: {e}. Supervisor running in background daemon mode.")

    def show_balloon(self, title: str, text: str):
        """Display a Windows balloon notification from the tray."""
        if not self.tray_installed:
            return
        try:
            nid = (
                self.hwnd, 0,
                win32gui.NIF_INFO,
                WM_TRAY_NOTIFY,
                self.hicon,
                "",
                text,
                200,
                title,
                win32gui.NIIF_INFO,
            )
            win32gui.Shell_NotifyIcon(win32gui.NIM_MODIFY, nid)
        except Exception as e:
            logger.debug(f"Balloon error: {e}")

    def update_tooltip(self):
        """Update tray tooltip with count of running services."""
        if not self.tray_installed:
            return
        running_cnt = sum(1 for v in self.supervisor.service_status.values() if v)
        total_cnt = len(SERVICES_CONFIG)
        tip = f"MindSCADA 守护 [{running_cnt}/{total_cnt} 正常]"
        try:
            nid = (self.hwnd, 0, win32gui.NIF_TIP, WM_TRAY_NOTIFY, self.hicon, tip)
            win32gui.Shell_NotifyIcon(win32gui.NIM_MODIFY, nid)
        except Exception:
            pass

    def on_tray_notify(self, hwnd, msg, wparam, lparam):
        """Handle mouse clicks on tray icon."""
        if lparam == win32con.WM_LBUTTONDBLCLK:
            # Double click: open Web SCADA in default browser
            webbrowser.open("http://127.0.0.1:8090/mindscada.html")
        elif lparam in (win32con.WM_RBUTTONUP, win32con.WM_CONTEXTMENU):
            self.show_menu()
        return 0

    def show_menu(self):
        """Build and display the context menu."""
        menu = win32gui.CreatePopupMenu()

        # Title item (disabled)
        running_cnt = sum(1 for v in self.supervisor.service_status.values() if v)
        total_cnt = len(SERVICES_CONFIG)
        title_str = f"MindSCADA 后台守护 ({running_cnt}/{total_cnt} 运行中)"
        win32gui.AppendMenu(menu, win32con.MF_STRING | win32con.MF_DISABLED | win32con.MF_GRAYED, 0, title_str)
        win32gui.AppendMenu(menu, win32con.MF_SEPARATOR, 0, "")

        # 6 Service status rows
        for i, s in enumerate(SERVICES_CONFIG):
            is_alive = self.supervisor.service_status.get(s["id"], False)
            mark = "● [运行]" if is_alive else "○ [停止]"
            text = f"{mark} {s['name']} (:{s['port']})"
            win32gui.AppendMenu(menu, win32con.MF_STRING | win32con.MF_GRAYED, self.CMD_FIRST_SERVICE + i, text)

        win32gui.AppendMenu(menu, win32con.MF_SEPARATOR, 0, "")

        # Browser links
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_OPEN_WEB, "🌐 打开 Web SCADA (8090)")
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_OPEN_STREAM, "📺 打开 画面串流 (8095)")

        win32gui.AppendMenu(menu, win32con.MF_SEPARATOR, 0, "")

        # Control actions
        auto_check = win32con.MF_CHECKED if self.supervisor.auto_restart else win32con.MF_UNCHECKED
        win32gui.AppendMenu(menu, win32con.MF_STRING | auto_check, self.CMD_TOGGLE_AUTORESTART, "自动崩溃自愈守护")
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_START_ALL, "▶ 启动所有服务")
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_RESTART_ALL, "🔄 重启所有服务")
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_STOP_ALL, "⏹ 停止所有服务")

        win32gui.AppendMenu(menu, win32con.MF_SEPARATOR, 0, "")

        # Exit options
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_EXIT_KEEP, "退出托盘 (保留服务运行)")
        win32gui.AppendMenu(menu, win32con.MF_STRING, self.CMD_EXIT_STOP, "停止所有服务并退出")

        # Track popup menu
        pos = win32gui.GetCursorPos()
        win32gui.SetForegroundWindow(self.hwnd)
        cmd = win32gui.TrackPopupMenu(
            menu,
            win32con.TPM_LEFTALIGN | win32con.TPM_RIGHTBUTTON | win32con.TPM_RETURNCMD | win32con.TPM_NONOTIFY,
            pos[0], pos[1],
            0,
            self.hwnd,
            None,
        )
        win32gui.PostMessage(self.hwnd, win32con.WM_NULL, 0, 0)
        win32gui.DestroyMenu(menu)

        if cmd:
            self.handle_menu_cmd(cmd)

    def handle_menu_cmd(self, cmd: int):
        if cmd == self.CMD_OPEN_WEB:
            webbrowser.open("http://127.0.0.1:8090/mindscada.html")
        elif cmd == self.CMD_OPEN_STREAM:
            webbrowser.open("http://127.0.0.1:8095/stream/view")
        elif cmd == self.CMD_TOGGLE_AUTORESTART:
            self.supervisor.auto_restart = not self.supervisor.auto_restart
            state = "开启" if self.supervisor.auto_restart else "关闭"
            self.show_balloon("自动守护配置", f"已{state}服务崩溃自愈功能。")
        elif cmd == self.CMD_START_ALL:
            threading.Thread(target=self.supervisor.start_all, daemon=True).start()
        elif cmd == self.CMD_STOP_ALL:
            threading.Thread(target=self.supervisor.stop_all, daemon=True).start()
        elif cmd == self.CMD_RESTART_ALL:
            threading.Thread(target=self.supervisor.restart_all, daemon=True).start()
        elif cmd == self.CMD_EXIT_KEEP:
            self.supervisor.running = False
            win32gui.DestroyWindow(self.hwnd)
        elif cmd == self.CMD_EXIT_STOP:
            self.supervisor.running = False
            threading.Thread(target=self._stop_and_exit, daemon=True).start()

    def _stop_and_exit(self):
        self.supervisor.stop_all()
        win32gui.DestroyWindow(self.hwnd)

    def on_command(self, hwnd, msg, wparam, lparam):
        return 0

    def on_destroy(self, hwnd, msg, wparam, lparam):
        if self.tray_installed:
            nid = (self.hwnd, 0)
            try:
                win32gui.Shell_NotifyIcon(win32gui.NIM_DELETE, nid)
            except Exception:
                pass
        win32gui.PostQuitMessage(0)
        return 0


def main():
    logger.info("Initializing MindSCADA Daemon & Tray Supervisor...")
    supervisor = ServiceSupervisor()

    # Initial check and start services that are not yet running
    supervisor.check_all()
    supervisor.start_all()

    app = MindScadaTrayApp(supervisor)

    # Start monitor thread
    monitor_thread = threading.Thread(
        target=supervisor.monitor_loop,
        args=(app.update_tooltip,),
        daemon=True,
    )
    monitor_thread.start()

    # Message loop
    win32gui.PumpMessages()
    logger.info("MindSCADA Daemon & Tray Supervisor exited.")


if __name__ == "__main__":
    main()
