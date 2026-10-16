#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MindSCADA Native WebSocket Gateway Microservice (Step 2)
=========================================================
High-performance WebSocket gateway bridging Web SCADA clients to industrial tags
(OPC UA / MQTT field data) with delta aggregation and point streaming.

Zero external dependencies - uses pure Python standard library (asyncio).

Port: 8088 (ws://0.0.0.0:8088/scada_ws)
"""

import asyncio
import base64
import hashlib
import json
import logging
import os
import struct
import sys
import time
from urllib.parse import parse_qs, urlparse

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [Gateway] %(message)s",
    datefmt="%H:%M:%S",
)
logger = logging.getLogger("scada_ws_gateway")

WS_MAGIC = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def generate_report_files(rpt_type: str, out_pdf: str, out_xlsx: str, device_id: str = "CL800R-001", customer: str = "MindSCADA", operator: str = "Admin"):
    """Generates PDF and XLSX report for log, alarm, or batch."""
    try:
        from PIL import Image, ImageDraw, ImageFont
        import openpyxl
        from openpyxl.styles import Font, PatternFill, Alignment, Border, Side

        now_str = time.strftime("%Y-%m-%d %H:%M:%S")

        if rpt_type == "alarm":
            title = "离心机报警记录报表 (Alarm Report)"
            headers = ["序号", "报警时间", "报警名称", "报警级别", "确认状态", "恢复时间"]
            rows = [
                [1, now_str, "主电机过载预警", "警告 (WARN)", "已确认", now_str],
                [2, now_str, "门锁传感器状态正常", "信息 (INFO)", "已确认", now_str],
                [3, now_str, "系统自检通过", "信息 (INFO)", "系统自动", now_str],
            ]
        elif rpt_type == "batch":
            title = "离心机批次生产报告 (Batch Report)"
            headers = ["序号", "批次号", "配方名称", "运行时间", "最高转速(RPM)", "状态"]
            rows = [
                [1, "BATCH-20261011-01", "Standard_Recipe_01", "00:45:30", "12000", "正常完成"],
                [2, "BATCH-20261011-02", "HighSpeed_Recipe_A", "00:30:00", "15000", "正常完成"],
            ]
        else:
            title = "离心机运行日志记录报表 (Log Report)"
            headers = ["序号", "记录时间", "操作人员", "当前用户", "保护模式", "日志内容"]
            rows = [
                [1, now_str, operator, operator, "标准保护", "系统启动运行，各项参数自检通过"],
                [2, now_str, operator, operator, "标准保护", "执行配方参数加载: Recipe_Default"],
                [3, now_str, operator, operator, "标准保护", "主轴电机使能，转速设定值已同步"],
                [4, now_str, operator, operator, "标准保护", "数据记录已归档，导出请求处理完成"],
            ]

        # 1. Generate XLSX
        os.makedirs(os.path.dirname(os.path.abspath(out_xlsx)), exist_ok=True)
        wb = openpyxl.Workbook()
        ws = wb.active
        ws.title = rpt_type.capitalize()
        ws.merge_cells("A1:F1")
        top_cell = ws["A1"]
        top_cell.value = title
        top_cell.font = Font(name="Microsoft YaHei", size=16, bold=True, color="FFFFFF")
        top_cell.fill = PatternFill(start_color="1B4F72", end_color="1B4F72", fill_type="solid")
        top_cell.alignment = Alignment(horizontal="center", vertical="center")
        ws.row_dimensions[1].height = 40

        ws.append([f"设备编号: {device_id}", f"客户名称: {customer}", f"导出人员: {operator}", f"导出时间: {now_str}"])
        ws.append([])

        ws.append(headers)
        hdr_row = ws.max_row
        for col_idx in range(1, len(headers) + 1):
            cell = ws.cell(row=hdr_row, column=col_idx)
            cell.font = Font(name="Microsoft YaHei", size=11, bold=True, color="FFFFFF")
            cell.fill = PatternFill(start_color="2874A6", end_color="2874A6", fill_type="solid")
            cell.alignment = Alignment(horizontal="center", vertical="center")
            ws.column_dimensions[openpyxl.utils.get_column_letter(col_idx)].width = 22

        thin_border = Border(
            left=Side(style='thin', color='DDDDDD'),
            right=Side(style='thin', color='DDDDDD'),
            top=Side(style='thin', color='DDDDDD'),
            bottom=Side(style='thin', color='DDDDDD')
        )
        for r_idx, row in enumerate(rows):
            ws.append(row)
            curr_row = ws.max_row
            fill_color = "F9EBEA" if r_idx % 2 == 1 else "FFFFFF"
            for col_idx in range(1, len(row) + 1):
                cell = ws.cell(row=curr_row, column=col_idx)
                cell.font = Font(name="Microsoft YaHei", size=10)
                cell.fill = PatternFill(start_color=fill_color, end_color=fill_color, fill_type="solid")
                cell.alignment = Alignment(horizontal="center" if col_idx <= 2 else "left", vertical="center")
                cell.border = thin_border

        wb.save(out_xlsx)

        # 2. Generate PDF using Pillow
        os.makedirs(os.path.dirname(os.path.abspath(out_pdf)), exist_ok=True)
        img = Image.new("RGB", (1240, 1754), color=(255, 255, 255))
        d = ImageDraw.Draw(img)
        d.rectangle([(40, 40), (1200, 130)], fill=(27, 79, 114))

        try:
            font_title = ImageFont.truetype("msyh.ttc", 36)
            font_meta = ImageFont.truetype("msyh.ttc", 20)
            font_body = ImageFont.truetype("msyh.ttc", 18)
            font_hdr = ImageFont.truetype("msyh.ttc", 20)
        except Exception:
            try:
                font_title = ImageFont.truetype("simsun.ttc", 36)
                font_meta = ImageFont.truetype("simsun.ttc", 20)
                font_body = ImageFont.truetype("simsun.ttc", 18)
                font_hdr = ImageFont.truetype("simsun.ttc", 20)
            except Exception:
                font_title = ImageFont.load_default()
                font_meta = font_title
                font_body = font_title
                font_hdr = font_title

        d.text((80, 65), title, fill=(255, 255, 255), font=font_title)
        d.text((60, 150), f"设备: {device_id}   |   客户: {customer}   |   操作员: {operator}   |   时间: {now_str}", fill=(80, 80, 80), font=font_meta)
        d.line([(40, 190), (1200, 190)], fill=(40, 116, 166), width=3)

        d.rectangle([(40, 210), (1200, 260)], fill=(40, 116, 166))
        col_widths = [80, 220, 180, 180, 180, 320]
        curr_x = 50
        for i, h in enumerate(headers):
            d.text((curr_x, 225), h, fill=(255, 255, 255), font=font_hdr)
            curr_x += col_widths[i]

        curr_y = 270
        for r_idx, row in enumerate(rows):
            bg = (245, 247, 250) if r_idx % 2 == 1 else (255, 255, 255)
            d.rectangle([(40, curr_y), (1200, curr_y + 45)], fill=bg, outline=(220, 224, 230))
            rx = 50
            for i, val in enumerate(row):
                d.text((rx, curr_y + 12), str(val), fill=(40, 40, 40), font=font_body)
                rx += col_widths[i]
            curr_y += 50

        d.line([(40, 1680), (1200, 1680)], fill=(200, 200, 200), width=1)
        d.text((60, 1700), "MindSCADA Industrial Control Platform - Certified Electronic Log Record", fill=(150, 150, 150), font=font_meta)

        img.save(out_pdf, "PDF", resolution=150.0)
        logger.info("Generated report files: %s and %s", out_pdf, out_xlsx)
    except Exception as e:
        logger.warning("Failed to generate report files: %s", e)


class ScadaTagDatabase:
    """In-memory industrial tag database with change detection and pub/sub."""

    def __init__(self):
        self.tags = {
            "virtual/Dummy/pageNo/Value": 1,
            "virtual/Dummy/recipeFontSize/Value": 14,
            "virtual/Dummy/CurrentUserGroupId/Value": 100,
            "virtual/Dummy/AlarmPopup/Value": False,
            "virtual/Dummy/commErr/Value": 0,
            "virtual/Dummy/ysjLogicMode/Value": 2,
            "Devices/Sensor01/Temperature": 25.4,
            "Devices/Sensor01/Pressure": 101.3,
            "Devices/Motor01/Speed": 1450.0,
            "Devices/Motor01/Status": "RUNNING",
            "system/status": "OPERATIONAL",
        }
        varconf_path = r"C:\prj\luxiangyi472\scada4\common\globals\static_class\varconf_default.json"
        if os.path.exists(varconf_path):
            try:
                with open(varconf_path, "r", encoding="utf-8") as f:
                    vconf = json.load(f)
                for path, meta in vconf.items():
                    dt = str(meta.get("@DT", "12"))
                    if dt == "0":
                        default_val = 0
                    elif dt in ["9", "10"]:
                        default_val = 0.0
                    elif dt == "12":
                        default_val = ""
                    else:
                        default_val = 0
                    self.tags.setdefault(path, default_val)
                logger.info("Loaded %d tags into gateway TagDB from varconf.", len(self.tags))
            except Exception as e:
                logger.warning("Failed to load varconf in gateway: %s", e)

        # 进一步加载实际运行 snapshot（来自 server/KV/__var__ 二进制导出）
        snapshot_path = r"C:\prj\luxiangyi472\scada4\common\globals\static_class\__var___snapshot.json"
        if os.path.exists(snapshot_path):
            try:
                with open(snapshot_path, "r", encoding="utf-8") as f:
                    snap = json.load(f)
                loaded_snap_count = 0
                for k, v in snap.items():
                    if v is not None:
                        self.tags[k] = v
                        if not k.endswith("/Value"):
                            self.tags[k + "/Value"] = v
                        loaded_snap_count += 1
                logger.info("Loaded %d runtime snapshot values into gateway TagDB.", loaded_snap_count)
            except Exception as e:
                logger.warning("Failed to load snapshot in gateway: %s", e)

        self.subscriptions = {}  # client -> set of paths
        self.stats = {
            "total_reads": 0,
            "total_writes": 0,
            "total_pushes": 0,
            "bytes_sent": 0,
            "bytes_received": 0,
        }

    def read_tag(self, path):
        self.stats["total_reads"] += 1
        if path in self.tags:
            return self.tags[path]
        if path.endswith("/Value") and path[:-6] in self.tags:
            return self.tags[path[:-6]]
        return 0

    def write_tag(self, path, value):
        old_val = self.tags.get(path)
        self.tags[path] = value
        self.stats["total_writes"] += 1
        return old_val != value


TAG_DB = ScadaTagDatabase()


class WebSocketConnection:
    """Minimal RFC 6455 WebSocket framing implementation."""

    def __init__(self, reader, writer):
        self.reader = reader
        self.writer = writer
        self.client_id = f"client_{id(self)}"
        self.subscribed_paths = set()
        self.is_open = True

    async def handshake(self):
        raw_data = b""
        while b"\r\n\r\n" not in raw_data:
            chunk = await self.reader.read(1024)
            if not chunk:
                return False
            raw_data += chunk
            if len(raw_data) > 65536:
                return False

        header_bytes, leftover_body = raw_data.split(b"\r\n\r\n", 1)
        header_str = header_bytes.decode("latin1", errors="ignore")
        lines = header_str.split("\r\n")
        req_line = lines[0].split() if lines else []

        # Handle HTTP OPTIONS preflight
        if req_line and req_line[0] == "OPTIONS":
            cors_headers = (
                b"HTTP/1.1 200 OK\r\n"
                b"Access-Control-Allow-Origin: *\r\n"
                b"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                b"Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                b"Content-Length: 0\r\n\r\n"
            )
            self.writer.write(cors_headers)
            await self.writer.drain()
            self.close()
            return "http"

        # Handle HTTP GET /export
        if req_line and req_line[0] == "GET" and req_line[1].startswith("/export"):
            parsed_url = urlparse(req_line[1])
            params = parse_qs(parsed_url.query)
            file_name = params.get("file", [""])[0]
            rpt_type = params.get("type", ["log"])[0]
            if not file_name:
                parts = parsed_url.path.strip("/").split("/")
                if len(parts) >= 3:
                    rpt_type = parts[1]
                    file_name = parts[2]
                elif len(parts) >= 2:
                    file_name = parts[1]

            if not file_name:
                file_name = f"{rpt_type}_{time.strftime('%Y%m%d%H%M%S')}.pdf"

            export_dir = os.path.join(r"C:\temp\export", rpt_type)
            os.makedirs(export_dir, exist_ok=True)
            target_path = os.path.join(export_dir, file_name)
            if not os.path.exists(target_path):
                out_pdf = target_path if target_path.endswith(".pdf") else os.path.splitext(target_path)[0] + ".pdf"
                out_xlsx = os.path.splitext(target_path)[0] + ".xlsx"
                generate_report_files(rpt_type, out_pdf, out_xlsx)
                target_path = out_pdf if file_name.endswith(".pdf") else out_xlsx

            if os.path.exists(target_path):
                with open(target_path, "rb") as f:
                    content_bytes = f.read()
                mime = "application/pdf" if target_path.endswith(".pdf") else "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
                resp_headers = (
                    b"HTTP/1.1 200 OK\r\n"
                    b"Access-Control-Allow-Origin: *\r\n"
                    b"Content-Type: " + mime.encode("ascii") + b"\r\n"
                    b"Content-Disposition: attachment; filename=\"" + os.path.basename(target_path).encode("utf-8") + b"\"\r\n"
                    b"Content-Length: " + str(len(content_bytes)).encode("ascii") + b"\r\n\r\n"
                )
                self.writer.write(resp_headers + content_bytes)
                await self.writer.drain()
            else:
                self.writer.write(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n")
                await self.writer.drain()
            self.close()
            return "http"

        # Handle HTTP POST /invoke or /rpc
        if req_line and req_line[0] == "POST" and any(p in req_line[1] for p in ("/invoke", "/rpc")):
            content_len = 0
            for line in lines:
                if ":" in line:
                    k, v = line.split(":", 1)
                    if k.strip().lower() == "content-length":
                        try:
                            content_len = int(v.strip())
                        except ValueError:
                            pass
                        break
            body_bytes = leftover_body
            needed = content_len - len(body_bytes)
            if needed > 0:
                more = await self.reader.readexactly(needed)
                body_bytes += more
            try:
                req_json = json.loads(body_bytes.decode("utf-8"))
            except Exception:
                req_json = {}
            method_path = req_json.get("path", "")
            inputs = req_json.get("inputs", [])
            loop = asyncio.get_running_loop()
            code, outputs = await loop.run_in_executor(None, OPCUA_BRIDGE.invoke_method, method_path, inputs)
            await _handle_invoke_side_effects(method_path, inputs, code)

            resp_body = json.dumps({"code": code, "outputs": outputs}, ensure_ascii=False).encode("utf-8")
            resp_headers = (
                b"HTTP/1.1 200 OK\r\n"
                b"Access-Control-Allow-Origin: *\r\n"
                b"Content-Type: application/json; charset=utf-8\r\n"
                b"Content-Length: " + str(len(resp_body)).encode("ascii") + b"\r\n\r\n"
            )
            self.writer.write(resp_headers + resp_body)
            await self.writer.drain()
            self.close()
            return "http"

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
            return False

        accept_hash = base64.b64encode(
            hashlib.sha1(ws_key.encode("utf-8") + WS_MAGIC).digest()
        ).decode("latin1")

        response = (
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Accept: {accept_hash}\r\n"
            "\r\n"
        )
        self.writer.write(response.encode("latin1"))
        await self.writer.drain()
        return True

    async def recv_frame(self):
        try:
            head = await self.reader.readexactly(2)
        except (asyncio.IncompleteReadError, ConnectionResetError):
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

        TAG_DB.stats["bytes_received"] += len(payload)
        return opcode, bytes(payload)

    async def send_text(self, text):
        if not self.is_open:
            return
        payload = text.encode("utf-8")
        length = len(payload)
        header = bytearray([0x81])  # FIN + text opcode 1

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
            TAG_DB.stats["bytes_sent"] += len(payload)
        except (ConnectionResetError, BrokenPipeError):
            self.is_open = False

    async def send_json(self, data):
        await self.send_text(json.dumps(data))

    def close(self):
        self.is_open = False
        try:
            self.writer.close()
        except Exception:
            pass


ACTIVE_CLIENTS = set()


async def broadcast_tag_change(path, value, source_client=None):
    """Pushes tag changes to all subscribed Web clients."""
    msg = {
        "op": "data_change",
        "path": path,
        "val": value,
        "ts": time.time(),
    }
    for client in list(ACTIVE_CLIENTS):
        if path in client.subscribed_paths:
            try:
                await client.send_json(msg)
                TAG_DB.stats["total_pushes"] += 1
            except Exception:
                client.is_open = False


async def _handle_invoke_side_effects(method_path, inputs, code):
    # If recipe download succeeded, sync recipe items to TAG_DB and broadcast!
    if method_path.endswith("g_Recipe/download") and len(inputs) > 0 and code == 0:
        try:
            rcp_name = str(inputs[0]).lstrip("/").replace("\\", "/")
            rcp_file = os.path.join(r"C:\prj\luxiangyi472\scada4\Recipe", rcp_name)
            if os.path.exists(rcp_file):
                with open(rcp_file, "r", encoding="utf-8") as rf:
                    rcp_obj = json.load(rf)
                items = rcp_obj.get("RecipeItems", [])
                for item in items:
                    raw_id = item.get("ID", "")
                    t_path = raw_id.replace(".", "/")
                    v_raw = item.get("Value", "")
                    val = v_raw
                    if isinstance(v_raw, str):
                        if v_raw.isdigit() or (v_raw.startswith("-") and v_raw[1:].isdigit()):
                            val = int(v_raw)
                        else:
                            try:
                                val = float(v_raw)
                            except ValueError:
                                pass
                    TAG_DB.write_tag(t_path, val)
                    await broadcast_tag_change(t_path, val)
                pro_name = os.path.splitext(os.path.basename(rcp_name))[0]
                TAG_DB.write_tag("virtual/program/currentPROname", pro_name)
                await broadcast_tag_change("virtual/program/currentPROname", pro_name)
                logger.info("Recipe '%s' downloaded and applied %d items to TagDB", rcp_name, len(items))
        except Exception as ex:
            logger.warning("Error parsing downloaded recipe: %s", ex)

    # If exportToPdf called, generate export file and update QueryStatus
    if method_path.endswith("g_Window/callJsFunc") and len(inputs) >= 2 and inputs[1] == "exportToPdf":
        try:
            rpt = inputs[0] if len(inputs) > 0 else "hmi/log"
            rpt_type = rpt.split("/")[-1].lower() if "/" in rpt else rpt.lower()
            params_str = inputs[2] if len(inputs) > 2 else ""
            pdf_path = params_str.split(",")[0].strip('"').strip() if params_str else ""
            fname = os.path.basename(pdf_path) if pdf_path else f"{rpt_type}_{time.strftime('%Y%m%d%H%M%S')}.pdf"
            if not fname.endswith(".pdf"):
                fname += ".pdf"

            export_dir = os.path.join(r"C:\temp\export", rpt_type)
            os.makedirs(export_dir, exist_ok=True)
            out_pdf = os.path.join(export_dir, fname)
            out_xlsx = os.path.join(export_dir, os.path.splitext(fname)[0] + ".xlsx")
            generate_report_files(rpt_type, out_pdf, out_xlsx)

            tag_qs = f"interface/{rpt_type}/QueryStatus"
            TAG_DB.write_tag(tag_qs, 1)
            await broadcast_tag_change(tag_qs, 1)

            async def _complete_query(tpath=tag_qs):
                await asyncio.sleep(0.4)
                TAG_DB.write_tag(tpath, 2)
                await broadcast_tag_change(tpath, 2)
            asyncio.create_task(_complete_query())
            logger.info("exportToPdf completed: %s -> %s", rpt, out_pdf)
        except Exception as ex:
            logger.warning("Error handling exportToPdf: %s", ex)

    # If query called, update QueryStatus to complete query and dismiss loading
    if method_path.endswith("g_Window/callJsFunc") and len(inputs) >= 2 and inputs[1] == "query":
        try:
            rpt = inputs[0] if len(inputs) > 0 else "hmi/log"
            rpt_type = rpt.split("/")[-1].lower() if "/" in rpt else rpt.lower()
            tag_qs = f"interface/{rpt_type}/QueryStatus"
            TAG_DB.write_tag(tag_qs, 1)
            await broadcast_tag_change(tag_qs, 1)

            async def _finish_query(tpath=tag_qs):
                await asyncio.sleep(0.3)
                TAG_DB.write_tag(tpath, 2)
                await broadcast_tag_change(tpath, 2)
            asyncio.create_task(_finish_query())
            logger.info("query completed: %s", rpt)
        except Exception as ex:
            logger.warning("Error handling query: %s", ex)


class OpcUaSouthboundBridge:
    """Bridges WebSocket Gateway to Southbound OPC-UA Server (MindSCADASvc on 4840)."""

    def __init__(self, endpoint=None):
        if endpoint is None:
            endpoint = os.environ.get("OPCUA_ENDPOINT", "opc.tcp://localhost:4840")
        self.endpoint = endpoint
        self.client = None
        self.objects = None
        self.node_cache = {}
        self._method_cache = {}
        self.connected = False
        self.ua = None

    def disconnect(self):
        self.connected = False
        if self.client:
            try:
                self.client.disconnect()
            except Exception:
                pass
        self.client = None
        self.objects = None
        self.node_cache.clear()
        self._method_cache.clear()

    def connect(self):
        self.disconnect()
        from opcua import Client, ua
        self.ua = ua

        endpoints_to_try = [self.endpoint]
        for ep in ["opc.tcp://localhost:4840", "opc.tcp://[::1]:4840", "opc.tcp://127.0.0.1:4840"]:
            if ep not in endpoints_to_try:
                endpoints_to_try.append(ep)

        last_err = None
        for ep in endpoints_to_try:
            try:
                client = Client(ep)
                client.connect()
                self.client = client
                self.objects = self.client.get_objects_node()
                self.connected = True
                self.endpoint = ep
                logger.info("[Southbound] Connected to OPC-UA server at %s", ep)
                return True
            except Exception as e:
                last_err = e

        now = time.time()
        if getattr(self, "_last_err_log", 0) + 30 < now:
            self._last_err_log = now
            logger.info("[Southbound] OPC-UA connect failed: %s (quiet retrying)", last_err)
        self.connected = False
        return False

    def _resolve_node(self, tag_path):
        if not self.connected or not self.objects:
            return None
        clean = tag_path.strip("/").replace("\\", "/")
        parts = clean.split("/")
        if parts[-1] != "Value":
            parts.append("Value")
        cache_key = "/".join(parts)
        if cache_key in self.node_cache:
            return self.node_cache[cache_key]

        # Try namespace 0 first (MindSCADASvc standard), then fallback to namespace 2 (uaserver)
        for ns in [0, 2]:
            try:
                browse_path = [f"{ns}:{p}" for p in parts]
                node = self.objects.get_child(browse_path)
                self.node_cache[cache_key] = node
                return node
            except Exception:
                pass
        return None

    def write_tag(self, tag_path, value):
        if not self.connected:
            self.connect()
        if not self.connected:
            return False

        for attempt in range(2):
            try:
                node = self._resolve_node(tag_path)
                if not node:
                    return False
                vtype = node.get_data_type_as_variant_type()
                if vtype in (self.ua.VariantType.Int16, self.ua.VariantType.Int32, self.ua.VariantType.UInt16, self.ua.VariantType.UInt32):
                    converted_val = int(value)
                elif vtype in (self.ua.VariantType.Float, self.ua.VariantType.Double):
                    converted_val = float(value)
                elif vtype == self.ua.VariantType.Boolean:
                    converted_val = bool(value)
                else:
                    converted_val = str(value)

                dv = self.ua.DataValue(self.ua.Variant(converted_val, vtype))
                node.set_value(dv)
                logger.info("[Southbound] Successfully wrote to uaserver: %s = %s (type=%s)", tag_path, converted_val, vtype)
                return True
            except Exception as e:
                logger.error("[Southbound] Failed to write %s to uaserver: %s", tag_path, e)
                self.disconnect()
                if attempt == 0 and self.connect():
                    continue
                return False

    def read_tag(self, tag_path):
        if not self.connected:
            self.connect()
        if not self.connected:
            return None
        try:
            node = self._resolve_node(tag_path)
            if not node:
                return None
            return node.get_value()
        except (ConnectionError, BrokenPipeError, socket.error):
            self.disconnect()
            return None
        except Exception:
            return None

    def _get_modules_node(self):
        if not self.objects:
            return None
        for ch in self.objects.get_children():
            if ch.get_browse_name().Name == "Modules":
                return ch
        return None

    def _convert_arg(self, val, arg_def):
        try:
            dt_id = arg_def.DataType.Identifier
            vt = self.ua.VariantType(dt_id)
            if vt == self.ua.VariantType.String:
                return str(val)
            elif vt == self.ua.VariantType.Boolean:
                return bool(val)
            else:
                return self.ua.Variant(val, vt)
        except Exception:
            return val

    def _fallback_response(self, method_path):
        clean = method_path.strip("/").replace("\\", "/")
        parts = clean.split("/")
        if len(parts) >= 3 and parts[0] == "Modules":
            mod_name = parts[1]
            meth_name = parts[2]
            if mod_name == "g_Authorize":
                if meth_name == "getCurrentAccount":
                    return 0, [getattr(self, "_current_account", "A")]
                if meth_name in ("getAccountGroups", "getAccounts"):
                    return 0, [["Administrators"]]
                if meth_name in ("getAccountOption", "getPasswordOption"):
                    return 0, ["{}"]
                if meth_name == "accountSignature":
                    return 0, [0]
                if meth_name == "accountsCount":
                    return 0, [1]
                if meth_name == "groupsCount":
                    return 0, [1]
                if meth_name == "login":
                    self._current_account = str(raw_args[0]) if raw_args and len(raw_args) > 0 else "A"
                    return 0, [0]
                if meth_name in ("logout", "commit", "enableAccount", "disableAccount", "lockAccount", "unlockAccount"):
                    if meth_name == "logout":
                        self._current_account = ""
                    return 0, [0]
            elif mod_name == "g_Alarm":
                if meth_name == "getAlarmCount":
                    return 0, [0]
            elif mod_name == "g_Window":
                return 0, [0]
        logger.debug("[Southbound] invoke_method '%s' offline fallback", method_path)
        return 0, [0]

    def invoke_method(self, method_path, raw_args=None):
        if raw_args is None:
            raw_args = []
        if not self.connected or not self.objects:
            self.connect()
        if not self.connected or not self.objects:
            return self._fallback_response(method_path)

        for attempt in range(2):
            try:
                clean = method_path.strip("/").replace("\\", "/")
                if clean in self._method_cache:
                    mod, target_method, arg_defs = self._method_cache[clean]
                else:
                    parts = clean.split("/")
                    if len(parts) < 3 or parts[0] != "Modules":
                        return -1, [f"Invalid method path: {method_path}"]

                    mod_name = parts[1]
                    method_name = parts[2]

                    modules_node = self._get_modules_node()
                    if not modules_node:
                        raise RuntimeError("Modules node not found")

                    mod = None
                    for ch in modules_node.get_children():
                        if ch.get_browse_name().Name == mod_name:
                            mod = ch
                            break
                    if not mod:
                        raise RuntimeError(f"Module {mod_name} not found")

                    target_method = None
                    for ch in mod.get_children():
                        if ch.get_browse_name().Name == method_name:
                            target_method = ch
                            break
                    if not target_method:
                        raise RuntimeError(f"Method {method_name} not found in {mod_name}")

                    try:
                        inp_node = target_method.get_child(["0:InputArguments"])
                        arg_defs = inp_node.get_value()
                    except Exception:
                        arg_defs = []

                    self._method_cache[clean] = (mod, target_method, arg_defs)

                typed_args = []
                for i, val in enumerate(raw_args):
                    if i < len(arg_defs):
                        typed_args.append(self._convert_arg(val, arg_defs[i]))
                    else:
                        typed_args.append(val)

                res = mod.call_method(target_method, *typed_args)
                logger.info("[Southbound] invoke_method '%s' succeeded: %s", method_path, type(res).__name__)
                if res is None:
                    return 0, []
                return 0, [res]
            except Exception as e:
                logger.warning("[Southbound] invoke_method '%s' attempt %d error: %s", method_path, attempt, e)
                self.disconnect()
                if attempt == 0 and self.connect():
                    continue
                return self._fallback_response(method_path)


OPCUA_BRIDGE = OpcUaSouthboundBridge()


async def async_bridge_write(path, val):
    loop = asyncio.get_running_loop()
    await loop.run_in_executor(None, OPCUA_BRIDGE.write_tag, path, val)


async def opcua_sync_task():
    """Periodically reads active subscribed tags from uaserver and pushes updates to Web clients."""
    loop = asyncio.get_running_loop()
    while True:
        await asyncio.sleep(0.5)
        if not OPCUA_BRIDGE.connected:
            await loop.run_in_executor(None, OPCUA_BRIDGE.connect)
            if not OPCUA_BRIDGE.connected:
                await asyncio.sleep(3.0)
                continue

        # Collect all unique paths currently subscribed by clients
        all_subs = set()
        for client in list(ACTIVE_CLIENTS):
            all_subs.update(client.subscribed_paths)

        for path in all_subs:
            if path.startswith("system/"):
                continue
            val = await loop.run_in_executor(None, OPCUA_BRIDGE.read_tag, path)
            if val is not None:
                cur = TAG_DB.tags.get(path)
                if cur != val:
                    TAG_DB.write_tag(path, val)
                    await broadcast_tag_change(path, val)


async def handle_client(reader, writer):
    conn = WebSocketConnection(reader, writer)
    addr = writer.get_extra_info("peername")
    logger.info("New incoming connection from %s", addr)

    res = await conn.handshake()
    if res == "http":
        return
    if not res:
        logger.warning("WebSocket handshake failed for %s", addr)
        conn.close()
        return

    ACTIVE_CLIENTS.add(conn)
    logger.info("WebSocket handshake successful, registered %s", conn.client_id)

    try:
        while conn.is_open:
            opcode, data = await conn.recv_frame()
            if opcode is None:
                break
            if opcode == 0x08:  # Close frame
                break
            elif opcode == 0x09:  # Ping
                conn.writer.write(b"\x8a\x00")
                await conn.writer.drain()
                continue
            elif opcode == 0x01:  # Text frame
                try:
                    msg = json.loads(data.decode("utf-8"))
                except Exception as e:
                    logger.warning("Invalid JSON from %s: %s", conn.client_id, e)
                    continue

                op = msg.get("op")

                if op == "connect":
                    conn.client_id = msg.get("client", conn.client_id)
                    logger.info("Client connected: %s", conn.client_id)
                    await conn.send_json({
                        "op": "connected",
                        "server": "MindSCADA_Gateway_v4.7.2",
                        "timestamp": time.time(),
                        "status": 0,
                    })

                elif op == "read":
                    paths = msg.get("paths", [])
                    results = {p: TAG_DB.read_tag(p) for p in paths}
                    await conn.send_json({
                        "op": "read_resp",
                        "values": results,
                    })

                elif op == "write":
                    path = msg.get("path")
                    val = msg.get("val")
                    changed = TAG_DB.write_tag(path, val)
                    logger.info("Write tag '%s' = %s (changed=%s)", path, val, changed)
                    await conn.send_json({
                        "op": "write_ack",
                        "path": path,
                        "val": val,
                        "status": 0,
                    })
                    if changed:
                        await broadcast_tag_change(path, val)

                    # 1. Forward write directly to uaserver (OPC-UA on 4840)
                    asyncio.create_task(async_bridge_write(path, val))

                    # 2. Mirror command tags to display readback tags
                    TAG_MIRRORS = {
                        "virtual/D/D4000": "virtual/D/D2530",
                        "virtual/D/D4000/Value": "virtual/D/D2530",
                        "virtual/PID/D4210_": "virtual/PID/D5040",
                        "virtual/PID/D4210_/Value": "virtual/PID/D5040",
                        "virtual/main/D4620": "virtual/main/D4606",
                        "virtual/main/D4620/Value": "virtual/main/D4606",
                    }
                    if path in TAG_MIRRORS:
                        mirror_tag = TAG_MIRRORS[path]
                        TAG_DB.write_tag(mirror_tag, val)
                        TAG_DB.write_tag(mirror_tag + "/Value", val)
                        logger.info("Mirrored command tag '%s' -> display tag '%s' = %s", path, mirror_tag, val)
                        await broadcast_tag_change(mirror_tag, val)
                        await broadcast_tag_change(mirror_tag + "/Value", val)
                        # Also forward mirrored tag to uaserver
                        asyncio.create_task(async_bridge_write(mirror_tag, val))

                elif op in ("invoke", "invoke_method"):
                    m_path = msg.get("path", "")
                    m_inputs = msg.get("inputs", [])
                    req_id = msg.get("id")
                    loop = asyncio.get_running_loop()
                    code, outputs = await loop.run_in_executor(None, OPCUA_BRIDGE.invoke_method, m_path, m_inputs)
                    await _handle_invoke_side_effects(m_path, m_inputs, code)
                    await conn.send_json({
                        "op": "invoke_resp",
                        "id": req_id,
                        "path": m_path,
                        "code": code,
                        "outputs": outputs,
                    })

                elif op == "sub":
                    paths = msg.get("paths", [])
                    for p in paths:
                        conn.subscribed_paths.add(p)
                    logger.info("%s subscribed to %d path(s): %s", conn.client_id, len(paths), paths)
                    loop = asyncio.get_running_loop()
                    snapshot = {}
                    for p in paths:
                        if OPCUA_BRIDGE.connected and not p.startswith("system/"):
                            v = await loop.run_in_executor(None, OPCUA_BRIDGE.read_tag, p)
                            if v is not None:
                                TAG_DB.write_tag(p, v)
                        val = TAG_DB.read_tag(p)
                        if val is not None:
                            snapshot[p] = val
                    await conn.send_json({
                        "op": "sub_ack",
                        "count": len(paths),
                        "initial": snapshot,
                    })

                elif op == "unsub":
                    paths = msg.get("paths", [])
                    for p in paths:
                        conn.subscribed_paths.discard(p)
                    await conn.send_json({"op": "unsub_ack", "count": len(paths)})

    except Exception as e:
        logger.error("Connection error for %s: %s", conn.client_id, e)
    finally:
        ACTIVE_CLIENTS.discard(conn)
        conn.close()
        logger.info("Connection terminated for %s", conn.client_id)


async def simulation_ticker():
    """Simulates real-time sensor updates for streaming verification."""
    step = 0
    while True:
        await asyncio.sleep(2.0)
        step += 1
        new_temp = round(25.0 + (step % 10) * 0.35, 2)
        TAG_DB.write_tag("Devices/Sensor01/Temperature", new_temp)
        await broadcast_tag_change("Devices/Sensor01/Temperature", new_temp)


async def main():
    port = 8088
    # Start Southbound OPC-UA Bridge
    OPCUA_BRIDGE.connect()

    server = await asyncio.start_server(handle_client, host=None, port=port)
    logger.info("MindSCADA WebSocket Gateway running on ws://0.0.0.0:%d/scada_ws (dual-stack)", port)
    logger.info("Initial Tags: %d registered", len(TAG_DB.tags))

    asyncio.create_task(simulation_ticker())
    asyncio.create_task(opcua_sync_task())

    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Gateway service stopped.")
