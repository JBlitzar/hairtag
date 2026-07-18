#!/usr/bin/env bash
# Upload firmware to ESP32 or ESP32-C3, auto-detecting the connected chip.
# Usage: ./upload.sh [--port /dev/cu.xxx]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIO_DIR="$SCRIPT_DIR/esp32c3-openhaystack"
PIO_PYTHON="$PIO_DIR/.venv/bin/python"

# ── Parse args ────────────────────────────────────────────────────────────────
PORT=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --port|-p) PORT="$2"; shift 2 ;;
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
done

# ── Find serial port ──────────────────────────────────────────────────────────
if [[ -z "$PORT" ]]; then
    # Prefer usbserial ports (ESP dev boards), filter out bluetooth/debug-console
    PORT=$("$PIO_PYTHON" - <<'EOF'
import serial.tools.list_ports, sys
for p in serial.tools.list_ports.comports():
    d = p.device.lower()
    desc = (p.description or "").lower()
    if "bluetooth" in d or "bluetooth" in desc or "debug-console" in d:
        continue
    if "usbserial" in d or "usbmodem" in d or "usberial" in d:
        print(p.device)
        sys.exit(0)
# Fallback: any non-bluetooth port
for p in serial.tools.list_ports.comports():
    d = p.device.lower()
    desc = (p.description or "").lower()
    if "bluetooth" not in d and "bluetooth" not in desc and "debug-console" not in d:
        print(p.device)
        sys.exit(0)
sys.exit(0)
EOF
    )
fi

if [[ -z "$PORT" ]]; then
    echo "Error: No serial port found. Is the device plugged in?"
    exit 1
fi

echo "Port: $PORT"

# ── Detect chip type via esptool ──────────────────────────────────────────────
echo "Detecting chip..."
CHIP_INFO=$("$PIO_PYTHON" -m esptool --port "$PORT" chip-id 2>&1 || true)

# The "Chip type:" line is reliable across esptool versions, e.g.:
#   Chip type: ESP32-C3
#   Chip type: ESP32-D0WD-V3  (bare ESP32)
CHIP_LINE=$(echo "$CHIP_INFO" | grep -i "Chip type:" || true)

if echo "$CHIP_LINE" | grep -qi "ESP32-C3"; then
    ENV="hairtag-c3"
    echo "Detected: ESP32-C3  →  env:$ENV"
elif echo "$CHIP_LINE" | grep -qi "ESP32-S3"; then
    echo "Error: ESP32-S3 not supported. Add an [env:esp32s3dev] to platformio.ini."
    exit 1
elif echo "$CHIP_LINE" | grep -qi "ESP32"; then
    ENV="esp32dev"
    echo "Detected: ESP32  →  env:$ENV"
else
    echo "Error: Could not detect chip type. esptool output:"
    echo "$CHIP_INFO"
    echo ""
    echo "Try specifying the port manually:  ./upload.sh --port /dev/cu.usbserial-xxx"
    exit 1
fi

# ── Build and upload ──────────────────────────────────────────────────────────
cd "$PIO_DIR"
pio run --environment "$ENV" --target upload --upload-port "$PORT"

echo ""
echo "Done. Firmware uploaded to $PORT ($ENV)."
