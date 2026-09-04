from __future__ import annotations

import csv
import json
import random
from pathlib import Path

rng = random.Random(13)
out_dir = Path(__file__).resolve().parent / 'artifacts'
out_dir.mkdir(exist_ok=True)
rows = []
for day in range(180):
    dry_spell = rng.randint(0, 40)
    deep_soil = round(rng.uniform(18, 85), 2)
    runtime = round(rng.uniform(12, 95), 2)
    score = min(0.99, max(0.01, 0.04 + dry_spell / 30 + max(0, 45 - deep_soil) / 70 + runtime / 180))
    rows.append({'day': day, 'dry_spell_days': dry_spell, 'soil_deep_pct': deep_soil, 'runtime_min': runtime, 'risk': round(score, 3)})
with (out_dir / 'dry_well_forecast_synth.csv').open('w', newline='') as fh:
    writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
    writer.writeheader()
    writer.writerows(rows)
artifact = {
    'model': 'dry_well_forecast_v1',
    'samples': len(rows),
    'warning_days': [r['day'] for r in rows if r['risk'] > 0.6][:10],
}
(out_dir / 'dry_well_forecast.json').write_text(json.dumps(artifact, indent=2))
print(artifact)
