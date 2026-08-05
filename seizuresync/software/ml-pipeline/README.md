# SeizureSync — ML Pipeline (8 models)

Training scripts for all SeizureSync ML models. See README.md table for
the full model list.

## Models

| # | Script | Model | Architecture |
|---|---|---|---|
| 1 | train_seizurennet.py | SeizureNet | 1D CNN (8-layer, int8 quantized, tflite-micro) |
| 2 | train_semiologynet.py | SemiologyNet | Temporal CNN (ILAE 5-class) |
| 3 | train_auranet.py | AuraNet | Bidirectional LSTM (pre-ictal) |
| 4 | train_sudepnet.py | SUDEPNet | 1D CNN + attention (apnea 5-class) |
| 5 | train_triggernet.py | TriggerNet | XGBoost + SHAP |
| 6 | train_risknet.py | RiskNet | LSTM 2-layer (24-hr forecast) |
| 7 | train_recoverynet.py | RecoveryNet | Temporal CNN |
| 8 | train_sudep_score.py | SUDEP Risk | Bayesian logistic regression |

## Data sources

- **TUH EEG Seizure Corpus** — seizure detection labels
- **EPILEPSIAE consortium** — wrist-worn accel/PPG/EDA paired with EEG
- **IEEG.org** — ECoG + autonomic signal paired data
- **MORTEMUS study** — SUDEP monitoring unit recordings
- **ILAE 2017** — seizure classification reference

## Train all

```bash
cd software/ml-pipeline
pip install -r requirements.txt
./train_all.sh
```