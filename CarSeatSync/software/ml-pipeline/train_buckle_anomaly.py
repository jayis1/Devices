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
    tension = round(random.uniform(10.0, 45.0), 2); clip = round(random.uniform(0.25, 0.85), 3); closed = random.randint(0, 1); label = min(1.0, max(0.0, (26.0 - tension) / 20.0 + abs(clip - 0.58) * 1.4 + (0.5 if closed == 0 else 0.0)))
    rows.append({'sample': idx, 'strap_tension_n': tension, 'chest_clip_ratio': clip, 'buckle_closed': closed, 'label': round(label, 4)})

csv_path = ARTIFACTS / 'buckle_anomaly_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'buckle_anomaly',
    'samples': len(rows),
    'average_label': round(avg, 4),
    'threshold': round(avg * 1.14, 4),
    'notes': 'Synthetic harness misuse scorer.'
}
json_path = ARTIFACTS / 'buckle_anomaly.json'
json_path.write_text(json.dumps(model, indent=2) + '\n')
print(json.dumps(model))
