# RoutineSync ML Pipeline

This directory contains lightweight training scripts for the first-pass RoutineSync models.

## Models
- `train_routine_risk.py` -> ExitRiskNet-style departure failure predictor
- `train_item_presence_model.py` -> AnchorLocate room ranking helper
- `train_focus_bandit.py` -> cue policy baseline estimator
- `train_transition_classifier.py` -> focus/transition state classifier

All scripts generate synthetic but structured seed data so the repo is runnable before household-specific data collection exists.
