# VoiceSync — ML Pipeline

6-model ML pipeline for voice health monitoring and disorder risk prediction.

## Models

| # | Model | Architecture | Purpose | Metrics |
|---|-------|-------------|---------|---------|
| 1 | VoiceNet | 2D-CNN (Conv2D×3) | Voice quality classification (8 classes) | Acc 93.1% |
| 2 | VocalLoad | XGBoost | Cumulative vocal dose estimation | R² 0.91 |
| 3 | VoiceRisk | 3-layer LSTM (128) | 7-day disorder risk forecast | RMSE 0.09 |
| 4 | RefluxDetect | 1D-CNN (Conv1D×4) | Laryngopharyngeal reflux detection | AUC 0.94 |
| 5 | HydrationModel | XGBoost | Hydration status from voice + intake | R² 0.87 |
| 6 | VocalAnomaly | Isolation Forest | Vocal change anomaly detection | Detection 91% |

## Training

```bash
# Install dependencies
pip install torch xgboost scikit-learn numpy

# Train all models
python ../scripts/train_models.py --model all

# Train individual models
python train_voicenet.py 50
python train_voice_risk.py 50
python train_vocal_load.py
python train_reflux_detect.py 50
python train_hydration_model.py
python train_vocal_anomaly.py
```

## Training Data

- **VoiceNet:** Saarbrücken Voice Database (SVD) + MIT Voice Bank + augmentations
- **VocalLoad:** NCVS voice dosimetry dataset + synthetic vocal dose models
- **VoiceRisk:** 5-year synthetic biomechanical vocal fold model
- **RefluxDetect:** Clinical LPR recordings (120 patients) + augmentation
- **HydrationModel:** Dehydration study voice recordings + paired intake data
- **VocalAnomaly:** Longitudinal voice quality trends from clinical studies

## Edge Deployment

VoiceNet runs on-device (ESP32-S3) using TFLite-Micro with int8 quantization.
Other models run in the cloud. The Hub runs local heuristics for real-time alerts.