#!/bin/bash
# AllergySync — Train All ML Models
# Runs all training scripts and exports models for deployment
set -euo pipefail

echo "=== AllergySync ML Pipeline Training ==="
echo ""

cd "$(dirname "$0")/../software/ml-pipeline"

# Check Python environment
if [ ! -d ".venv" ]; then
  echo "Creating virtual environment..."
  python3 -m venv .venv
fi
source .venv/bin/activate

echo "Installing dependencies..."
pip install -e ".[dev]" 2>/dev/null || pip install tensorflow numpy xgboost scikit-learn scipy

echo ""
echo "1/4 — Training PollenNet (1D-CNN, on-device)..."
python train_pollennet.py
echo "  → pollennet_model.tflite"
echo "  → pollennet_model.h (copy to firmware/room-sentinel/)"

echo ""
echo "2/4 — Training PollenForecast (LSTM, cloud)..."
python train_pollen_forecast.py
echo "  → pollen_forecast_lstm.keras"

echo ""
echo "3/4 — Training SymptomPredict + AllergenSensitivity + AnomalyDetector..."
python train_symptom_sensitivity.py
echo "  → symptom_predict_xgb.pkl"
echo "  → allergen_sensitivity.pkl"
echo "  → anomaly_detector.pkl"

echo ""
echo "4/4 — Training ActivityCNN (TinyCNN, on-device nRF52840)..."
python train_activity_cnn.py
echo "  → activity_cnn_int8.tflite"

echo ""
echo "=== All models trained ==="
echo ""
echo "Deploy:"
echo "  1. Copy pollennet_model.h → firmware/room-sentinel/"
echo "  2. Copy activity_cnn_int8.tflite → firmware/wearable-tag/src/"
echo "  3. Copy cloud models → software/dashboard/models/"