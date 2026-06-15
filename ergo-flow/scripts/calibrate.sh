#!/bin/bash
# ErgoFlow — Calibration Script
# Guides through pressure sensor, IMU, and desk height calibration
#
# Usage: ./calibrate.sh [pressure|imu|desk|all]
# Copyright (c) 2026 jayis1. MIT License.

set -e

API_URL="${ERGOFLOW_API_URL:-http://localhost:8000}"

echo "═══════════════════════════════════════════"
echo "  ErgoFlow Calibration Utility"
echo "═══════════════════════════════════════════"
echo ""

calibrate_pressure() {
    echo "━━━ Pressure Sensor Calibration ━━━"
    echo ""
    echo "This calibrates the FSR pressure grid in the chair pad."
    echo "You will need to:"
    echo "  1. Remove all weight from the chair"
    echo "  2. Sit naturally for 10 seconds"
    echo "  3. Slouch deliberately for 5 seconds"
    echo "  4. Lean left for 5 seconds"
    echo "  5. Lean right for 5 seconds"
    echo ""

    read -p "Press Enter when the chair is empty (no weight)..." _
    echo "Recording zero baseline..."
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "pressure", "param1": 0, "param2": 0}' | jq .

    read -p "Sit naturally in the chair. Press Enter when ready..." _
    echo "Recording natural sitting posture (10 seconds)..."
    for i in $(seq 10 -1 1); do
        echo "  ${i}..."
        sleep 1
    done
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "pressure", "param1": 1, "param2": 0}' | jq .

    read -p "Slouch deliberately. Press Enter when slouching..." _
    echo "Recording slouch posture (5 seconds)..."
    for i in $(seq 5 -1 1); do
        echo "  ${i}..."
        sleep 1
    done
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "pressure", "param1": 2, "param2": 0}' | jq .

    read -p "Lean to your LEFT. Press Enter when leaning..." _
    echo "Recording left lean (5 seconds)..."
    for i in $(seq 5 -1 1); do
        echo "  ${i}..."
        sleep 1
    done
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "pressure", "param1": 3, "param2": 0}' | jq .

    read -p "Lean to your RIGHT. Press Enter when leaning..." _
    echo "Recording right lean (5 seconds)..."
    for i in $(seq 5 -1 1); do
        echo "  ${i}..."
        sleep 1
    done
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "pressure", "param1": 4, "param2": 0}' | jq .

    echo ""
    echo "✅ Pressure sensor calibration complete!"
}

calibrate_imu() {
    echo "━━━ IMU Calibration ━━━"
    echo ""
    echo "This calibrates the IMU in the wearable tag."
    echo "You will need to:"
    echo "  1. Place the tag on a flat, level surface"
    echo "  2. Rotate it slowly through all orientations"
    echo ""

    read -p "Place the wearable tag on a flat surface. Press Enter..." _
    echo "Recording level reference (5 seconds)..."
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "imu", "param1": 0, "param2": 0}' | jq .
    sleep 5

    echo ""
    echo "Now pick up the tag and slowly rotate it through all orientations."
    echo "Make a full rotation around each axis (X, Y, Z)."
    echo ""
    read -p "Press Enter when ready to start rotation..." _
    echo "Recording IMU rotation (30 seconds)..."
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "imu", "param1": 1, "param2": 0}' | jq .
    for i in $(seq 30 -1 1); do
        echo "  ${i}..."
        sleep 1
    done

    echo ""
    echo "✅ IMU calibration complete!"
}

calibrate_desk() {
    echo "━━━ Desk Height Calibration ━━━"
    echo ""
    echo "This calibrates the desk height sensors."
    echo "The desk will move to its lowest and highest positions."
    echo "⚠️  Make sure the area is clear!"
    echo ""

    read -p "Is the desk area clear? (y/N) " confirm
    if [[ "$confirm" != "y" && "$confirm" != "Y" ]]; then
        echo "Aborting desk calibration."
        return
    fi

    echo "Moving desk to lowest position..."
    curl -s -X POST "${API_URL}/api/v1/desk/preset" \
        -H "Content-Type: application/json" \
        -d '{"cmd": "preset", "preset": "sit"}' | jq .
    echo "Waiting for desk to reach bottom position..."
    sleep 10

    echo "Recording minimum height..."
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "desk", "param1": 650, "param2": 0}' | jq .

    read -p "Press Enter to move desk to highest position..." _
    echo "Moving desk to highest position..."
    curl -s -X POST "${API_URL}/api/v1/desk/preset" \
        -H "Content-Type: application/json" \
        -d '{"cmd": "preset", "preset": "stand"}' | jq .
    echo "Waiting for desk to reach top position..."
    sleep 10

    echo "Recording maximum height..."
    curl -s -X POST "${API_URL}/api/v1/calibrate" \
        -H "Content-Type: application/json" \
        -d '{"target": "desk", "param1": 1250, "param2": 0}' | jq .

    echo ""
    echo "✅ Desk height calibration complete!"
}

# ── Main ────────────────────────────────────────────────────────

case "${1:-all}" in
    pressure)
        calibrate_pressure
        ;;
    imu)
        calibrate_imu
        ;;
    desk)
        calibrate_desk
        ;;
    all)
        calibrate_pressure
        echo ""
        calibrate_imu
        echo ""
        calibrate_desk
        echo ""
        echo "═══════════════════════════════════════════"
        echo "  ✅ All calibrations complete!"
        echo "═══════════════════════════════════════════"
        ;;
    *)
        echo "Usage: $0 [pressure|imu|desk|all]"
        exit 1
        ;;
esac