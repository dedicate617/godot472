#!/usr/bin/env bash
# ==============================================================================
# Build SCADA Transparent Tunnel Gateway on Linux / Raspberry Pi
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

echo "[INFO] Compiling scada_tunnel using gcc..."
gcc -std=gnu99 -O2 -Wall -Wextra -I. main.c mongoose.c -lpthread -o scada_tunnel
chmod +x scada_tunnel

echo "[SUCCESS] scada_tunnel compiled successfully:"
ls -lh scada_tunnel
