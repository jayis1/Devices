# BloomSync — ML Pipeline

Six models for postpartum maternal health monitoring:

| Model | Architecture | Input | Output | Location |
|-------|-------------|-------|--------|----------|
| HemorrhageRisk | LSTM (2-layer, 128 hidden, attention) | 30-min vitals (1800×4) | 3-class risk | Edge (screening) + Cloud (full) |
| PPDetect | 1D-CNN (3 conv blocks) | 32 prosody + 7 behavioral features | 2-class | Cloud |
| WoundInfect | LSTM (2-layer, 64 hidden) | 48h wound data (288×3) | 3-class | Edge (screening) + Cloud (full) |
| MastitisDetect | 1D-CNN (4 conv blocks) | 12h bilateral breast temp (72×3) | 2-class | Edge |
| PreeclampsiaRF | XGBoost (200 trees) | 6h vitals features (9 aggregated) | 2-class | Cloud |
| RecoveryLSTM | LSTM (3-layer, 256 hidden) | 14-day daily features (14×11) | 5 milestone predictions | Cloud |

## Training

```bash
# Install dependencies
pip install torch torchvision tensorboard xgboost scikit-learn numpy

# Train all models
python train_hemorrhage.py --data /data/hemorrhage --epochs 80
python train_ppd.py --data /data/ppd --epochs 100
python train_wound_infection.py --data /data/wound --epochs 80
python train_mastitis.py --data /data/mastitis --epochs 100
python train_preeclampsia.py --data /data/preeclampsia --epochs 300
python train_recovery.py --data /data/recovery --epochs 120

# Or use the master training script
python ../scripts/train_models.py --all
```

## Datasets

Each model expects data in `.npz` format with `train.npz` and `val.npz` splits.

### HemorrhageRisk
- `X`: (N, 1800, 4) — 30 min × 1 Hz × [HR, SpO2, skin_temp_cd, HRV_ms]
- `y`: (N,) — 0=low, 1=moderate, 2=high risk
- Source: Maternity ward vital signs data (anonymized, IRB-approved)

### PPDetect
- `X`: (N, 39) — 32 prosody features + 7 behavioral features
- `y`: (N,) — 0=normal, 1=PPD-screen-positive (EPDS ≥ 13)
- Source: Postpartum voice recordings + EPDS scores (prosody only, no transcription)

### WoundInfect
- `X`: (N, 288, 3) — 48h × 10-min × [wound_temp, moisture_pct, ph_x10]
- `y`: (N,) — 0=normal, 1=inflammation, 2=infection
- Source: C-section wound monitoring data + CDC SSI criteria

### MastitisDetect
- `X`: (N, 72, 3) — 12h × 10-min × [temp_left, temp_right, asymmetry]
- `y`: (N,) — 0=normal, 1=mastitis
- Source: Bilateral breast temperature data + clinical mastitis diagnosis

### PreeclampsiaRF
- `X`: (N, 360, 6) — 6h × 1-min × [HR, SpO2, temp, HRV, activity, steps]
- `y`: (N,) — 0=normal, 1=preeclampsia
- Source: Postpartum vitals + ACOG preeclampsia criteria

### RecoveryLSTM
- `X`: (N, 14, 11) — 14 days × 11 daily aggregated features
- `y_days`: (N, 5) — predicted day for each of 5 milestones
- `y_achieved`: (N, 5) — binary: milestone achieved
- Source: 6-week postpartum recovery data + functional assessments