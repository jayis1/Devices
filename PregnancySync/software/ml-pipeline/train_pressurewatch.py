from __future__ import annotations

import csv
import json
from pathlib import Path

dataset = Path(__file__).with_name('artifacts') / 'synthetic_pregnancy_cohort.csv'
if not dataset.exists():
    raise SystemExit('Run generate_synthetic_dataset.py first')

total = 0
pressure_risk = 0
protein_risk = 0
sg_risk = 0
with dataset.open() as fh:
    reader = csv.DictReader(fh)
    for row in reader:
        total += 1
        if int(row['systolic_mmHg']) >= 140 or int(row['diastolic_mmHg']) >= 90:
            pressure_risk += 1
        if int(row['protein_score']) >= 25:
            protein_risk += 1
        if float(row['specific_gravity']) >= 1.024:
            sg_risk += 1

model = {
    'model': 'PressureWatch-surrogate',
    'pressure_rate': round(pressure_risk / total, 3),
    'protein_rate': round(protein_risk / total, 3),
    'specific_gravity_rate': round(sg_risk / total, 3),
    'triage_rule': 'high risk when BP high and protein positive, medium when either one persists',
}
out = Path(__file__).with_name('artifacts') / 'pressurewatch.json'
out.write_text(json.dumps(model, indent=2) + '\n')
print(out)
