from __future__ import annotations

import csv
import json
import random
from pathlib import Path

rng = random.Random(12)
out_dir = Path(__file__).resolve().parent / 'artifacts'
out_dir.mkdir(exist_ok=True)
rows = []
for _ in range(240):
    starts = round(rng.uniform(1, 18), 2)
    cavitation = round(rng.uniform(0, 95), 2)
    rise = round(rng.uniform(40, 220), 1)
    risk = min(0.99, max(0.01, 0.05 + starts / 30 + cavitation / 130 + max(0, 160 - rise) / 220))
    rows.append({'starts_per_hour': starts, 'cavitation_score': cavitation, 'pressure_rise_kpa': rise, 'risk': round(risk, 3)})
with (out_dir / 'pump_failure_synth.csv').open('w', newline='') as fh:
    writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)
artifact = {
    'model': 'pump_failure_v1',
    'samples': len(rows),
    'max_risk': round(max(r['risk'] for r in rows), 3),
    'features': ['starts_per_hour', 'cavitation_score', 'pressure_rise_kpa'],
}
(out_dir / 'pump_failure.json').write_text(json.dumps(artifact, indent=2))
print(artifact)
