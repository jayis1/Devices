from __future__ import annotations

import json
from pathlib import Path


MATERIALS = {
    'painted_drywall': 1.00,
    'tile_over_cement_board': 0.82,
    'cabinet_plywood': 1.14,
}


def main() -> None:
    report = {
        'materials': MATERIALS,
        'notes': 'Use dry baseline room, wait 10 minutes after mounting electrodes, capture three medians per surface.',
    }
    out = Path(__file__).resolve().parent / 'artifacts'
    out.mkdir(exist_ok=True)
    (out / 'calibration_report.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
