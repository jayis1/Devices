# LawnSync ML Pipeline

AI/ML training and inference scripts for the LawnSync smart lawn management system.

## Models

| Model | Architecture | Purpose | Output | Metrics |
|-------|-------------|---------|--------|---------|
| DiseaseNet | MobileNetV3-Small + custom head | 15-class lawn disease classification | Class label + confidence | 91.3% top-1, 97.8% top-3 |
| WeedSeg | U-Net-tiny (MobileNetV2 encoder) | 9-class weed semantic segmentation | Pixel-wise mask | 78.4% mIoU |
| IrrigationRL | DQN (2-layer MLP) | Optimal irrigation scheduling | Zone + duration | 38% water savings |
| SoilForecast | 2-layer LSTM (64 hidden) | 14-day soil moisture prediction | Daily moisture % | RMSE 2.1% (7d), 3.8% (14d) |
| DroughtNet | 1D-CNN + spatial CNN | 4-class drought stress | Class label | 88.7% accuracy |
| FertScheduler | XGBoost regressor | Fertilization timing + NPK ratio | Days + recommendation | MAE 3.2 days |

## Directory Structure

```
software/ml-pipeline/
├── pyproject.toml              # Dependencies
├── train_diseasenet.py         # DiseaseNet training (PyTorch → ONNX → TFLite)
├── train_weedseg.py            # WeedSeg training (PyTorch → ONNX → TFLite)
├── train_irrigation_rl.py      # IrrigationRL DQN training (PyTorch → ONNX)
├── train_soil_forecast.py      # SoilForecast LSTM training (PyTorch → ONNX)
├── train_drought_fert.py       # DroughtNet CNN + FertScheduler XGBoost
├── models/                     # Saved models (.pth, .onnx, .tflite)
└── README.md                   # This file
```

## Quick Start

```bash
# Install dependencies
pip install -e .

# Train all models
python train_diseasenet.py
python train_weedseg.py
python train_irrigation_rl.py
python train_soil_forecast.py
python train_drought_fert.py

# Or run the master script
python ../../scripts/train_models.py
```

## Edge Deployment

DiseaseNet and WeedSeg are quantized to int8 TFLite models for on-device
inference on the ESP32-S3 Lawn Scanner node:

- DiseaseNet: ~670 KB, ~200 ms inference per 224×224 image
- WeedSeg: ~1.2 MB, ~800 ms inference per 512×512 image

The cloud backend runs the full-precision models for batch processing and
generates training data from user images for continuous improvement.