# OralSync ML Pipeline

Six models power OralSync's oral-health intelligence:

| # | Model | Input | Output | Architecture | File |
|---|-------|-------|--------|--------------|------|
| 1 | Brushing-technique classifier | IMU 6-DoF @50 Hz, 2 s windows | 8-class technique | 1D CNN (3×Conv1D + Dense) | `train_brushing_technique.py` |
| 2 | Plaque-segmentation U-Net-tiny | 405 nm image 256×256 | per-pixel plaque mask | U-Net-tiny (4 levels) | `train_plaque_segmentation.py` |
| 3 | Gingivitis classifier | multispectral image 224×224 | 4-class severity | MobileNetV3-tiny | `train_gingivitis.py` |
| 4 | Caries white-spot detector | 405+NIR diff image 416×416 | lesion bbox + prob | YOLOv8-nano | `train_caries_detector.py` |
| 5 | Plaque-growth LSTM | per-tooth weekly plaque history | 72 h plaque % forecast | LSTM (2-layer) | `train_plaque_growth.py` |
| 6 | Caries-risk forecaster | per-tooth weekly features | 90-day risk 0-100 | LightGBM (TGBM) | `train_caries_risk.py` |

## Data Sources

- **Brushing**: Wearable IMU dataset from labeled brushing sessions (8 techniques, 200 subjects)
- **Plaque segmentation**: Intraoral 405 nm fluorescence images with pixel-level plaque annotations (dentist-labeled)
- **Gingivitis**: Sextant-labeled inflammation images (Löe-Silness GI 0-3)
- **Caries**: Radiograph + clinical white-spot images (ICDAS 0-6), bbox-labeled
- **Plaque growth**: Longitudinal per-tooth plaque % from scanner + brushing logs
- **Caries risk**: Per-tooth weekly features (plaque trend, pH, nitrite, coverage, lesion history, age) → 90-day caries outcome

## Training

```bash
pip install -r requirements.txt
python train_all.py                    # train all 6 models
python train_brushing_technique.py     # individual
python train_plaque_segmentation.py
python train_gingivitis.py
python train_caries_detector.py
python train_plaque_growth.py
python train_caries_risk.py
```

Each script loads its dataset from `data/<name>/`, trains, evaluates on held-out test set, and exports:
- On-device models (1, 2, 3) → TensorFlow Lite (`.tflite`) for RP2040/ESP32-S3
- Cloud models (4, 5, 6) → ONNX (`.onnx`) for the inference service

## Export Targets

| Model | On-device? | Target |
|-------|-----------|--------|
| Brushing technique | Yes | TFLite Micro (RP2040 Core 1, int8, <80 KB) |
| Plaque segmentation | Yes | TFLite (ESP32-S3, int8, <500 KB) |
| Gingivitis | Preview on-device; full in cloud | TFLite (ESP32-S3) + ONNX (cloud) |
| Caries detector | Cloud | ONNX |
| Plaque growth | Cloud | ONNX |
| Caries risk | Cloud | ONNX (LightGBM → ONNX) |