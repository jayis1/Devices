from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(17)

rows = []
for idx in range(160):
    fever = round(random.uniform(36.3, 39.8), 2)
    coughs = random.randint(0, 80)
    co2 = random.randint(450, 2600)
    hydration = random.randint(100, 1600)
    label = round(min(100.0, (co2 / 25.0) + coughs * 0.9), 4)
    rows.append({'sample': idx, 'fever_c': fever, 'coughs_per_hour': coughs, 'co2_ppm': co2, 'hydration_ml': hydration, 'label': label})

csv_path = ARTIFACTS / 'spread_score_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg_label = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'spread_score',
    'samples': len(rows),
    'average_label': round(avg_label, 4),
    'threshold': round(avg_label * 0.9, 4),
    'notes': 'Synthetic household contagion spread score.'
}

json_path = ARTIFACTS / 'spread_score.json'
json_path.write_text(json.dumps(model, indent=2) + "\n")
print(json.dumps(model))
