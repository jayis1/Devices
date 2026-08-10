# FireSync ML Pipeline

6 models for home fire detection, prevention, and response:

| # | Model | Type | Edge Target | Inference | Purpose |
|---|-------|------|-------------|-----------|---------|
| 1 | FlameNet | Multi-modal 1D-CNN | ESP32-S3 | <200 ms | Fire classification (7-class) |
| 2 | ThermalAnomaly | LSTM Autoencoder | ESP32-S3 | <100 ms | Thermal array anomaly detection |
| 3 | ArcDetect | 1D-CNN (FFT) | STM32G431 | <10 ms | Electrical arc fault detection (4-class) |
| 4 | EscapeRouter | Dijkstra + learned weights | Hub ESP32-S3 | <50 ms | Dynamic escape route optimization |
| 5 | OccupantTracker | Hidden Markov Model | Cloud | <100 ms | Multi-room PIR occupant tracking |
| 6 | RiskForecast | XGBoost | Cloud | <10 ms | 7-day fire risk forecast (0-100) |

## Training

```bash
# Install dependencies
pip install torch torchvision tensorflow tflite-micro onnxruntime scikit-learn xgboost hmmlearn

# Train all models
python ../scripts/train_models.py

# Or train individually
python train_flamenet.py data/fire 50
python train_thermal_anomaly.py 50
python train_arcdetect.py 50
python train_escape_router.py data/escape 50
python train_occupant_tracker.py
python train_risk_forecast.py
python train_sensor_anomaly.py
```

## Models

| Model | Size (int8) | Accuracy | Key Metric |
|-------|-------------|----------|------------|
| FlameNet | ~180 KB | 97.3% | 98.6% recall on fire classes |
| ThermalAnomaly | ~60 KB | 94.2% recall | 0.01 FP/hour |
| ArcDetect | ~45 KB | 96.8% | 94.1% recall on arc classes |
| EscapeRouter | <1 KB (JSON graph) | — | Dijkstra shortest path |
| OccupantTracker | ~50 KB | 91.3% | 3s detection latency |
| RiskForecast | ~500 KB | AUC 0.89 | 7-day fire risk forecast |