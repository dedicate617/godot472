#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Backend Services One-Key Launcher (Linux / Raspberry Pi)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "${SCRIPT_DIR}")"
BIN_DIR="${SCRIPT_DIR}/bin"
LOG_DIR="${SCRIPT_DIR}/logs"
PID_DIR="${SCRIPT_DIR}/.pids"
WEB_DIR="${SCRIPT_DIR}/web"

if [ ! -d "${WEB_DIR}" ]; then
    if [ -d "${ROOT_DIR}/web" ]; then
        WEB_DIR="${ROOT_DIR}/web"
    elif [ -d "${ROOT_DIR}/../luxiangyi472/scada4/build_web" ]; then
        WEB_DIR="${ROOT_DIR}/../luxiangyi472/scada4/build_web"
    else
        WEB_DIR="${SCRIPT_DIR}"
    fi
fi

mkdir -p "${LOG_DIR}" "${PID_DIR}"

PYTHON_BIN="$(which python3 2>/dev/null || which python 2>/dev/null || echo python3)"

# Colors
C_GREEN="\033[92m"
C_YELLOW="\033[93m"
C_RED="\033[91m"
C_BOLD="\033[1m"
C_RESET="\033[0m"

echo "========================================================"
echo " MindSCADA Backend Services Launcher (Linux/Raspberry Pi)"
echo "========================================================"
echo " Working Dir : ${SCRIPT_DIR}"
echo " Log Dir     : ${LOG_DIR}"
echo " Python      : ${PYTHON_BIN}"
echo "========================================================"
echo ""

is_port_listening() {
    local port=$1
    if command -v ss >/dev/null 2>&1; then
        ss -tuln | grep -q ":${port} "
        return $?
    elif command -v netstat >/dev/null 2>&1; then
        netstat -tuln | grep -q ":${port} "
        return $?
    else
        # Fallback to bash tcp check
        timeout 0.3 bash -c "</dev/tcp/127.0.0.1/${port}" >/dev/null 2>&1
        return $?
    fi
}

start_bg() {
    local name="$1"
    local pid_file="${PID_DIR}/${name}.pid"
    local log_file="${LOG_DIR}/${name}.log"
    shift
    nohup "$@" >> "${log_file}" 2>&1 &
    local pid=$!
    echo "${pid}" > "${pid_file}"
}

# 1. OPC-UA Server [4840]
echo -n "[1/6] Checking OPC-UA Server [4840]... "
if is_port_listening 4840; then
    echo -e "${C_GREEN}[OK] Already listening on port 4840${C_RESET}"
else
    echo "Starting..."
    if [ -f "${SCRIPT_DIR}/run_mindscada_svc.py" ]; then
        start_bg opcua "${PYTHON_BIN}" "${SCRIPT_DIR}/run_mindscada_svc.py"
    elif [ -f "${SCRIPT_DIR}/uaserver_service.py" ]; then
        start_bg opcua "${PYTHON_BIN}" "${SCRIPT_DIR}/uaserver_service.py"
    else
        echo -e "${C_RED}[WARN] OPC-UA script not found!${C_RESET}"
    fi
    sleep 1
fi

# 2. NanoMQ Broker [1883]
echo -n "[2/6] Checking NanoMQ Broker [1883]... "
if is_port_listening 1883; then
    echo -e "${C_GREEN}[OK] Already listening on port 1883${C_RESET}"
else
    echo "Starting..."
    NANOMQ_BIN="$(which nanomq 2>/dev/null || echo "${BIN_DIR}/nanomq")"
    if [ -x "${NANOMQ_BIN}" ]; then
        start_bg nanomq "${NANOMQ_BIN}" start --daemon
    else
        echo -e "${C_YELLOW}[WARN] nanomq executable not found. Install via 'sudo apt install nanomq' or place in bin/${C_RESET}"
    fi
    sleep 0.5
fi

# 3. SCADA Tunnel [4841]
echo -n "[3/6] Checking SCADA Tunnel [4841]... "
if is_port_listening 4841; then
    echo -e "${C_GREEN}[OK] Already listening on port 4841${C_RESET}"
else
    echo "Starting..."
    TUNNEL_BIN="${BIN_DIR}/scada_tunnel"
    if [ ! -x "${TUNNEL_BIN}" ] && [ -f "${ROOT_DIR}/tools/scada_tunnel/main.c" ]; then
        echo "      Compiling scada_tunnel from source..."
        mkdir -p "${BIN_DIR}"
        gcc -std=gnu99 -O2 -Wall -I "${ROOT_DIR}/tools/scada_tunnel" \
            "${ROOT_DIR}/tools/scada_tunnel/main.c" "${ROOT_DIR}/tools/scada_tunnel/mongoose.c" \
            -lpthread -o "${TUNNEL_BIN}" || true
        chmod +x "${TUNNEL_BIN}" 2>/dev/null || true
    fi

    if [ -x "${TUNNEL_BIN}" ]; then
        start_bg tunnel "${TUNNEL_BIN}"
    else
        echo -e "${C_YELLOW}[WARN] scada_tunnel executable not found.${C_RESET}"
    fi
    sleep 0.5
fi

# 4. WebSocket Gateway [8088]
echo -n "[4/6] Checking WS Gateway [8088]... "
if is_port_listening 8088; then
    echo -e "${C_GREEN}[OK] Already listening on port 8088${C_RESET}"
else
    echo "Starting..."
    if [ -f "${SCRIPT_DIR}/scada_ws_gateway.py" ]; then
        start_bg ws_gateway "${PYTHON_BIN}" "${SCRIPT_DIR}/scada_ws_gateway.py"
    else
        echo -e "${C_RED}[WARN] scada_ws_gateway.py not found!${C_RESET}"
    fi
    sleep 1
fi

# 5. Web Server [8090]
echo -n "[5/6] Checking Web Server [8090]... "
if is_port_listening 8090; then
    echo -e "${C_GREEN}[OK] Already listening on port 8090${C_RESET}"
else
    echo "Starting..."
    if [ -f "${SCRIPT_DIR}/dev_server.py" ]; then
        start_bg dev_server "${PYTHON_BIN}" "${SCRIPT_DIR}/dev_server.py" --port 8090 --dir "${WEB_DIR}"
    else
        echo -e "${C_RED}[WARN] dev_server.py not found!${C_RESET}"
    fi
    sleep 0.5
fi

# 6. Stream Gateway [8095]
echo -n "[6/6] Checking Stream Gateway [8095]... "
if is_port_listening 8095; then
    echo -e "${C_GREEN}[OK] Already listening on port 8095${C_RESET}"
else
    echo "Starting..."
    if [ -f "${SCRIPT_DIR}/scada_stream_gateway.py" ]; then
        start_bg stream_gateway "${PYTHON_BIN}" "${SCRIPT_DIR}/scada_stream_gateway.py"
    else
        echo -e "${C_RED}[WARN] scada_stream_gateway.py not found!${C_RESET}"
    fi
    sleep 1
fi

echo ""
echo "========================================================"
echo -e "${C_GREEN}[SUCCESS] All requested services initialized.${C_RESET}"
echo "Run '${SCRIPT_DIR}/status_services.sh' to check health."
echo "========================================================"
