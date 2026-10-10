#!/usr/bin/env bash
# ==============================================================================
# Stop MindSCADA Supervisor Daemon (Linux / Raspberry Pi)
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="$(which python3 2>/dev/null || which python 2>/dev/null || echo python3)"

exec "${PYTHON_BIN}" "${SCRIPT_DIR}/scada_daemon.py" stop
