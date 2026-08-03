# MenoSync — ML Pipeline

Six models for menopause management:

| Model | Architecture | Input | Output | Location |
|-------|-------------|-------|--------|----------|
| HotFlashNet | LSTM (2-layer, 128 hidden, attention) | 20-min multi-modal (120×4) | 2-class (hot flash in 15 min) | Edge (screening) + Cloud (full) |
| NightSweatDetect | 1D-CNN (4 conv blocks) | 8h bed mat (48×4) | 3-class (none/mild/severe) | Edge (screening) + Cloud (full) |
| SleepQuality | LSTM (2-layer, 64 hidden) | 7-night features (7×6) | Sleep quality score 0-100 | Cloud |
| MoodStress | 1D-CNN (6 conv blocks) | 32 prosody + 7 behavioral (39) | 3-class (normal/mood/brain fog) | Cloud |
| BoneRisk | XGBoost (300 trees) | 30-day activity + demographics (12) | Risk score 0-100 | Cloud |
| CoolingOptimizer | DQN (256 hidden, 3 hidden layers) | 9-dim state + hot flash prediction | Cooling action (126 actions) | Cloud (trains), Edge (infers) |

## Training

```bash
# Install dependencies
pip install torch torchvision tensorboard xgboost scikit-learn numpy

# Train all models
python train_hotflash.py --data /data/hotflash --epochs 80
python train_nightsweat.py --data /data/nightsweat --epochs 100
python train_sleepquality.py --data /data/sleep --epochs 120
python train_mood.py --data /data/mood --epochs 100
python train_bonerisk.py --data /data/bone --epochs 300
python train_cooling.py --episodes 10000

# Or use the master training script
python ../scripts/train_models.py --all
```

## Datasets

Each model expects data in `.npz` format with `train.npz` and `val.npz` splits.

### HotFlashNet
- `X`: (N, 120, 4) — 20 min × 0.1 Hz × [skin_temp_cd, hrv_ms, eda_µS, ambient_cd]
- `y`: (N,) — 0=no hot flash, 1=hot flash in next 15 min
- Source: 12,000 logged hot flash events from 800 perimenopausal women

### NightSweatDetect
- `X`: (N, 48, 4) — 8h × 10-min × [sweat_pct, bed_temp_cd, motion, hr_bcg]
- `y`: (N,) — 0=none, 1=mild, 2=severe
- Source: 3,200 night sweat events from 450 women (PSG + mattress moisture)

### SleepQuality
- `X`: (N, 7, 6) — 7 nights × [sleep_eff, deep_pct, rem_pct, ns_count, hrv_avg, ambient_avg]
- `y`: (N,) — sleep quality score 0-100 (PSG-validated)
- Source: 1,800 nights of PSG from 300 women

### MoodStress
- `X`: (N, 39) — 32 prosody features + 7 behavioral features
- `y`: (N,) — 0=normal, 1=mood_change, 2=brain_fog
- Source: Voice recordings + EPDS/GAD-7/MENQOL scores (prosody only, no transcription)

### BoneRisk
- `X`: (N, 12) — 30-day [weight_bearing_min, steps_avg, sleep_qual, ns_count, age, bmi,
  family_hist, calcium_mg, vit_d_iu, hotflash_count, hrv_avg, activity_var]
- `y`: (N,) — risk score 0-100 (DXA-aligned, FRAX-validated)
- Source: Activity data + DXA scans from 1,200 women

### CoolingOptimizer
- Reinforcement learning (no labeled dataset)
- Simulated environment with realistic menopause physiology
- 10,000 episodes of 2-hour cooling scenarios
- Reward: -hot_flash_severity - energy_cost + comfort_bonus