from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(19)

rows = []
for idx in range(128):
    fever = round(random.uniform(36.2, 39.5), 2)
    coughs = random.randint(0, 60)
    co2 = random.randint(450, 1800)
    hydration = random.randint(150, 1500)
    label = round(max(0.0, 100.0 - ((fever - 36.8) * 18.0 + coughs * 0.8) + hydration / 25.0), 4)
    rows.append({'sample': idx, 'fever_c': fever, 'coughs_per_hour': coughs, 'co2_ppm': co2, 'hydration_ml': hydration, 'label': label})

csv_path = ARTIFACTS / 'recovery_clock_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg_label = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'recovery_clock',
    'samples': len(rows),
    'average_label': round(avg_label, 4),
    'threshold': round(avg_label * 0.88, 4),
    'notes': 'Synthetic recovery phase estimator.'
}

json_path = ARTIFACTS / 'recovery_clock.json'
json_path.write_text(json.dumps(model, indent=2) + "\n")
print(json.dumps(model))
