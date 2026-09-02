from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(11)

rows = []
for idx in range(96):
    fever = round(random.uniform(36.7, 40.8), 2)
    coughs = random.randint(0, 40)
    co2 = random.randint(450, 1800)
    hydration = random.randint(200, 1400)
    label = round(36.8 + max(0.0, fever - 37.0) * 0.7 + (coughs / 150.0), 4)
    rows.append({'sample': idx, 'fever_c': fever, 'coughs_per_hour': coughs, 'co2_ppm': co2, 'hydration_ml': hydration, 'label': label})

csv_path = ARTIFACTS / 'fever_forecast_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg_label = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'fever_forecast',
    'samples': len(rows),
    'average_label': round(avg_label, 4),
    'threshold': round(avg_label * 1.02, 4),
    'notes': 'Synthetic 6-hour fever forecast.'
}

json_path = ARTIFACTS / 'fever_forecast.json'
json_path.write_text(json.dumps(model, indent=2) + "\n")
print(json.dumps(model))
