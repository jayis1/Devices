#!/bin/bash
# SeizureSync — Train all 8 models
set -e
cd "$(dirname "$0")"
mkdir -p models

echo "=== Training SeizureNet (1/8) ==="
python train_seizurennet.py --epochs 50

echo "=== Training SemiologyNet (2/8) ==="
python train_semiologynet.py --epochs 30

echo "=== Training AuraNet (3/8) ==="
python train_auranet.py --epochs 50

echo "=== Training SUDEPNet (4/8) ==="
python train_sudepnet.py --epochs 40

echo "=== Training TriggerNet (5/8) ==="
python train_triggernet.py

echo "=== Training RiskNet (6/8) ==="
python train_risknet.py --epochs 50

echo "=== Training RecoveryNet (7/8) ==="
python train_recoverynet.py --epochs 40

echo "=== Training SUDEP Risk Score (8/8) ==="
python train_sudep_score.py

echo "=== Exporting to TFLite (edge models) ==="
python export_tflite.py

echo "=== All models trained ==="
ls -la models/