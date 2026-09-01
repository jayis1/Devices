from __future__ import annotations

import json
from pathlib import Path

profile_path = Path(__file__).with_name('calibration_profile.json')
profile = json.loads(profile_path.read_text())

summary = {
    'entry_margin_g': profile['entry_dock']['bag_hook_present_g'] - profile['entry_dock']['key_tray_empty_g'],
    'bag_security_window_m': profile['bag_tag']['separation_alert_m'],
    'mobility_warn_vector': [
        profile['mobility_beacon']['pm25_warn_ug_m3'],
        profile['mobility_beacon']['vibration_warn_rms'],
    ],
    'desk_hold_seconds': profile['desk_dock']['left_behind_timeout_s'],
}

print(json.dumps(summary, indent=2))
