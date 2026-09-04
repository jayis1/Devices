from __future__ import annotations

import json
from pathlib import Path

profile = {
    'ph_offset': -0.03,
    'ec_scale': 1.02,
    'orp_offset_mv': 4,
    'turbidity_zero_ntu': 0.12,
    'pressure_offset_kpa': -6,
}
path = Path(__file__).with_name('calibration_profile.json')
path.write_text(json.dumps(profile, indent=2))
print(profile)
