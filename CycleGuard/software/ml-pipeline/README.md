# CycleGuard ML Pipeline

6 models for the CycleGuard cycling safety system:

| Model | Script | Type | Target | Accuracy |
|-------|--------|------|--------|----------|
| CrashNet | `train_crash_net.py` | 1D-CNN | nRF52840 (helmet, edge) | 95% sensitivity |
| BlindSpotNet | `train_blind_spot.py` | MobileNetV3-small | ESP32-S3 (hub, edge) | 87% mAP |
| CollisionPredict | `train_collision_predict.py` | LSTM | ESP32-S3 (hub, edge) | 87% recall |
| TheftPattern | `train_theft_pattern.py` | LSTM | Cloud | 92% precision |
| RouteSafety | `train_route_safety.py` | GCN | Cloud | 0.84 AUC |
| CrashRiskForecast | `train_crash_risk_forecast.py` | XGBoost | Cloud | 0.79 AUC |

## Training

```bash
pip install -r requirements.txt
python train_crash_net.py
python train_blind_spot.py
python train_collision_predict.py
python train_theft_pattern.py
python train_route_safety.py
python train_crash_risk_forecast.py
```

## Edge Deployment

- CrashNet → ONNX → TFLite int8 → nRF52840 tflite-micro (helmet)
- BlindSpotNet → ONNX → TFLite int8 → ESP32-S3 tflite-micro (hub)
- CollisionPredict → ONNX → TFLite int8 → ESP32-S3 tflite-micro (hub)

## Cloud Deployment

- TheftPattern → ONNX → FastAPI ml_inference.py
- RouteSafety → TorchScript → FastAPI ml_inference.py
- CrashRiskForecast → XGBoost JSON → FastAPI ml_inference.py