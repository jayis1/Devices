from __future__ import annotations

import csv
import json
from pathlib import Path

dataset = Path(__file__).with_name('artifacts') / 'synthetic_pregnancy_cohort.csv'
if not dataset.exists():
    raise SystemExit('Run generate_synthetic_dataset.py first')

movements = []
supine = []
labels = []
with dataset.open() as fh:
    reader = csv.DictReader(fh)
    for row in reader:
        movements.append(int(row['movement_count_10m']))
        supine.append(int(row['supine_minutes']))
        labels.append(int(row['risk_label']))

threshold = sum(movements) / len(movements)
model = {
    'model': 'KickGuard-surrogate',
    'movement_threshold': round(threshold - 3.0, 2),
    'supine_weight': round(sum(supine) / len(supine) / 120.0, 3),
    'positive_rate': round(sum(labels) / len(labels), 3),
}
out = Path(__file__).with_name('artifacts') / 'kickguard.json'
out.write_text(json.dumps(model, indent=2) + '\n')
print(out)
