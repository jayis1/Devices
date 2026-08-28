from __future__ import annotations

import json
from pathlib import Path


def main() -> None:
    output = {
        'doorway_dock': {
            'tray_zero_offset_g': 12.4,
            'badge_nfc_threshold': 0.82,
            'uwb_anchor_offset_cm': 6.0,
        },
        'focus_beacon': {
            'noise_floor_db': 36.5,
            'light_floor_lux': 145,
            'voc_baseline': 83,
        },
    }
    path = Path(__file__).resolve().with_name('calibration_profile.json')
    path.write_text(json.dumps(output, indent=2) + '\n', encoding='utf-8')
    print(f'wrote {path}')


if __name__ == '__main__':
    main()
