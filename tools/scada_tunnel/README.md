# MindSCADA Transparent Network Tunnel Gateway (C / Mongoose)

高性能、超轻量、纯 C 语言编写的 SCADA 透明网络隧道网关（基于 Cesanta Mongoose v7 嵌入式网络引擎）。

用于让 Web 浏览器环境中的 SCADA WebAssembly 前端透明连接传统的原生 TCP 服务（MQTT Broker、OPC-UA Server 等），无需改造旧系统后台或桌面工程代码。

---

## 架构说明

```
[Web 浏览器 (WASM)]
   |
   |-- WebSocket (ws://host:8083/mqtt)  -----> [ scada_tunnel.exe ] -----> Raw TCP (127.0.0.1:1883) -----> [ MQTT Broker ]
   |
   |-- WebSocket (ws://host:4841)       -----> [ scada_tunnel.exe ] -----> Raw TCP (127.0.0.1:4840) -----> [ OPC-UA Server ]
```

- **单进程多端口监听**：
  - `ws://0.0.0.0:8083` <-> `tcp://127.0.0.1:1883`（MQTT 协议通道，自动透传 `Sec-WebSocket-Protocol: mqtt`）
  - `ws://0.0.0.0:4841` <-> `tcp://127.0.0.1:4840`（OPC-UA 协议通道）
- **协议透明**：双向二进制流无损透传，浏览器前端使用标准的 `WebSocketPeer` 即可完成与原生协议完全相同的报文交互。

---

## 性能指标对比

| 指标 | 原 Python 网关方案 (`ws_tunnel_server.py`) | 本方案 (C / Mongoose 网关 `scada_tunnel.exe`) | 优势 |
| :--- | :--- | :--- | :--- |
| **可执行文件大小** | 需 Python 解释器环境 (~50MB+) | **~207 KB** (单文件静态独立可执行) | **缩小 99.6%** |
| **内存占用 (Working Set)** | 28MB - 45MB | **~4.7 MB** (私有内存 < 1MB) | **降低 85% - 90%** |
| **冷启动耗时** | 600ms - 1200ms | **< 10ms** | **快 100 倍** |
| **运行时依赖** | Python 3.8+ 解释器及标准库 | **零外部依赖**（仅依赖系统标准 C 库与系统 Socket） | 开箱即用，极简运维部署 |
| **多平台支持** | Windows / Linux / macOS | Windows (`ws2_32`) / Linux / ARM 嵌入式设备 | 可直接交叉编译至各类网关硬件 |

---

## 编译指南

### Windows (GCC / MinGW)
直接运行：
```cmd
build.bat
```
或手动编译：
```cmd
gcc -std=gnu99 -O2 -I. main.c mongoose.c -lws2_32 -o scada_tunnel.exe
```

### Linux / ARM
直接运行：
```bash
make
```

---

## 运行与验证
```cmd
scada_tunnel.exe
```
控制台将输出：
```text
===============================================================
MindSCADA Transparent Network Tunnel Gateway (C / Mongoose)
===============================================================
[RUNNING] MQTT Tunnel (WS:8083 -> TCP:1883)
[RUNNING] OPC-UA Tunnel (WS:4841 -> TCP:4840)
Listening for Web browser connections. Press Ctrl+C to exit.
```
