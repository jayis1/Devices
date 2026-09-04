from __future__ import annotations

import csv
import json
import random
from pathlib import Path

rng = random.Random(11)
out_dir = Path(__file__).resolve().parent / 'artifacts'
out_dir.mkdir(exist_ok=True)
rows = []
for _ in range(240):
    rain_mm = round(rng.uniform(0, 48), 2)
    turbidity = round(rng.uniform(0.2, 6.5), 2)
    orp = round(rng.uniform(140, 280), 1)
    ph = round(rng.uniform(5.9, 7.6), 2)
    risk = min(0.99, max(0.01, 0.06 + turbidity * 0.1 + rain_mm / 350 + max(0, 210 - orp) / 320 + max(0, 6.5 - ph) * 0.08))
    rows.append({'rain_mm': rain_mm, 'turbidity_ntu': turbidity, 'orp_mv': orp, 'ph': ph, 'risk': round(risk, 3)})
with (out_dir / 'contamination_risk_synth.csv').open('w', newline='') as fh:
    writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)
artifact = {
    'model': 'contamination_risk_v1',
    'samples': len(rows),
    'mean_risk': round(sum(r['risk'] for r in rows) / len(rows), 3),
    'thresholds': {'watch': 0.34, 'treat': 0.58, 'do_not_drink': 0.8},
}
(out_dir / 'contamination_risk.json').write_text(json.dumps(artifact, indent=2))
print(artifact)
