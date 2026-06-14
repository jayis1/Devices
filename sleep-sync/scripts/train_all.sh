#!/bin/bash
# train_all.sh — Train all ML models for SleepSync
#
# Prerequisites:
#   - Python 3.10+ with venv
#   - pip install tensorflow scikit-learn scipy numpy
#
# Usage:
#   ./train_all.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ML_DIR="$SCRIPT_DIR/../software/ml-pipeline"

echo "=== SleepSync ML Training Pipeline ==="
echo ""

# Create virtual environment if needed
if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    echo "Creating Python virtual environment..."
    python3 -m venv "$SCRIPT_DIR/.venv"
    source "$SCRIPT_DIR/.venv/bin/activate"
    pip install --upgrade pip
    pip install tensorflow scikit-learn scipy numpy
else
    source "$SCRIPT_DIR/.venv/bin/activate"
fi

echo "Training sleep staging model..."
python "$ML_DIR/train_sleep_staging.py" \
    --nights 200 \
    --epochs 80 \
    --output "$ML_DIR/sleep_staging.tflite"

echo ""
echo "Training apnea detection model..."
python "$ML_DIR/train_apnea.py" \
    --samples 10000 \
    --epochs 30 \
    --output "$ML_DIR/apnea_detector.tflite"

echo ""
echo "=== Training Complete ==="
echo "Models saved to $ML_DIR/"
echo ""
echo "Deploy to hub: Copy .tflite files to hub's SD card /models/ directory"