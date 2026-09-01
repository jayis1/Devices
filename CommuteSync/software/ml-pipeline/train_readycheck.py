from __future__ import annotations

import csv
import json
from pathlib import Path

base = Path(__file__).resolve().parent
csv_file = base / 'artifacts' / 'synthetic_commutes.csv'
out_file = base / 'artifacts' / 'readycheck.json'

rows = []
with csv_file.open() as fh:
    reader = csv.DictReader(fh)
    rows.extend(reader)

missing_risks = []
for row in rows:
    required_items = int(row['required_items'])
    confirmed_items = int(row['confirmed_items'])
    departure_in_minutes = int(row['departure_in_minutes'])
    missing = required_items - confirmed_items
    risk = round(min(1.0, (missing / max(1, required_items)) * 0.8 + max(0, 10 - departure_in_minutes) / 20), 3)
    missing_risks.append(risk)

artifact = {
    'model': 'ReadyCheck',
    'samples': len(rows),
    'mean_risk': round(sum(missing_risks) / len(missing_risks), 3),
    'high_risk_threshold': 0.65,
    'features': ['required_items', 'confirmed_items', 'departure_in_minutes'],
}

out_file.write_text(json.dumps(artifact, indent=2))
print(out_file)
