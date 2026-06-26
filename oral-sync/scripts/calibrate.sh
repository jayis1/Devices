#!/usr/bin/env bash
# Calibrate OralSync sensors.
# Usage: ./calibrate.sh <node> <port>
#   node: toothbrush (FSR pressure) | saliva (pH + nitrite)
set -euo pipefail

NODE="${1:?usage: $0 <toothbrush|saliva> <port>}"
PORT="${2:?port required}"

if ! command -v python3 &>/dev/null; then echo "[err] python3 required"; exit 1; fi

case "$NODE" in
  toothbrush)
    echo "[cal] Toothbrush FSR 402 pressure calibration"
    echo "  Step 1: Apply 0 N (no load) — press ENTER"; read -r
    echo "  Step 2: Apply 2.0 N (200g on brush tip) — press ENTER"; read -r
    echo "  Step 3: Apply 5.0 N (500g) — press ENTER"; read -r
    python3 - "$PORT" <<'PY'
import serial, sys, time
ser = serial.Serial(sys.argv[1], 115200, timeout=2)
time.sleep(0.5)
ser.write(b"CAL_FSR\n")
print("[cal] FSR calibration response:", ser.read(256).decode(errors="replace").strip())
ser.close()
PY
    ;;
  saliva)
    echo "[cal] Saliva sensor — pH ISFET + nitrite amperometry"
    echo "  Step 1: pH 4.00 buffer — insert tip, press ENTER"; read -r
    echo "  Step 2: pH 7.00 buffer — press ENTER"; read -r
    echo "  Step 3: pH 10.00 buffer — press ENTER"; read -r
    echo "  Step 4: 50 µM nitrite standard — press ENTER"; read -r
    python3 - "$PORT" <<'PY'
import serial, sys, time
ser = serial.Serial(sys.argv[1], 115200, timeout=2)
time.sleep(0.5)
ser.write(b"CAL_SALIVA\n")
print("[cal] saliva calibration response:", ser.read(256).decode(errors="replace").strip())
ser.close()
PY
    ;;
  *)
    echo "[err] unknown node: $NODE"; exit 1
    ;;
esac
echo "[ok] $NODE calibration complete — coefficients stored in on-board EEPROM"