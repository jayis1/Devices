from __future__ import annotations

import csv
import random
from pathlib import Path

random.seed(46)
out_dir = Path(__file__).with_name('artifacts')
out_dir.mkdir(exist_ok=True)
out_file = out_dir / 'synthetic_pregnancy_cohort.csv'

with out_file.open('w', newline='') as fh:
    writer = csv.writer(fh)
    writer.writerow([
        'movement_count_10m', 'supine_minutes', 'systolic_mmHg', 'diastolic_mmHg',
        'protein_score', 'specific_gravity', 'restlessness_index', 'risk_label'
    ])
    for _ in range(256):
        movement = random.randint(6, 22)
        supine = random.randint(0, 140)
        systolic = random.randint(102, 156)
        diastolic = random.randint(62, 104)
        protein = random.choice([0, 10, 25, 50])
        sg = round(random.uniform(1.008, 1.030), 3)
        restless = round(random.uniform(0.4, 3.8), 2)
        risk = int(
            movement < 10 or (systolic > 139 and diastolic > 89) or protein >= 25 or supine > 95
        )
        writer.writerow([movement, supine, systolic, diastolic, protein, sg, restless, risk])
print(out_file)
