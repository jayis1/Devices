from __future__ import annotations

import json
from pathlib import Path

profile = {
    'room_id': 'bedroom-a',
    'co2_offset_ppm': -18,
    'humidity_offset_pct': 1.4,
    'pressure_zero_pa': -0.3,
    'bottle_tare_g': 412.0,
    'band_skin_temp_offset_c': 0.18,
}
path = Path(__file__).resolve().parent / 'calibration_profile.json'
path.write_text(json.dumps(profile, indent=2) + "\n")
print(path)
