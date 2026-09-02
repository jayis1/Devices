# SickDaySync ML pipeline

These training scripts use synthetic data so the repository remains runnable without protected clinical datasets.
Each script writes:

- `artifacts/<model>.json` — coefficients, thresholds, and metadata
- `artifacts/<model>_synth.csv` — generated sample data used during the run
