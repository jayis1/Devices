from __future__ import annotations

import csv
import json
import random
from pathlib import Path

rng = random.Random(14)
out_dir = Path(__file__).resolve().parent / 'artifacts'
out_dir.mkdir(exist_ok=True)
rows = []
for _ in range(180):
    uv_lux = round(rng.uniform(120, 930), 1)
    filter_days_remaining = rng.randint(0, 120)
    cabinet_openings = rng.randint(0, 8)
    score = min(0.99, max(0.01, 0.03 + max(0, 500 - uv_lux) / 650 + max(0, 21 - filter_days_remaining) / 60 + cabinet_openings / 80))
    rows.append({'uv_lux': uv_lux, 'filter_days_remaining': filter_days_remaining, 'cabinet_openings': cabinet_openings, 'risk': round(score, 3)})
with (out_dir / 'treatment_integrity_synth.csv').open('w', newline='') as fh:
    writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)
artifact = {
    'model': 'treatment_integrity_v1',
    'samples': len(rows),
    'median_uv_lux': sorted(r['uv_lux'] for r in rows)[len(rows) // 2],
}
(out_dir / 'treatment_integrity.json').write_text(json.dumps(artifact, indent=2))
print(artifact)
