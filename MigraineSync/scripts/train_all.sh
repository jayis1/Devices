#!/bin/bash
# MigraineSync — Train All Models
# =================================
# Runs the full ML training pipeline:
#   1. Generate synthetic data
#   2. Train onset predictor (LSTM)
#   3. Train trigger identifier (XGBoost + SHAP)
#   4. Train prodrome detector (1D-CNN)
#   5. Train hydration classifier (RF)
#   6. Train sleep quality regressor (GBR)
#   7. Train barometric change-point detector (Bayesian)
#
# License: MIT

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ML_DIR="$SCRIPT_DIR/../software/ml-pipeline"

cd "$ML_DIR"

echo "============================================"
echo "  MigraineSync ML Pipeline — Train All"
echo "============================================"
echo ""

echo "[1/7] Generating synthetic data..."
python data_gen.py
echo ""

echo "[2/7] Training migraine onset predictor (LSTM)..."
python train_onset_lstm.py
echo ""

echo "[3/7] Training trigger identifier (XGBoost + SHAP)..."
python train_trigger_xgb.py
echo ""

echo "[4/7] Training prodrome detector (1D-CNN)..."
python train_prodrome_cnn.py
echo ""

echo "[5/7] Training hydration classifier (RF)..."
python train_hydration_rf.py
echo ""

echo "[6/7] Training sleep quality regressor (GBR)..."
python train_sleep_gbr.py
echo ""

echo "[7/7] Training barometric change-point detector (Bayesian)..."
python train_barometric_bocpd.py
echo ""

echo "============================================"
echo "  All models trained successfully!"
echo "  Models saved in: $ML_DIR/../models/"
echo "============================================"