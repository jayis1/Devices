from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(7)

rows = []
for idx in range(120):
    fever = round(random.uniform(36.4, 40.5), 2)
    coughs = random.randint(0, 70)
    co2 = random.randint(500, 2200)
    hydration = random.randint(100, 1600)
    label = round(min(1.0, (coughs / 70.0) + max(0.0, fever - 37.2) * 0.08), 4)
    rows.append({'sample': idx, 'fever_c': fever, 'coughs_per_hour': coughs, 'co2_ppm': co2, 'hydration_ml': hydration, 'label': label})

csv_path = ARTIFACTS / 'cough_classifier_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg_label = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'cough_classifier',
    'samples': len(rows),
    'average_label': round(avg_label, 4),
    'threshold': round(avg_label * 1.15, 4),
    'notes': 'Synthetic cough burden scorer.'
}

json_path = ARTIFACTS / 'cough_classifier.json'
json_path.write_text(json.dumps(model, indent=2) + "\n")
print(json.dumps(model))
