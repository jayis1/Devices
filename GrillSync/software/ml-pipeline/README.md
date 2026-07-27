# GrillSync ML Pipeline

6-model ML pipeline for the GrillSync smart grilling system.

## Models

| # | Model | Type | Purpose | Target |
|---|-------|------|---------|--------|
| 1 | DonenessNet | 1D-CNN | Meat doneness prediction from thermal gradient | ESP32-S3 (Hub) |
| 2 | FlareUpNet | LSTM | Flare-up prediction 8–15s ahead | ESP32-S3 (Sentinel) |
| 3 | GasLeakNet | XGBoost | Gas leak pattern classification | Cloud |
| 4 | SmokeNet | 1D-CNN | Smoke quality classification (5-class) | ESP32-S3 (Smoke) |
| 5 | GrillAnomaly | Isolation Forest | Grill behavior anomaly detection | Cloud |
| 6 | SafetyForecast | LSTM | Cook session safety risk forecast | Cloud |

## Training

```bash
# Install dependencies
pip install -e .

# Train all models
python train_doneness.py --data /data/cook-sessions --output models/
python train_flareup.py --data /data/flare-events --output models/
python train_gasleak.py --data /data/gas-events --output models/
python train_smoke.py --data /data/smoke-sessions --output models/
python train_anomaly.py --data /data/grill-sessions --output models/
python train_safety.py --data /data/safety-events --output models/

# Convert to TFLite int8 for edge deployment
python train_doneness.py --convert-tflite --input models/doneness_v2.pth --output models/doneness_v2.tflite
python train_flareup.py --convert-tflite --input models/flareup_v1.pth --output models/flareup_v1.tflite
python train_smoke.py --convert-tflite --input models/smoke_v1.pth --output models/smoke_v1.tflite
```

## Data Format

### Cook Sessions (DonenessNet training)
```
{
  "session_id": "cook_001",
  "meat_type": 0,           # 0=beef, 1=pork, ...
  "target_doneness": 3,      # 0-5
  "probe_history": [
    {"timestamp": 0.0, "tip_c": 20.0, "mid_c": 20.0, "surface_c": 25.0, "ambient_c": 220.0},
    {"timestamp": 0.5, "tip_c": 21.0, "mid_c": 20.5, "surface_c": 26.0, "ambient_c": 225.0},
    ...
  ],
  "label_doneness": 3,       # Ground truth (chef-verified)
  "label_temp_c": 60.0       # Ground truth final temp
}
```

### Flare-up Events (FlareUpNet training)
```
{
  "session_id": "cook_001",
  "thermal_history": [...],  # 50 timesteps × 6 channels
  "flare_up_events": [
    {"timestamp_s": 120.0, "severity": "major", "thermal_spike_c": 350}
  ]
}
```

### Smoke Sessions (SmokeNet training)
```
{
  "session_id": "smoke_001",
  "sensor_history": [...],   # 30 timesteps × 5 channels
  "labels": [
    {"timestamp_s": 0, "quality": "clean_blue"},
    {"timestamp_s": 30, "quality": "dirty_white"}
  ]
}
```

## Model Architecture Details

### DonenessNet (1D-CNN, ~140 KB int8)
```
Input:  4-channel thermocouple history (90s × 2Hz = 180 timesteps)
  → Conv1D(32, k=5) → ReLU → MaxPool(2)
  → Conv1D(64, k=5) → ReLU → MaxPool(2)
  → Flatten → Dense(64) → ReLU → Dropout(0.2)
  → Dense(16) → ReLU
  → Dense(6)  # 6-class doneness softmax
```

### FlareUpNet (LSTM, ~180 KB int8)
```
Input:  6-channel time series (5s × 10Hz = 50 timesteps)
  → LSTM(64) → Dropout(0.2)
  → Dense(32) → ReLU
  → Dense(16) → ReLU
  → Dense(2)  # [risk%, time_to_flare×100ms]
```

### SmokeNet (1D-CNN, ~90 KB int8)
```
Input:  5-channel time series (30s × 1Hz = 30 timesteps)
  → Conv1D(16, k=5) → ReLU → MaxPool(2)
  → Conv1D(32, k=3) → ReLU → MaxPool(2)
  → Flatten → Dense(32) → ReLU
  → Dense(5)  # 5-class smoke quality softmax
```

## Evaluation Metrics

| Model | Metric | Target |
|-------|--------|--------|
| DonenessNet | Accuracy | >92% |
| DonenessNet | MAE (temp) | <2.0°C |
| FlareUpNet | AUC-ROC | >0.90 |
| FlareUpNet | Lead time | 8–15s |
| FlareUpNet | False positive rate | <5% |
| GasLeakNet | F1 | >0.95 |
| SmokeNet | Accuracy | >88% |
| GrillAnomaly | Detection rate | >80% |
| SafetyForecast | AUC-ROC | >0.85 |