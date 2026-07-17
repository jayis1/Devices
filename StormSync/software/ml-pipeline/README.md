# StormSync ML Pipeline

## Models

| # | Model | Architecture | Purpose |
|---|-------|-------------|---------|
| 1 | FloodForecast | 3-layer LSTM (128 hidden) | 6-hour sump water level prediction |
| 2 | PumpHealth | Dual-branch 1D-CNN | Sump pump failure classification (6-class) |
| 3 | SoilSat | 2-layer LSTM (64 hidden) | 24-hour soil saturation forecast at 3 depths |
| 4 | RainfallRunoff | XGBoost regressor | Rainfall-to-runoff volume + peak inflow |
| 5 | StormRisk | Bayesian ensemble | Single 0-100 StormSync Score from all models |
| 6 | SensorAnomaly | Isolation Forest | Multi-sensor fault detection |

## Training

```bash
# Install dependencies
pip install torch torchvision xgboost scikit-learn numpy

# Train all models
python train_floodforecast.py
python train_pumphealth.py
python train_soilsat.py
python train_rainfall_runoff.py
python train_storm_risk.py
python train_sensor_anomaly.py

# Or use the pipeline runner
cd ../../scripts && python train_models.py
```

## Output

- `models/floodforecast_best.pth` + `floodforecast.onnx` — PyTorch + ONNX
- `models/pumphealth_best.pth` + `pumphealth.onnx` — PyTorch + ONNX
- `models/soilsat_best.pth` — PyTorch
- `models/rainfall_runoff_vol.pkl` + `rainfall_runoff_peak.pkl` — XGBoost
- `models/storm_risk.pkl` — StormRisk ensemble
- `models/sensor_anomaly.pkl` — Isolation Forest

## Edge Deployment

PumpHealth is converted to TFLite-Micro int8 quantized (~180 KB) for on-device inference on the ESP32-S3 Hub. This enables real-time pump anomaly detection even when cloud connectivity is lost.

All other models run in the cloud (GPU inference via ONNX runtime + Celery workers).