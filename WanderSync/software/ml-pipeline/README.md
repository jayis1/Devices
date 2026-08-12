# WanderSync — ML Pipeline

7-model ML pipeline for dementia care intelligence.

## Models

| # | Model | Type | Purpose | Deployment | Size |
|---|-------|------|---------|------------|------|
| 1 | **WanderNet** | LSTM | Wandering risk prediction (2-6h ahead) from GPS + activity + time + history | Edge (nRF52840) + Cloud | ~120 KB (lite) / 4 MB (full) |
| 2 | **ADLNet** | 1D-CNN | Activity of daily living recognition (8 classes) from mmWave + PIR | Edge (ESP32-S3) | ~90 KB |
| 3 | **CogDecline** | XGBoost | Cognitive decline trajectory from longitudinal ADL patterns (3-6 month forecast) | Cloud | ~2 MB |
| 4 | **AnomalyDetect** | Isolation Forest | Behavioral anomaly detection (UTI, pain, delirium) from 20 ADL features | Cloud | ~1 MB |
| 5 | **RoutePredict** | Seq2Seq LSTM | Wandering route prediction (30-60 min ahead) from GPS trajectory | Cloud | ~4 MB |
| 6 | **ReminderOpt** | DQN | Personalized reminder timing optimization via reinforcement learning | Cloud | ~1 MB |
| 7 | **SleepNet** | BiLSTM | Sleep quality + circadian disruption + nighttime wandering risk from overnight mmWave + PPG | Cloud | ~3 MB |

## Training

```bash
# Install dependencies
pip install torch xgboost scikit-learn joblib numpy

# Train all models
python train_wandernet.py
python train_adlnet.py
python train_cogdecline.py
python train_anomalydetect.py
python train_routepredict.py
python train_reminderopt.py
python train_sleepnet.py
```

## Edge Deployment

- **WanderNet lite** → TFLite-Micro int8 → nRF52840 (Wander Band)
- **ADLNet** → TFLite-Micro int8 → ESP32-S3 (Room Sentinel)
- **KeywordNet** → TFLite-Micro int8 → ESP32-S3 (Voice Node)

Edge models use int8 quantization with representative dataset calibration.
Conversion pipeline: PyTorch → ONNX → TFLite → TFLite-Micro.

## Cloud Deployment

- **CogDecline, AnomalyDetect, RoutePredict, ReminderOpt, SleepNet** → ONNX Runtime
- Served via FastAPI endpoints + Celery workers for async inference
- InfluxDB stores time-series telemetry for model input
- Model retraining: monthly on accumulated patient data