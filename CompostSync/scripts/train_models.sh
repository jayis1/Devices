#!/bin/bash
# Train all CompostSync ML models
# Usage: ./scripts/train_models.sh

set -e

echo "=== CompostSync ML Model Training ==="

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ML_DIR="$(dirname "$SCRIPT_DIR")/software/ml-pipeline"

cd "$ML_DIR"

# Create venv
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt

mkdir -p models data

# Step 1: Generate synthetic data
echo ""
echo "=== Step 1: Generate synthetic compost data (50,000 cycles) ==="
echo "This will take a while..."
python synthetic_compost_sim.py

# Step 2: Train maturity LSTM
echo ""
echo "=== Step 2: Train Maturity LSTM ==="
python train_maturity_lstm.py

# Step 3: Train C:N ratio estimator
echo ""
echo "=== Step 3: Train C:N Ratio Estimator ==="
python train_cn_ratio.py

# Step 4: Train completion forecaster
echo ""
echo "=== Step 4: Train Completion Forecaster ==="
python train_completion.py

# Step 5: Train pest risk predictor
echo ""
echo "=== Step 5: Train Pest Risk Predictor ==="
python train_pest_risk.py

# Step 6: Train add-item classifier (requires image data)
echo ""
echo "=== Step 6: Train Add-Item Classifier ==="
echo "Requires image data in data/images/train/ and data/images/val/"
echo "Run manually: python train_additem_classifier.py"

echo ""
echo "=== All models trained! ==="
echo "Models saved to: $ML_DIR/models/"
ls -la models/