from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(13)

rows = []
for idx in range(144):
    fever = round(random.uniform(36.3, 40.2), 2)
    coughs = random.randint(0, 45)
    co2 = random.randint(450, 1700)
    hydration = random.randint(0, 1100)
    label = round(min(100.0, max(0.0, (fever - 37.1) * 22.0 + (600 - hydration) / 10.0)), 4)
    rows.append({'sample': idx, 'fever_c': fever, 'coughs_per_hour': coughs, 'co2_ppm': co2, 'hydration_ml': hydration, 'label': label})

csv_path = ARTIFACTS / 'hydration_risk_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg_label = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'hydration_risk',
    'samples': len(rows),
    'average_label': round(avg_label, 4),
    'threshold': round(avg_label * 0.95, 4),
    'notes': 'Synthetic dehydration risk estimator.'
}

json_path = ARTIFACTS / 'hydration_risk.json'
json_path.write_text(json.dumps(model, indent=2) + "\n")
print(json.dumps(model))
