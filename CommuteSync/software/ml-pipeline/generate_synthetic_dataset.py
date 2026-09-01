from __future__ import annotations

import csv
import random
from pathlib import Path

random.seed(65)
out_dir = Path(__file__).with_name('artifacts')
out_dir.mkdir(exist_ok=True)
out_file = out_dir / 'synthetic_commutes.csv'

with out_file.open('w', newline='') as fh:
    writer = csv.writer(fh)
    writer.writerow([
        'required_items', 'confirmed_items', 'departure_in_minutes', 'eta_delta_minutes',
        'route_minutes', 'pm25_ug_m3', 'voc_index', 'vibration_rms', 'tamper_score',
        'separation_m', 'late_label', 'readiness_label', 'exposure_label'
    ])
    for _ in range(320):
        required_items = random.randint(3, 7)
        confirmed_items = random.randint(max(0, required_items - 3), required_items)
        departure_in_minutes = random.randint(0, 45)
        eta_delta = random.randint(-2, 18)
        route_minutes = random.randint(8, 90)
        pm25 = round(random.uniform(4.0, 78.0), 1)
        voc_index = random.randint(20, 320)
        vibration = round(random.uniform(0.08, 1.8), 2)
        tamper = round(random.uniform(0.01, 0.95), 2)
        separation = round(random.uniform(0.1, 18.0), 2)
        late = int(eta_delta > 7 or (departure_in_minutes < 7 and confirmed_items < required_items))
        ready = int(confirmed_items >= required_items and departure_in_minutes >= 5)
        exposure = int(pm25 > 28 or vibration > 1.0 or voc_index > 180)
        writer.writerow([
            required_items, confirmed_items, departure_in_minutes, eta_delta, route_minutes,
            pm25, voc_index, vibration, tamper, separation, late, ready, exposure
        ])

print(out_file)
