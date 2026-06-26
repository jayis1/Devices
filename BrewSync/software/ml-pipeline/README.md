# BrewSync ML Pipeline

This directory contains training scripts for BrewSync's 6 ML models:

## Models

1. **Fermentation Progress Model** — LSTM predicting final gravity and completion time
2. **Stuck Fermentation Predictor** — Random Forest on CO2 evolution, temperature, pH
3. **Infection Detector** — Spectral anomaly detection + 15-class classifier
4. **Yeast Health Model** — Cell count and viability estimation from CO2 + temperature
5. **Recipe Optimizer** — Temperature schedule and pitch rate recommendations
6. **Flavor Predictor** — IBU, ABV, SRM, flavor notes from grain bill + fermentation data

## Setup

```bash
pip install -r requirements.txt
```

## Training

```bash
# Train all models
python train_all.py

# Train individual models
python train_fermentation_progress.py --epochs 100
python train_stuck_predictor.py --cross-validate
python train_infection_detector.py --data-path data/spectral/
python train_yeast_health.py
python train_recipe_optimizer.py
python train_flavor_predictor.py
```