# MindSCADA Linux & 树莓派 (Raspberry Pi) 部署与运行指南

本套件为 MindSCADA 工业网关与可视化引擎在 Linux / 树莓派（Raspberry Pi OS 32/64 位、Ubuntu、Debian）嵌入式环境下的完整服务包。

---

## 一、系统架构与服务端口

| 服务标识 | 服务名称 | 监听端口 | 协议 / 作用 |
| :--- | :--- | :--- | :--- |
| `opcua` | OPC-UA 仿真/核心数据服务 | **4840** | OPC-UA TCP 工业实时变量交互 |
| `nanomq` | NanoMQ 消息中间件 | **1883** | 轻量级高吞吐 MQTT 消息总线 |
| `tunnel` | SCADA 隧道服务 | **4841** | 跨网段 TCP/WebSocket 数据穿透与转发 |
| `ws_gateway` | WebSocket 数据网关 | **8088** | 浏览器 WebAssembly 与 OPC-UA 桥接 |
| `dev_server` | Web 发布服务 | **8090** | 静态托管 Web SCADA (支持 COOP/COEP) |
| `stream_gateway` | 画面串流网关 | **8095** | 远程 HMI 实时画面与音视频低延迟串流 |

---

## 二、运行依赖准备

在树莓派或 Linux 系统终端执行：

```bash
# 1. 更新包索引并安装 Python 3 与基础编译工具
sudo apt update
sudo apt install -y python3 python3-pip gcc make psmisc net-tools

# 2. 安装 Python 核心网关依赖
pip3 install websockets aiohttp asyncio
# 若网络受限可使用国内镜像源：
# pip3 install -i https://pypi.tuna.tsinghua.edu.cn/simple websockets aiohttp

# 3. (可选) 安装 NanoMQ MQTT Broker
# 方式 A：通过包管理器安装
sudo apt install -y nanomq
# 方式 B：将预编译的 nanomq 二进制文件放置于 bin/ 目录下
```

---

## 三、快速启动与控制

### 方式 1：守护进程监视器（推荐，具备崩溃自愈功能）

守护监视器运行在后台，每隔 2.5 秒探测 6 大服务端口，发现服务异常自动重拉起：

```bash
# 启动守护进程
./start_daemon.sh
# 或
python3 services/scada_daemon.py start

# 查看所有服务状态与健康检查表
./status_services.sh
# 或
python3 services/scada_daemon.py status

# 停止守护进程及所有服务
./stop_daemon.sh
# 或
python3 services/scada_daemon.py stop
```

### 方式 2：纯 Shell 脚本一键启动

```bash
# 一键后台启动全部 6 项服务
./start_all_services.sh

# 查看服务端口健康状态
./status_services.sh

# 一键停止全部服务
./stop_all_services.sh
```

---

## 四、配置为树莓派系统服务 (开机自启)

套件内置标准 systemd 服务单元与自动配置脚本：

```bash
# 1. 进入 systemd 目录并以 root 权限执行安装
cd services/systemd
sudo ./install_service.sh

# 2. 服务管理常用命令
sudo systemctl status mindscada.service   # 查看运行状态
sudo systemctl restart mindscada.service  # 重启服务
sudo systemctl stop mindscada.service     # 停止服务
journalctl -u mindscada.service -f        # 实时查看运行日志

# 3. 如需卸载开机自启服务
sudo ./uninstall_service.sh
```

---

## 五、访问 Web SCADA 界面

当服务启动且端口 8090 监听后，同局域网内任意 PC、平板或手机浏览器打开：

```text
http://<树莓派IP地址>:8090/mindscada.html
```

- **数据维护**：支持浏览器原生上传恢复包与导出备份文件下载。
- **画面串流**：访问 `http://<树莓派IP地址>:8095` 可直接预览实时 HMI 操作画面。

---

## 六、树莓派本地屏幕启动原生 HMI 客户端

若树莓派连接了本地触摸屏或 HDMI 显示器，可直接运行原生编译的客户端：

```bash
# 自动识别 ARM32 / ARM64 架构并启动 HMI
./start_mindscada_pi.sh

# 附加全屏或分辨率参数
./start_mindscada_pi.sh --fullscreen
```
