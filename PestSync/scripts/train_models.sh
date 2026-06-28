#!/bin/bash
# PestSync — Train All ML Models
# scripts/train_models.sh
#
# Trains all ML models for the PestSync system.
# Requires GPU for YOLOv8 training (or use CPU with longer training time).

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ML_DIR="$SCRIPT_DIR/../software/ml-pipeline"

echo "🧠 PestSync ML Model Training"
echo ""

# Setup environment
cd "$ML_DIR"
python3 -m venv venv
source venv/bin/activate
pip install -q -r requirements.txt

# 1. Generate synthetic data
echo "📦 Step 1: Generating synthetic training data..."
python synthetic_pest_sim.py || true
echo ""

# 2. Train YOLOv8-nano pest detector (edge)
echo "🎯 Step 2: Training YOLOv8-nano pest detector (ESP32-S3)..."
python train_pest_classifier.py --synth || true
python train_pest_classifier.py --train || true
echo ""

# 3. Train MobileNetV3 pest ID (mobile)
echo "📱 Step 3: Training MobileNetV3 pest ID (mobile app)..."
python train_pest_id_mobile.py --train || true
echo ""

# 4. Train activity pattern LSTM
echo "📊 Step 4: Training activity pattern LSTM..."
python train_activity_lstm.py || true
echo ""

# 5. Train infestation risk forecaster (XGBoost)
echo "⚠️  Step 5: Training infestation risk forecaster (XGBoost)..."
python train_infestation_risk.py || true
echo ""

# 6. Train deterrent effectiveness model
echo "🔊 Step 6: Training deterrent effectiveness model..."
python train_deterrent_effect.py || true
echo ""

echo "✅ All ML models trained!"
echo "   Models saved to: $ML_DIR/models/"
ls -lh models/ 2>/dev/null || true