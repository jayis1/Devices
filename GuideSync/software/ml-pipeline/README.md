# GuideSync — ML Pipeline

6-model ML pipeline for the GuideSync visual assistance system.

## Models

| # | Model | Architecture | Purpose | Edge Target |
|---|-------|-------------|---------|-------------|
| 1 | SceneNet | YOLOv8-nano | Object/obstacle detection (80 classes) | ESP32-S3 (TFLite-Micro int8, ~3.8 MB) |
| 2 | ObstacleNet | 2-layer 1D-CNN | ToF depth grid hazard classification (6 classes) | ESP32-S3 (int8, ~80 KB) |
| 3 | TextReader | EAST + CRNN | OCR for signs/labels/menus | Hub ESP32-S3 (ONNX, ~12 MB) |
| 4 | NavNet | 2-layer LSTM (64 hidden) | Indoor positioning from BLE beacon RSSI | Cloud + Hub |
| 5 | CrosswalkNet | MobileNetV3-small | Crosswalk & pedestrian signal detection (4-class) | ESP32-S3 (int8, ~220 KB) |
| 6 | FallNet | 1D-CNN (2-layer) | Fall detection from 200 Hz IMU (3-class) | nRF52840 (int8, ~45 KB) |

## Training

```bash
# Train all models
python ../scripts/train_models.py --model all

# Train individual model
python ../scripts/train_models.py --model scenenet
python ../scripts/train_models.py --model fallnet
```

## Data Requirements

- **SceneNet:** COCO 2017 (118K images) + custom blind-navigation dataset (5K images)
- **ObstacleNet:** 20K synthetic ToF grids + 5K real VL53L5CX recordings
- **TextReader:** ICDAR 2015 + COCO-Text + 1K medication labels
- **NavNet:** 50K RSSI fingerprints from 12 mapped buildings
- **CrosswalkNet:** 8K labeled crosswalk images (day/night, 6 countries)
- **FallNet:** SisFall (4,505 recordings) + UMAFall + custom blind-user recordings