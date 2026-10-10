#!/usr/bin/env bash
# ==============================================================================
# MindSCADA Backend Services Status Checker (Linux / Raspberry Pi)
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(which python3 2>/dev/null || which python 2>/dev/null || echo python)"

if [ -f "${SCRIPT_DIR}/scada_daemon.py" ]; then
    "${PYTHON_BIN}" "${SCRIPT_DIR}/scada_daemon.py" status
    exit $?
fi

# Fallback shell status display
C_GREEN="\033[92m"
C_RED="\033[91m"
C_BOLD="\033[1m"
C_RESET="\033[0m"

echo "================================================================="
echo -e " ${C_BOLD}MindSCADA Service Status${C_RESET}"
echo "================================================================="

is_port_listening() {
    local port=$1
    if command -v ss >/dev/null 2>&1; then
        ss -tuln | grep -q ":${port} "
        return $?
    elif command -v netstat >/dev/null 2>&1; then
        netstat -tuln | grep -q ":${port} "
        return $?
    else
        timeout 0.3 bash -c "</dev/tcp/127.0.0.1/${port}" >/dev/null 2>&1
        return $?
    fi
}

declare -A SERVICES=(
    [4840]="OPC-UA Server"
    [1883]="NanoMQ Broker"
    [4841]="SCADA Tunnel"
    [8088]="WS Gateway"
    [8090]="Web Server"
    [8095]="Stream Gateway"
)

printf " %-20s %-10s %-15s\n" "Service Name" "Port" "Status"
echo "-----------------------------------------------------------------"

for port in 4840 1883 4841 8088 8090 8095; do
    sname="${SERVICES[$port]}"
    if is_port_listening "$port"; then
        printf " %-20s %-10s ${C_GREEN}%-15s${C_RESET}\n" "$sname" "$port" "[OK] ACTIVE"
    else
        printf " %-20s %-10s ${C_RED}%-15s${C_RESET}\n" "$sname" "$port" "[DOWN] INACTIVE"
    fi
done
echo "================================================================="
