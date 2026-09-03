from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(19)

rows = []
for idx in range(160):
    chin = random.randint(0, 1); asleep = random.randint(0, 1); temp = round(random.uniform(35.5, 38.8), 2); label = min(1.0, 0.5 * chin + 0.25 * asleep + max(0.0, temp - 37.4) * 0.18)
    rows.append({'sample': idx, 'chin_to_chest': chin, 'asleep': asleep, 'skin_temp_c': temp, 'label': round(label, 4)})

csv_path = ARTIFACTS / 'sleep_posture_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'sleep_posture',
    'samples': len(rows),
    'average_label': round(avg, 4),
    'threshold': round(avg * 1.08, 4),
    'notes': 'Synthetic sleep-posture comfort classifier.'
}
json_path = ARTIFACTS / 'sleep_posture.json'
json_path.write_text(json.dumps(model, indent=2) + '\n')
print(json.dumps(model))
