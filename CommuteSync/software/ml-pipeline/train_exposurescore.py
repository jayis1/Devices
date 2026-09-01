from __future__ import annotations

import csv
import json
from pathlib import Path

base = Path(__file__).resolve().parent
csv_file = base / 'artifacts' / 'synthetic_commutes.csv'
out_file = base / 'artifacts' / 'exposurescore.json'

rows = []
with csv_file.open() as fh:
    rows.extend(csv.DictReader(fh))

weighted_scores = []
for row in rows:
    pm = min(1.0, float(row['pm25_ug_m3']) / 55.0)
    voc = min(1.0, int(row['voc_index']) / 300.0)
    vib = min(1.0, float(row['vibration_rms']) / 1.6)
    weighted_scores.append(round(pm * 0.5 + voc * 0.2 + vib * 0.3, 3))

artifact = {
    'model': 'ExposureScore',
    'samples': len(rows),
    'mean_score': round(sum(weighted_scores) / len(weighted_scores), 3),
    'p90_score': sorted(weighted_scores)[int(len(weighted_scores) * 0.9)],
    'high_risk_threshold': 0.55,
    'features': ['pm25_ug_m3', 'voc_index', 'vibration_rms'],
}

out_file.write_text(json.dumps(artifact, indent=2))
print(out_file)
