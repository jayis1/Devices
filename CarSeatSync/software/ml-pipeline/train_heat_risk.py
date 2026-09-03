from __future__ import annotations

import csv
import json
import random
from pathlib import Path

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
random.seed(11)

rows = []
for idx in range(160):
    temp = round(random.uniform(20.0, 47.0), 2); slope = round(random.uniform(0.0, 0.8), 3); cry = round(random.uniform(0.0, 1.0), 3); label = min(1.0, max(0.0, (temp - 25.0) / 20.0 + slope * 0.6 + cry * 0.15))
    rows.append({'sample': idx, 'cabin_temp_c': temp, 'heat_slope': slope, 'cry_score': cry, 'label': round(label, 4)})

csv_path = ARTIFACTS / 'heat_risk_synth.csv'
with csv_path.open('w', newline='') as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)

avg = sum(row['label'] for row in rows) / len(rows)
model = {
    'model_name': 'heat_risk',
    'samples': len(rows),
    'average_label': round(avg, 4),
    'threshold': round(avg * 1.18, 4),
    'notes': 'Synthetic hot-cabin escalation scorer.'
}
json_path = ARTIFACTS / 'heat_risk.json'
json_path.write_text(json.dumps(model, indent=2) + '\n')
print(json.dumps(model))
