# RehabSync ML Pipeline

## Overview

6-model ML pipeline for the RehabSync physical therapy rehabilitation system.

| Model | Architecture | Input | Output | Purpose |
|-------|-------------|-------|--------|---------|
| ExerciseNet | 1D-CNN (6 conv layers) | 1s × 9 features (IMU) | 30-class exercise ID | Exercise recognition |
| FormNet | Temporal CNN (4 dilated conv) | 2s × 18 features (joint angles) | Form score 0-100 + deviation (6-class) | Form quality assessment |
| RepCount | Peak detection + state machine | 500ms × joint angle + force | Rep count increment | Automatic rep counting |
| RecoveryLSTM | 2-layer LSTM (128 hidden) | 8 weeks × daily features | Milestone prediction (weeks) | 8-week recovery timeline forecast |
| AdherenceRF | Random Forest (500 trees) | 7-day features | Adherence risk (0-1) | 7-day dropout prediction |
| AnomalyIF | Isolation Forest (256 trees) | Per-rep joint trajectory + force | Anomaly score | Compensation pattern detection |

## On-Device vs Cloud

| Model | Location | Framework | Size | Latency |
|-------|----------|-----------|------|---------|
| ExerciseNet | Edge (ESP32-S3) | TFLite-Micro | 180 KB | <80 ms |
| FormNet | Edge (ESP32-S3) | TFLite-Micro | 95 KB | <50 ms |
| RepCount | Edge (ESP32-S3) | Custom C state machine | 12 KB | <5 ms |
| RecoveryLSTM | Cloud (GPU) | PyTorch | 2.1 MB | ~100 ms |
| AdherenceRF | Cloud (GPU) | scikit-learn | 850 KB | ~10 ms |
| AnomalyIF | Cloud (GPU) | scikit-learn | 1.2 MB | ~20 ms |

## Training Data

- **ExerciseNet:** 50,000+ labeled reps across 30 exercises, 200+ subjects, 6 Body Sensors @ 100 Hz
- **FormNet:** 20,000+ labeled reps with form scores (0-100) from 3 licensed PTs
- **RecoveryLSTM:** 10,000+ patient recovery trajectories (8+ weeks) from 3 PT clinics
- **AdherenceRF:** 15,000+ patient adherence records with demographics, plans, completion rates
- **AnomalyIF:** Unsupervised training on clean form data

## Data Augmentation

- Gaussian noise (σ=0.01 on normalized IMU)
- Time warping (±10% stretch/compress)
- Sensor dropout (random 1-2 sensors missing)
- Channel permutation (left/right body swap)
- Amplitude scaling (±5%)

## Usage

```bash
# Train all models
python scripts/train_models.py --all

# Train individual models
python software/ml-pipeline/train_exercise.py --data /data/exercise_dataset --epochs 100
python software/ml-pipeline/train_form.py --data /data/form_dataset --epochs 200
python software/ml-pipeline/train_rep_count.py --data /data/rep_dataset
python software/ml-pipeline/train_recovery.py --data /data/recovery_trajectories --epochs 500
python software/ml-pipeline/train_adherence.py --data /data/adherence_records
python software/ml-pipeline/train_anomaly.py --data /data/clean_form_data
```