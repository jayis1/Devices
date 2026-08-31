from __future__ import annotations

import csv
import json
from pathlib import Path

dataset = Path(__file__).with_name('artifacts') / 'synthetic_pregnancy_cohort.csv'
if not dataset.exists():
    raise SystemExit('Run generate_synthetic_dataset.py first')

total = 0
weighted_supine = 0.0
restless_total = 0.0
labels = 0
with dataset.open() as fh:
    reader = csv.DictReader(fh)
    for row in reader:
        total += 1
        weighted_supine += int(row['supine_minutes'])
        restless_total += float(row['restlessness_index'])
        labels += int(row['risk_label'])

model = {
    'model': 'SleepSide-surrogate',
    'avg_supine_minutes': round(weighted_supine / total, 2),
    'avg_restlessness': round(restless_total / total, 2),
    'risk_prevalence': round(labels / total, 3),
}
out = Path(__file__).with_name('artifacts') / 'sleepside.json'
out.write_text(json.dumps(model, indent=2) + '\n')
print(out)
