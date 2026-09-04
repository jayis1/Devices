from __future__ import annotations

import csv
import json
import random
from pathlib import Path

rng = random.Random(15)
out_dir = Path(__file__).resolve().parent / 'artifacts'
out_dir.mkdir(exist_ok=True)
rows = []
actions = ['lab_test', 'shock_chlorination', 'filter_change', 'pump_service', 'uv_lamp_replace']
for _ in range(220):
    contamination = round(rng.uniform(0.01, 0.99), 3)
    pump = round(rng.uniform(0.01, 0.99), 3)
    dry_well = round(rng.uniform(0.01, 0.99), 3)
    treatment = round(rng.uniform(0.01, 0.99), 3)
    action = actions[max(range(len(actions)), key=lambda i: [contamination, contamination, treatment, pump, treatment][i] + (0.1 if actions[i] == 'pump_service' and dry_well > 0.6 else 0.0))]
    rows.append({'contamination': contamination, 'pump_failure': pump, 'dry_well': dry_well, 'treatment_integrity': treatment, 'action': action})
with (out_dir / 'service_priority_synth.csv').open('w', newline='') as fh:
    writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)
counts = {action: sum(1 for row in rows if row['action'] == action) for action in actions}
artifact = {'model': 'service_priority_v1', 'samples': len(rows), 'action_counts': counts}
(out_dir / 'service_priority.json').write_text(json.dumps(artifact, indent=2))
print(artifact)
