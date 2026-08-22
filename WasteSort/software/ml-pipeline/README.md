# WasteSort ML Pipeline

Training scripts for WasteSort's household waste intelligence stack.

## Models
- `train_material_classifier.py` - SortNet RGB + spectral classifier
- `train_contamination_net.py` - contamination probability estimator
- `train_fill_forecast.py` - LSTM fill forecast baseline
- `train_pickup_predictor.py` - missed-pickup / overflow predictor
- `train_diversion_policy.py` - contextual bandit policy simulator

Each script is written to run on CSV/Parquet feature exports and can be replaced with production datasets later.
