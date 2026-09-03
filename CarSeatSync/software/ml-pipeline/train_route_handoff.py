from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(23)

rows = []
for idx in range(160):
    routine_change = random.randint(0, 1); stop_count = random.randint(0, 5); daycare_open = random.randint(0, 1); label = min(1.0, 0.45 * routine_change + 0.08 * stop_count + 0.25 * (1 - daycare_open))
    rows.append({'sample': idx, 'routine_change': routine_change, 'stop_count': stop_count, 'destination_open': daycare_open, 'label': round(label, 4)})

csv_path = ARTIFACTS / 'route_handoff_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'route_handoff',
    'samples': len(rows),
    'average_label': round(avg, 4),
    'threshold': round(avg * 1.12, 4),
    'notes': 'Synthetic route-context missed-handoff scorer.'
}
json_path = ARTIFACTS / 'route_handoff.json'
json_path.write_text(json.dumps(model, indent=2) + '\n')
print(json.dumps(model))
