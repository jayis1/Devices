# MosquitoSync — ML Pipeline

6 models for mosquito detection, activity forecasting, and disease risk prediction.

## Models

### 1. WingNet — Mosquito Species Classification CNN
- **Input:** 64×32 mel-spectrogram (1s audio @ 16 kHz)
- **Architecture:** 2D-CNN (3 conv + 2 dense)
- **Classes:** 8 (7 mosquito species + non-mosquito)
- **Edge:** TFLite-Micro int8 (~140 KB), <200 ms on ESP32-S3
- **Metrics:** 94.3% accuracy, 96.8% recall on disease-vector species
- **Script:** `train_wingnet.py`

### 2. ActivityForecast — 72-Hour Activity LSTM
- **Input:** 168h history + 72h NWS forecast (10 features)
- **Architecture:** 3-layer LSTM (128 hidden) → Dense(72)
- **Output:** Activity index (0–1) at 1-hour resolution for 72 hours
- **Metrics:** RMSE 0.11 (24h), 0.16 (48h), 0.21 (72h)
- **Script:** `train_activity_forecast.py`

### 3. DiseaseRisk — Dengue/West Nile/Malaria XGBoost
- **Input:** Species trap counts, temperature, rainfall, population, season
- **Architecture:** 3 XGBoost models + Bayesian ensemble
- **Output:** Per-disease probability (0–1) + overall score (0–100)
- **Metrics:** Dengue AUC 0.93, West Nile 0.89, Malaria 0.91
- **Script:** `train_disease_risk.py`

### 4. BiteRisk — Personal Bite Risk XGBoost
- **Input:** Activity index, species, time, temp, humidity, wind, personal factors
- **Output:** BiteRisk Score (0–100)
- **Metrics:** MAE 8.2, R² = 0.84
- **Script:** `train_bite_risk.py`

### 5. CaptureCount — Trap Capture U-Net-tiny
- **Input:** 160×120 RGB image of trap catch bag (OV2640)
- **Architecture:** U-Net-tiny (encoder-decoder + density estimation)
- **Output:** Mosquito count (integrate density map)
- **Metrics:** Count MAE 2.3 (0–50), 11.7 (50–500)
- **Script:** `train_capture_count.py`

### 6. SensorAnomaly — Isolation Forest
- **Input:** 14-dim sensor feature vector
- **Architecture:** Isolation Forest (100 trees, 256 samples)
- **Output:** Anomaly score (0–1) + anomalous sensor identification
- **Script:** `train_sensor_anomaly.py`

## Training

```bash
# Install dependencies
pip install -e .

# Train all models
python ../scripts/train_models.py --model all

# Train individual model
python ../scripts/train_models.py --model wingnet
```

## Model Outputs

```
models/
├── wingnet.pt              # PyTorch weights
├── wingnet.onnx            # ONNX export
├── wingnet_int8.tflite     # TFLite int8 (for ESP32-S3)
├── activity_forecast.pt    # LSTM weights
├── disease_dengue.json     # XGBoost model
├── disease_west_nile.json
├── disease_malaria.json
├── bite_risk.json          # XGBoost model
├── capture_count.pt        # U-Net weights
└── sensor_anomaly.pkl      # Isolation Forest
```

## Data Sources

- **WingNet:** Wingbeats dataset (50K labeled recordings) + field recordings
- **ActivityForecast:** 5 years synthetic (degree-day model) + real fine-tuning
- **DiseaseRisk:** CDC ArboNet, WHO DengueNet, PAHO PLISA + trap counts
- **BiteRisk:** 10K labeled bite events (citizen science + controlled trials)
- **CaptureCount:** 8K labeled trap images (manual annotation)
- **SensorAnomaly:** 6 months normal operation + injected faults