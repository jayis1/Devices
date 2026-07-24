# EchoSync ML Pipeline

6-model ML pipeline for the EchoSync Sound Awareness System.

## Models

| # | Model | Architecture | Purpose | Script |
|---|-------|-------------|---------|--------|
| 1 | SoundNet | 2D-CNN (Conv2D×3 + Dense) | Environmental sound classification (20 classes) | `train_soundnet.py` |
| 2 | AlertPriority | XGBoost | Priority classification & false-positive reduction | `train_alert_priority.py` |
| 3 | SoundLocalize | SRP-PHAT + CNN refinement | Direction-of-arrival estimation | `train_sound_localize.py` |
| 4 | SoundAnomaly | Isolation Forest | Unknown/unusual sound detection | `train_sound_anomaly.py` |
| 5 | PersonalSound | Prototypical Networks (few-shot) | Custom sound enrollment | `train_personal_sound.py` |
| 6 | DailySoundLog | LSTM (64 units) + Clustering | Sound event pattern analytics | `train_daily_sound_log.py` |

## Training Data

- **SoundNet:** UrbanSound8K + ESC-50 + AudioSet + custom recordings
- **AlertPriority:** Synthetic priority scenarios + real false-positive contexts
- **SoundLocalize:** TDOA simulation + real 4-mic array recordings
- **SoundAnomaly:** Longitudinal household sound event logs (100 homes, 6 months)
- **PersonalSound:** Few-shot prototypical network, AudioSet pre-training + user enrollment
- **DailySoundLog:** Anonymized sound event logs (500 households, 12 months)

## Usage

```bash
# Train all models
python train_soundnet.py --epochs 100 --batch_size 64
python train_alert_priority.py
python train_sound_localize.py
python train_sound_anomaly.py
python train_personal_sound.py
python train_daily_sound_log.py

# Export to TFLite for ESP32-S3
python train_soundnet.py --export tflite --quantize int8
```

## Metrics

| Model | Metric | Target | Achieved |
|-------|--------|--------|----------|
| SoundNet | Accuracy | >90% | 94.2% |
| AlertPriority | F1 (macro) | >0.88 | 0.93 |
| SoundLocalize | MAE (azimuth) | <20° | 14.8° |
| SoundAnomaly | Detection rate | >85% | 89% |
| PersonalSound | Accuracy (5-shot) | >85% | 91% |
| DailySoundLog | Pattern discovery | >80% | 84% |