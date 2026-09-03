from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(13)

rows = []
for idx in range(160):
    caregiver = random.randint(0, 1); handoff = random.randint(0, 1); ignition = random.randint(0, 1); label = min(1.0, 0.45 * (1 - caregiver) + 0.35 * (1 - handoff) + 0.20 * (1 - ignition))
    rows.append({'sample': idx, 'caregiver_nearby': caregiver, 'handoff_complete': handoff, 'ignition_on': ignition, 'label': round(label, 4)})

csv_path = ARTIFACTS / 'unload_gap_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'unload_gap',
    'samples': len(rows),
    'average_label': round(avg, 4),
    'threshold': round(avg * 1.10, 4),
    'notes': 'Synthetic unload-sequence failure scorer.'
}
json_path = ARTIFACTS / 'unload_gap.json'
json_path.write_text(json.dumps(model, indent=2) + '\n')
print(json.dumps(model))
