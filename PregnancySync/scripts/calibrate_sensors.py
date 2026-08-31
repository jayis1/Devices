from __future__ import annotations

import json
from pathlib import Path

profile = {
    'band': {'imu_bias_mg': [3, -2, 5], 'temp_offset_c': -0.14},
    'cuff': {'pressure_offset_mmHg': -1.8, 'pump_gain': 1.04},
    'strip_reader': {'white_reference': [0.98, 1.01, 1.00, 0.99], 'tray_mass_g': 12.4},
    'sleep_pad': {'bcg_gain': 1.12, 'supine_threshold': 0.64},
}

out = Path(__file__).with_name('calibration_profile.json')
out.write_text(json.dumps(profile, indent=2) + '\n')
print(out)
