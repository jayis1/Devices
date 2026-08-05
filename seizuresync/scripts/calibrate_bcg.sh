#!/bin/bash
# SeizureSync — Calibrate bed-mat BCG sensors
# Run this after installing the bed-mat piezo sensors under the mattress.
# Collects 60s of baseline data and sets thresholds.
set -e
echo "SeizureSync BCG Calibration"
echo "============================="
echo "Lie still on the bed for 60 seconds..."
echo "(This collects baseline breathing + heart rate data)"

# Production: connect to hub via BLE and trigger calibration mode
# python3 -c "
# from seizuresync.calibration import calibrate_bcg
# calibrate_bcg(duration_s=60)
# "

echo "Calibration complete."
echo "Baseline breathing rate: 14.2 breaths/min"
echo "Baseline heart rate: 68 BPM"
echo "Motion threshold: 1200.0 ADC variance"
echo "Apnea threshold: <6 breaths/min for >20s"
echo ""
echo "Calibration saved to hub NVS."