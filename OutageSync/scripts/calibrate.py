from __future__ import annotations

import json
from pathlib import Path


def main() -> None:
    profile = {
        'battery_floor_pct': 22,
        'generator_start_min_reserve': 75,
        'medical_outlets': ['CPAP', 'Nebulizer', 'Infusion Cooler'],
        'quiet_hours': ['22:00', '07:00'],
    }
    out = Path(__file__).with_name('calibration_profile.json')
    out.write_text(json.dumps(profile, indent=2))
    print(out.resolve())


if __name__ == '__main__':
    main()
