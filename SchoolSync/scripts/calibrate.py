from __future__ import annotations

import json

sample = {
    'doorway_uwb_cm': 110,
    'lunch_dock_empty_grams': 215,
    'backpack_motion_baseline_mg': 96,
    'transit_vehicle_signature': 'family_car_a',
}
print(json.dumps(sample, indent=2))
