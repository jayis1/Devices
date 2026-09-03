from __future__ import annotations

import json
from pathlib import Path

profile = {
    'seat_model': 'generic-convertible-seat',
    'recommended_tension_n': 30.0,
    'min_pass_tension_n': 24.0,
    'target_clip_ratio': 0.58,
    'allowed_clip_band': [0.45, 0.70],
}

out = Path(__file__).resolve().parent / 'calibration_profile.json'
out.write_text(json.dumps(profile, indent=2) + '\n')
print(json.dumps(profile))
