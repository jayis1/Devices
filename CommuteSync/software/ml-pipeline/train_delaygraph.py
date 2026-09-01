from __future__ import annotations

import csv
import json
from pathlib import Path

base = Path(__file__).resolve().parent
csv_file = base / 'artifacts' / 'synthetic_commutes.csv'
out_file = base / 'artifacts' / 'delaygraph.json'

rows = []
with csv_file.open() as fh:
    rows.extend(csv.DictReader(fh))

late_rate = sum(int(r['late_label']) for r in rows) / len(rows)
mean_eta_delta = sum(int(r['eta_delta_minutes']) for r in rows) / len(rows)
mean_route = sum(int(r['route_minutes']) for r in rows) / len(rows)

artifact = {
    'model': 'DelayGraph',
    'samples': len(rows),
    'late_rate': round(late_rate, 3),
    'mean_eta_delta_minutes': round(mean_eta_delta, 2),
    'mean_route_minutes': round(mean_route, 2),
    'high_risk_threshold': 0.6,
    'features': ['eta_delta_minutes', 'route_minutes', 'departure_in_minutes'],
}

out_file.write_text(json.dumps(artifact, indent=2))
print(out_file)
