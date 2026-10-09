#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Web Remote Stream Gateway (Phase 2 & 3)
==============================================
High-performance WebSocket & HTTP gateway bridging HTML5 browser clients to
either local Qt Shared Memory (MindSCADA_WB_*) for offscreen bridged windows,
or Sunshine host (112) via the uiasr_stream sidecar.

Features:
- Port: 8095 (HTTP /stream/view + WebSocket /stream/ws)
- Zero external Python network dependencies (pure asyncio + Win32 ctypes + cv2)
- Direct zero-copy QtShm reading for bridged forms: HMI/log, HMI/alarm, HMI/batch, etc.
- Auto-spawns uiasr_stream.exe on demand targeting Sunshine (192.167.8.112) when requested
- Bi-directional: High-speed JPEG stream push & sub-millisecond mouse/key event return
"""

import asyncio
import base64
import ctypes
import hashlib
import json
import logging
import mmap
import os
import struct
import subprocess
import sys
import threading
import time
from urllib.parse import parse_qs, urlparse
import cv2
import numpy as np

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [StreamGW] %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("scada_stream_gateway")

WS_MAGIC = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

# Pipe protocol constants for sidecar
PIPE_MSG_FRAME_READY  = 0x01
PIPE_MSG_CONNECTED    = 0x02
PIPE_MSG_DISCONNECTED = 0x03
PIPE_MSG_STATUS       = 0x04
PIPE_MSG_MOUSE_EVENT  = 0x10
PIPE_MSG_KEY_EVENT    = 0x11
PIPE_MSG_SHUTDOWN     = 0xFF
REMOTE_FRAME_SHM_MAGIC = 0x52535446

DEFAULT_TARGET_HOST = os.environ.get("UIASR_STREAM_HOST", "192.167.8.112")
SIDECAR_EXE = r"Z:\E\opensource\uiasr_framework\demo\addons\uiasr\bin\uiasr_stream.exe"
HTML_PLAYER_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "remote_stream_player.html")

PIPE_ACCESS_DUPLEX = 0x00000003
PIPE_TYPE_BYTE = 0x00000000
PIPE_READMODE_BYTE = 0x00000000
PIPE_WAIT = 0x00000000
k32 = ctypes.windll.kernel32
k32.OpenFileMappingW.argtypes = [ctypes.c_uint32, ctypes.c_bool, ctypes.c_wchar_p]
k32.OpenFileMappingW.restype = ctypes.c_void_p
k32.MapViewOfFile.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_size_t]
k32.MapViewOfFile.restype = ctypes.c_void_p
k32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]
k32.CloseHandle.argtypes = [ctypes.c_void_p]
k32.OpenEventW.argtypes = [ctypes.c_uint32, ctypes.c_bool, ctypes.c_wchar_p]
k32.OpenEventW.restype = ctypes.c_void_p
k32.SetEvent.argtypes = [ctypes.c_void_p]
k32.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
k32.WaitForSingleObject.restype = ctypes.c_uint32


def fnv1a32(text: str) -> int:
    h = 0x811C9DC5
    for b in text.encode("utf-8"):
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


class QtShmBridge:
    """Streams directly from MindSCADA Qt Shared Memory (MindSCADA_WB_{bridge_id})."""

    def __init__(self, form_name: str):
        self.form_name = form_name
        self.bridge_id = f"{fnv1a32(form_name):08X}"
        self.shm_name = f"MindSCADA_WB_{self.bridge_id}"
        self.fr_event_name = f"MindSCADA_FR_{self.bridge_id}"
        self.in_event_name = f"MindSCADA_IN_{self.bridge_id}"

        self.h_shm = None
        self.ptr = None
        self.h_fr_event = None
        self.h_in_event = None

        self.width = 1280
        self.height = 720
        self.max_width = 1280
        self.max_height = 720
        self.fps = 30

        self.is_running = False
        self.frame_listeners = []
        self._lock = threading.Lock()
        self.worker_thread = None
        self.latest_jpeg = None
        self.latest_seq = 0

        # Pre-load cached reference frame if available
        norm = form_name.lower().replace("/", "_")
        for candidate_dir in [
            os.path.join(os.path.dirname(os.path.abspath(__file__)), "assets"),
            r"C:\Users\Administrator\.gemini\antigravity-cli\brain\5917275b-81ec-4582-a183-e0cde3b05214",
        ]:
            cand = os.path.join(candidate_dir, f"{norm}_streamed.jpg")
            if os.path.exists(cand):
                try:
                    with open(cand, "rb") as f:
                        self.latest_jpeg = f.read()
                        logger.info("Preloaded reference frame for %s from %s (%d bytes)", self.form_name, cand, len(self.latest_jpeg))
                        break
                except Exception:
                    pass

    def connect(self) -> bool:
        self.h_shm = k32.OpenFileMappingW(6, False, self.shm_name)
        if not self.h_shm:
            self.h_shm = k32.OpenFileMappingW(4, False, self.shm_name)
        if not self.h_shm:
            return False

        self.ptr = k32.MapViewOfFile(self.h_shm, 6, 0, 0, 0)
        if not self.ptr:
            self.ptr = k32.MapViewOfFile(self.h_shm, 4, 0, 0, 0)
        if not self.ptr:
            k32.CloseHandle(self.h_shm)
            self.h_shm = None
            return False

        self.h_fr_event = k32.OpenEventW(0x00100000 | 0x0002, False, self.fr_event_name)
        self.h_in_event = k32.OpenEventW(0x0002, False, self.in_event_name)

        hdr = ctypes.string_at(self.ptr, 256)
        magic, ver, w, h, mw, mh = struct.unpack("<IHHHHH", hdr[:14])
        if magic == 0x4D534742 and mw > 0 and mh > 0:
            self.width = w if w > 0 else 800
            self.height = h if h > 0 else 600
            self.max_width = mw
            self.max_height = mh
            # Mark QTWINDOW_FLAG_GODOT_READY (bit 1)
            flags_data = ctypes.string_at(self.ptr + 100, 4)
            flags = struct.unpack("<I", flags_data)[0]
            ctypes.memmove(self.ptr + 100, struct.pack("<I", flags | 2), 4)
            logger.info("Connected to QtShm for %s (%s): %dx%d (max %dx%d)", self.form_name, self.bridge_id, self.width, self.height, mw, mh)
            return True
        return False

    def start(self) -> bool:
        with self._lock:
            if self.is_running:
                return True
            if not self.ptr:
                self.connect()
            self.is_running = True
            self.worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
            self.worker_thread.start()
            return True

    def _worker_loop(self):
        logger.info("QtShm worker started for %s", self.form_name)
        last_encode_time = 0
        while self.is_running:
            if not self.ptr:
                if not self.connect():
                    time.sleep(0.1)
                    continue
            try:
                res = 258
                if self.h_fr_event:
                    res = k32.WaitForSingleObject(self.h_fr_event, 33)
                else:
                    time.sleep(0.033)

                hdr = ctypes.string_at(self.ptr, 256)
                magic, ver, w, h, mw, mh = struct.unpack("<IHHHHH", hdr[:14])
                seq = struct.unpack("<I", hdr[20:24])[0]

                if magic != 0x4D534742 or mw <= 0 or mh <= 0:
                    time.sleep(0.05)
                    continue

                cur_w = w if w > 0 else self.width
                cur_h = h if h > 0 else self.height

                now = time.time()
                # Encode immediately if signaled or seq advanced, or every 100ms keepalive
                if res == 0 or seq != self.latest_seq or (now - last_encode_time > 0.1):
                    pix_offset = 4352
                    c_buf = (ctypes.c_uint8 * (mw * mh * 4)).from_address(self.ptr + pix_offset)
                    arr = np.ctypeslib.as_array(c_buf).reshape((mh, mw, 4))
                    cropped = arr[:cur_h, :cur_w]
                    ok, jpg = cv2.imencode(".jpg", cropped, [cv2.IMWRITE_JPEG_QUALITY, 85])
                    if ok:
                        jpg_bytes = jpg.tobytes()
                        self.latest_jpeg = jpg_bytes
                        self.latest_seq = seq
                        self.width = cur_w
                        self.height = cur_h
                        last_encode_time = now
                        for cb in list(self.frame_listeners):
                            try:
                                cb(jpg_bytes, seq, cur_w, cur_h)
                            except Exception:
                                pass
            except Exception as e:
                logger.debug("QtShm read error: %s", e)
                time.sleep(0.05)
        logger.info("QtShm worker ended for %s", self.form_name)

    def send_focus(self, has_focus: bool = True):
        if not self.ptr:
            return
        try:
            pos_data = ctypes.string_at(self.ptr + 92, 4)
            cur_wr = struct.unpack("<I", pos_data)[0]
            pos_rd = ctypes.string_at(self.ptr + 96, 4)
            cur_rd = struct.unpack("<I", pos_rd)[0]
            if cur_wr - cur_rd >= 128:
                return
            slot_idx = cur_wr % 128
            slot_offset = 256 + slot_idx * 32
            now_ms = int(time.time() * 1000) & 0xFFFFFFFF
            ev_type = 7  # QTWINDOW_EV_FOCUS
            slot_data = struct.pack(
                "<BBBB I B 23s",
                ev_type, 0, 0, 0,
                now_ms,
                1 if has_focus else 0,
                b"\x00" * 23,
            )
            ctypes.memmove(self.ptr + slot_offset, slot_data, 32)
            ctypes.memmove(self.ptr + 92, struct.pack("<I", cur_wr + 1), 4)
            if self.h_in_event:
                k32.SetEvent(self.h_in_event)
        except Exception as ex:
            logger.debug("Failed to inject focus to QtShm: %s", ex)

    def send_mouse(self, x: int, y: int, btn: int = 0, pressed: int = 0):
        if not self.ptr:
            return
        try:
            pos_data = ctypes.string_at(self.ptr + 92, 4)
            cur_wr = struct.unpack("<I", pos_data)[0]
            pos_rd = ctypes.string_at(self.ptr + 96, 4)
            cur_rd = struct.unpack("<I", pos_rd)[0]
            if cur_wr - cur_rd >= 128:
                return
            slot_idx = cur_wr % 128
            slot_offset = 256 + slot_idx * 32
            now_ms = int(time.time() * 1000) & 0xFFFFFFFF
            if btn == 0:
                ev_type = 3  # move
                qt_btn = 0
            else:
                ev_type = 1 if pressed else 2
                qt_btn = 1 if btn == 1 else (2 if btn == 2 else (4 if btn == 3 else 0))
            slot_data = struct.pack(
                "<BBBB I hh BB H 16s",
                ev_type, 0, 0, 0,
                now_ms,
                int(x), int(y), int(qt_btn), 0, 0,
                b"\x00" * 16,
            )
            ctypes.memmove(self.ptr + slot_offset, slot_data, 32)
            ctypes.memmove(self.ptr + 92, struct.pack("<I", cur_wr + 1), 4)
            if self.h_in_event:
                k32.SetEvent(self.h_in_event)
        except Exception as ex:
            logger.debug("Failed to inject mouse to QtShm: %s", ex)

    def send_wheel(self, x: int, y: int, delta_y: int):
        if not self.ptr:
            return
        try:
            pos_data = ctypes.string_at(self.ptr + 92, 4)
            cur_wr = struct.unpack("<I", pos_data)[0]
            pos_rd = ctypes.string_at(self.ptr + 96, 4)
            cur_rd = struct.unpack("<I", pos_rd)[0]
            if cur_wr - cur_rd >= 128:
                return
            slot_idx = cur_wr % 128
            slot_offset = 256 + slot_idx * 32
            now_ms = int(time.time() * 1000) & 0xFFFFFFFF
            ev_type = 6  # QTWINDOW_EV_WHEEL
            dy = 120 if delta_y > 0 else -120
            slot_data = struct.pack(
                "<BBBB I hhhh H 14s",
                ev_type, 0, 0, 0,
                now_ms,
                int(x), int(y), 0, int(dy), 0,
                b"\x00" * 14,
            )
            ctypes.memmove(self.ptr + slot_offset, slot_data, 32)
            ctypes.memmove(self.ptr + 92, struct.pack("<I", cur_wr + 1), 4)
            if self.h_in_event:
                k32.SetEvent(self.h_in_event)
        except Exception as ex:
            logger.debug("Failed to inject wheel to QtShm: %s", ex)

    def send_key(self, keycode: int, unicode_val: int, pressed: int):
        if not self.ptr:
            return
        try:
            pos_data = ctypes.string_at(self.ptr + 92, 4)
            cur_wr = struct.unpack("<I", pos_data)[0]
            pos_rd = ctypes.string_at(self.ptr + 96, 4)
            cur_rd = struct.unpack("<I", pos_rd)[0]
            if cur_wr - cur_rd >= 128:
                return
            slot_idx = cur_wr % 128
            slot_offset = 256 + slot_idx * 32
            now_ms = int(time.time() * 1000) & 0xFFFFFFFF
            ev_type = 4 if pressed else 5

            qt_key = keycode
            if keycode == 13: qt_key = 0x01000004       # Return
            elif keycode == 8: qt_key = 0x01000003      # Backspace
            elif keycode == 9: qt_key = 0x01000001      # Tab
            elif keycode == 27: qt_key = 0x01000000     # Escape
            elif keycode == 46: qt_key = 0x01000007     # Delete
            elif keycode == 37: qt_key = 0x01000012     # Left
            elif keycode == 38: qt_key = 0x01000013     # Up
            elif keycode == 39: qt_key = 0x01000014     # Right
            elif keycode == 40: qt_key = 0x01000015     # Down
            elif keycode == 33: qt_key = 0x01000016     # PageUp
            elif keycode == 34: qt_key = 0x01000017     # PageDown
            elif keycode == 36: qt_key = 0x01000010     # Home
            elif keycode == 35: qt_key = 0x01000011     # End
            elif keycode == 16: qt_key = 0x01000020     # Shift
            elif keycode == 17: qt_key = 0x01000021     # Control
            elif keycode == 18: qt_key = 0x01000023     # Alt

            text_bytes = b""
            if unicode_val > 0:
                try:
                    text_bytes = chr(unicode_val).encode("utf-8")[:7]
                except Exception:
                    pass
            text_padded = text_bytes.ljust(8, b"\x00")

            slot_data = struct.pack(
                "<BBBB I II H 2s I 8s",
                ev_type, 0, 0, 0,
                now_ms,
                int(qt_key), 0, 0, b"\x00" * 2,
                int(unicode_val), text_padded,
            )
            ctypes.memmove(self.ptr + slot_offset, slot_data, 32)
            ctypes.memmove(self.ptr + 92, struct.pack("<I", cur_wr + 1), 4)
            if self.h_in_event:
                k32.SetEvent(self.h_in_event)
        except Exception as ex:
            logger.debug("Failed to inject key to QtShm: %s", ex)

    def stop(self):
        self.is_running = False
        if self.worker_thread:
            self.worker_thread.join(timeout=1.0)
            self.worker_thread = None
        if self.ptr:
            k32.UnmapViewOfFile(self.ptr)
            self.ptr = None
        if self.h_shm:
            k32.CloseHandle(self.h_shm)
            self.h_shm = None
        if self.h_fr_event:
            k32.CloseHandle(self.h_fr_event)
            self.h_fr_event = None
        if self.h_in_event:
            k32.CloseHandle(self.h_in_event)
            self.h_in_event = None


ACTIVE_QTSHM_BRIDGES = {}


def get_qtshm_bridge(form_name: str) -> QtShmBridge:
    if form_name not in ACTIVE_QTSHM_BRIDGES:
        b = QtShmBridge(form_name)
        b.start()
        ACTIVE_QTSHM_BRIDGES[form_name] = b
    return ACTIVE_QTSHM_BRIDGES[form_name]


class StreamSidecarBridge:
    """Manages the uiasr_stream sidecar lifecycle and Named Pipe / SHM communication."""

    def __init__(self, host=DEFAULT_TARGET_HOST, width=1280, height=720, fps=30):
        self.host = host
        self.width = width
        self.height = height
        self.fps = fps

        self.shm_name = f"uiasr_shm_gw_{os.getpid()}"
        self.pipe_name = f"uiasr_pipe_gw_{os.getpid()}"
        self.full_pipe = f"\\\\.\\pipe\\{self.pipe_name}"

        self.shm = None
        self.h_pipe = None
        self.proc = None

        self.is_running = False
        self.connected_to_sunshine = False
        self.active_clients = set()
        self.frame_listeners = []
        self._lock = threading.Lock()
        self.latest_jpeg = None
        self.latest_seq = 0

    def start(self):
        with self._lock:
            if self.is_running:
                return True
            self.is_running = True

        shm_size = self.width * self.height * 4 + 32
        self.shm = mmap.mmap(-1, shm_size, self.shm_name)
        self.shm.seek(0)
        self.shm.write(struct.pack("<IIIII", REMOTE_FRAME_SHM_MAGIC, self.width, self.height, 0, 0))

        self.h_pipe = k32.CreateNamedPipeA(
            self.full_pipe.encode("ascii"),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, 65536, 65536, 10000, None
        )
        if self.h_pipe == -1:
            logger.error("Failed to create named pipe: %s", self.full_pipe)
            self.stop()
            return False

        t = threading.Thread(target=self._pipe_worker_loop, daemon=True)
        t.start()
        time.sleep(0.1)

        cmd = [
            SIDECAR_EXE,
            "--pipe", self.pipe_name,
            "--shm", self.shm_name,
            "--host", self.host,
            "--width", str(self.width),
            "--height", str(self.height),
            "--fps", str(self.fps),
        ]
        logger.info("Spawning Sidecar: %s", " ".join(cmd))
        try:
            self.proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=1,
                universal_newlines=True,
            )
            threading.Thread(target=self._log_reader, daemon=True).start()
        except Exception as e:
            logger.error("Failed to launch sidecar exe: %s", e)
            self.stop()
            return False
        return True

    def _log_reader(self):
        if not self.proc: return
        for line in self.proc.stdout:
            line = line.strip()
            if line:
                logger.info("[Sidecar] %s", line)

    def _pipe_worker_loop(self):
        logger.info("Named Pipe waiting for sidecar connection...")
        connected = k32.ConnectNamedPipe(self.h_pipe, None)
        if not connected and k32.GetLastError() != 535: # ERROR_PIPE_CONNECTED
            logger.warning("ConnectNamedPipe returned %d", k32.GetLastError())

        logger.info("Sidecar connected to Named Pipe successfully!")
        self.connected_to_sunshine = True

        msg_buf = bytearray(32)
        bytes_read = ctypes.c_ulong(0)

        while self.is_running:
            success = k32.ReadFile(
                self.h_pipe,
                (ctypes.c_char * 32).from_buffer(msg_buf),
                32,
                ctypes.byref(bytes_read),
                None
            )
            if not success or bytes_read.value == 0:
                logger.warning("Pipe read failed, sidecar disconnected.")
                break

            mtype = msg_buf[0]
            if mtype == PIPE_MSG_FRAME_READY:
                seq, w, h = struct.unpack_from("<III", msg_buf, 4)
                self._handle_frame_ready(seq, w, h)
            elif mtype == PIPE_MSG_CONNECTED:
                logger.info("Sidecar notified: Sunshine Session CONNECTED")
            elif mtype == PIPE_MSG_DISCONNECTED:
                logger.warning("Sidecar notified: Sunshine Session DISCONNECTED")
            elif mtype == PIPE_MSG_STATUS:
                logger.debug("Sidecar status tick")

    def _handle_frame_ready(self, seq, w, h):
        if not self.shm: return
        try:
            buf_size = w * h * 4
            raw_bytes = np.frombuffer(self.shm, dtype=np.uint8, count=buf_size, offset=32)
            arr = raw_bytes.reshape((h, w, 4))
            ok, jpg = cv2.imencode(".jpg", arr, [cv2.IMWRITE_JPEG_QUALITY, 80])
            if not ok: return
            jpg_bytes = jpg.tobytes()
            self.latest_jpeg = jpg_bytes
            self.latest_seq = seq
            for cb in list(self.frame_listeners):
                try: cb(jpg_bytes, seq, w, h)
                except Exception: pass
        except Exception as e:
            logger.error("Error decoding frame from SHM: %s", e)

    def send_mouse(self, x, y, btn=0, pressed=0):
        if not self.h_pipe or not self.connected_to_sunshine: return
        buf = struct.pack("<BBBBHHBB8s", PIPE_MSG_MOUSE_EVENT, 0, 0, 0, int(x), int(y), int(btn), int(pressed), b"\x00"*8)
        bytes_written = ctypes.c_ulong(0)
        k32.WriteFile(self.h_pipe, buf, len(buf), ctypes.byref(bytes_written), None)

    def send_key(self, keycode, unicode_val, pressed):
        if not self.h_pipe or not self.connected_to_sunshine: return
        buf = struct.pack("<BBBBIIB7s", PIPE_MSG_KEY_EVENT, 0, 0, 0, int(keycode), int(unicode_val), int(pressed), b"\x00"*7)
        bytes_written = ctypes.c_ulong(0)
        k32.WriteFile(self.h_pipe, buf, len(buf), ctypes.byref(bytes_written), None)

    def stop(self):
        self.is_running = False
        if self.proc:
            try: self.proc.terminate()
            except Exception: pass
            self.proc = None
        if self.h_pipe:
            k32.CloseHandle(self.h_pipe)
            self.h_pipe = None
        if self.shm:
            try: self.shm.close()
            except Exception: pass
            self.shm = None


SUNSHINE_BRIDGE = StreamSidecarBridge()


class ClientSession:
    def __init__(self, reader, writer, loop):
        self.reader = reader
        self.writer = writer
        self.loop = loop
        self.is_open = True
        self.path = "/"
        self.queue = asyncio.Queue(maxsize=10)

    async def handshake(self):
        raw = b""
        while b"\r\n\r\n" not in raw:
            chunk = await self.reader.read(1024)
            if not chunk: return False
            raw += chunk
            if len(raw) > 65536: return False

        head, _ = raw.split(b"\r\n\r\n", 1)
        lines = head.decode("latin1", errors="ignore").split("\r\n")
        req_line = lines[0].split() if lines else []

        # Handle HTTP /stream/view or /
        if req_line and req_line[0] == "GET":
            req_path = req_line[1] if len(req_line) > 1 else "/"
            self.path = req_path
            path_only = req_path.split("?")[0]
            if path_only in ("/", "/stream/view", "/view"):
                if os.path.exists(HTML_PLAYER_PATH):
                    with open(HTML_PLAYER_PATH, "rb") as f:
                        body = f.read()
                else:
                    body = b"<h1>HTML Player not found</h1>"
                resp = (
                    b"HTTP/1.1 200 OK\r\n"
                    b"Content-Type: text/html; charset=utf-8\r\n"
                    b"Access-Control-Allow-Origin: *\r\n"
                    b"Cross-Origin-Resource-Policy: cross-origin\r\n"
                    b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body
                )
                self.writer.write(resp)
                await self.writer.drain()
                self.writer.close()
                return "http"
            elif path_only == "/health":
                body = json.dumps({"status": "running", "host": SUNSHINE_BRIDGE.host, "active": SUNSHINE_BRIDGE.is_running}).encode("utf-8")
                resp = (
                    b"HTTP/1.1 200 OK\r\n"
                    b"Content-Type: application/json; charset=utf-8\r\n"
                    b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body
                )
                self.writer.write(resp)
                await self.writer.drain()
                self.writer.close()
                return "http"

        if req_line and len(req_line) > 1:
            self.path = req_line[1]

        # WebSocket Upgrade
        ws_key = None
        for line in lines:
            if ":" in line:
                k, v = line.split(":", 1)
                if k.strip().lower() == "sec-websocket-key":
                    ws_key = v.strip()
                    break

        if not ws_key:
            self.writer.write(b"HTTP/1.1 400 Bad Request\r\n\r\n")
            await self.writer.drain()
            self.writer.close()
            return False

        accept_hash = base64.b64encode(hashlib.sha1(ws_key.encode("utf-8") + WS_MAGIC).digest()).decode("latin1")
        resp = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept_hash}\r\n\r\n"
        )
        self.writer.write(resp.encode("latin1"))
        await self.writer.drain()
        return True

    async def send_frame_bytes(self, payload):
        if not self.is_open: return
        length = len(payload)
        header = bytearray([0x82]) # binary opcode 2
        if length <= 125:
            header.append(length)
        elif length <= 65535:
            header.append(126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(127)
            header.extend(struct.pack("!Q", length))
        try:
            self.writer.write(header + payload)
            await self.writer.drain()
        except Exception:
            self.is_open = False

    async def send_text(self, text):
        if not self.is_open: return
        payload = text.encode("utf-8")
        length = len(payload)
        header = bytearray([0x81]) # text opcode 1
        if length <= 125:
            header.append(length)
        elif length <= 65535:
            header.append(126)
            header.extend(struct.pack("!H", length))
        else:
            header.append(127)
            header.extend(struct.pack("!Q", length))
        try:
            self.writer.write(header + payload)
            await self.writer.drain()
        except Exception:
            self.is_open = False

    async def recv_frame(self):
        try:
            head = await self.reader.readexactly(2)
        except Exception:
            return None, None
        b1, b2 = head[0], head[1]
        opcode = b1 & 0x0F
        masked = (b2 & 0x80) != 0
        payload_len = b2 & 0x7F

        if payload_len == 126:
            ext = await self.reader.readexactly(2)
            payload_len = struct.unpack("!H", ext)[0]
        elif payload_len == 127:
            ext = await self.reader.readexactly(8)
            payload_len = struct.unpack("!Q", ext)[0]

        mask = None
        if masked:
            mask = await self.reader.readexactly(4)

        payload = await self.reader.readexactly(payload_len)
        if masked:
            payload = bytearray(payload[i] ^ mask[i % 4] for i in range(payload_len))
        return opcode, bytes(payload)


async def handle_client(reader, writer):
    loop = asyncio.get_running_loop()
    client = ClientSession(reader, writer, loop)
    res = await client.handshake()
    if res != True:
        return

    # Determine bridge target: QtShm (if form specified) or Sunshine
    url_parts = client.path.split("?")
    params = {}
    if len(url_parts) > 1:
        params = parse_qs(url_parts[1])
    form_param = params.get("form", [None])[0]

    bridge = None
    source_name = "Sunshine (112)"
    if form_param:
        qt_b = get_qtshm_bridge(form_param)
        if qt_b:
            bridge = qt_b
            source_name = f"QtShm ({form_param})"
            logger.info("Client connected to QtShm stream for %s", form_param)

    if not bridge:
        bridge = SUNSHINE_BRIDGE
        bridge.start()
        logger.info("Client connected to Sunshine Sidecar stream")

    # Send initial meta
    await client.send_text(json.dumps({
        "type": "meta",
        "width": bridge.width,
        "height": bridge.height,
        "fps": bridge.fps,
        "source": source_name,
    }))

    # If bridge already has a frame, send immediately
    if bridge.latest_jpeg:
        await client.send_frame_bytes(bridge.latest_jpeg)

    # Send initial focus to activate Qt window
    if hasattr(bridge, "send_focus"):
        bridge.send_focus(True)

    # Frame callback for this client
    def on_frame(jpg_bytes, seq, w, h):
        if not client.is_open: return
        def _enqueue():
            if not client.is_open: return
            try:
                client.queue.put_nowait(jpg_bytes)
            except asyncio.QueueFull:
                try: client.queue.get_nowait()
                except Exception: pass
                try: client.queue.put_nowait(jpg_bytes)
                except Exception: pass
        client.loop.call_soon_threadsafe(_enqueue)

    bridge.frame_listeners.append(on_frame)

    async def push_loop():
        while client.is_open:
            try:
                frame_data = await asyncio.wait_for(client.queue.get(), timeout=1.0)
                await client.send_frame_bytes(frame_data)
            except asyncio.TimeoutError:
                continue
            except Exception:
                break

    push_task = asyncio.create_task(push_loop())

    try:
        while client.is_open:
            opcode, payload = await client.recv_frame()
            if opcode is None or opcode == 0x08:  # close
                break
            if opcode == 0x01:  # text message
                try:
                    msg = json.loads(payload.decode("utf-8"))
                    mtype = msg.get("type")
                    if mtype == "ping":
                        await client.send_text(json.dumps({"type": "pong", "t": msg.get("t", 0)}))
                    elif mtype == "mouse_move":
                        bridge.send_mouse(msg.get("x", 0), msg.get("y", 0), 0, 0)
                    elif mtype == "mouse_btn":
                        bridge.send_mouse(msg.get("x", 0), msg.get("y", 0), msg.get("btn", 1), msg.get("pressed", 1))
                    elif mtype == "mouse_wheel":
                        btn = msg.get("btn", 4)
                        dy = 120 if btn == 4 else -120
                        if hasattr(bridge, "send_wheel"):
                            bridge.send_wheel(msg.get("x", 0), msg.get("y", 0), dy)
                        else:
                            bridge.send_mouse(msg.get("x", 0), msg.get("y", 0), btn, 1)
                    elif mtype == "focus":
                        if hasattr(bridge, "send_focus"):
                            bridge.send_focus(bool(msg.get("focus", True)))
                    elif mtype == "key":
                        bridge.send_key(msg.get("keycode", 0), msg.get("unicode", 0), msg.get("pressed", 0))
                except Exception as ex:
                    logger.debug("Failed to handle client msg: %s", ex)
    finally:
        client.is_open = False
        push_task.cancel()
        if on_frame in bridge.frame_listeners:
            bridge.frame_listeners.remove(on_frame)
        try:
            writer.close()
        except Exception:
            pass
        logger.info("Web client disconnected from stream WebSocket.")


async def main():
    port = int(os.environ.get("SCADA_STREAM_PORT", "8095"))
    host = os.environ.get("SCADA_STREAM_BIND", "127.0.0.1")
    server = await asyncio.start_server(handle_client, host, port)
    logger.info("==========================================================")
    logger.info(" MindSCADA Web Remote Stream Gateway listening on %s:%d", host, port)
    logger.info(" Web Player URL: http://%s:%d/stream/view", host, port)
    logger.info(" WebSocket Endpoint: ws://%s:%d/stream/ws", host, port)
    logger.info(" Target Sunshine Host: %s", DEFAULT_TARGET_HOST)
    logger.info("==========================================================")

    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Exiting on Ctrl+C...")
    finally:
        SUNSHINE_BRIDGE.stop()
        for b in ACTIVE_QTSHM_BRIDGES.values():
            b.stop()
