# MindSCADA Backend Services & Microservices

本目录包含 MindSCADA Web 平台与边缘网关所需的后端微服务及支持工具组件。

---

## 一、服务架构与端口拓扑

| 服务名称 | 对应文件 | 监听端口 | 协议 / 类型 | 功能职责 |
| :--- | :--- | :--- | :--- | :--- |
| **OPC-UA Core** | `MindSCADASvc.windows.opt.64.exe` / `uaserver_service.py` | `4840` | OPC-UA (opc.tcp) | SCADA 核心变量引擎、配方与用户鉴权底层服务 |
| **OPC-UA Supervisor** | `run_mindscada_svc.py` | - | 进程监控与自愈 | 监视 4840 端口守护进程并在异常退出时自动拉起 |
| **Tunnel Daemon** | `tools/scada_tunnel/scada_tunnel.exe` | `4841` | TCP / Mongoose 隧道 | SCADA 本地进程与边缘客户端隧道中继 |
| **MQTT Broker** | `nanomq.exe` | `1883` / `8083` | MQTT / WebSocket MQTT | 遥测点位数据发布/订阅高速消息中间件 |
| **Web Gateway** | `scada_ws_gateway.py` | `8088` | WebSocket & HTTP RPC | SCADA Web 统一通信网关：Tag 变量同步、RPC `/invoke` (配方/鉴权/文件) 调用、双栈 IPv4/IPv6 支持 |
| **Stream Gateway** | `scada_stream_gateway.py` | `8095` | HTTP MJPEG & WebSocket | 工业实时动态图表 (rtchart) 与高帧率画面流式传输网关 |
| **Stream Player** | `remote_stream_player.html` | - | HTML5 / Canvas | 实时图表与动态画面 Web 端测试与嵌入播放器 |

---

## 二、启动与停止

在前端工程根目录（`C:\prj\luxiangyi472\scada4`）使用一键批处理脚本统一调度：
- 启动全套服务：`start_all_services.bat`
- 优雅停止服务：`stop_all_services.bat`
