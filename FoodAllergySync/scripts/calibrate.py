from __future__ import annotations

import json
from pathlib import Path


def main() -> None:
    report = {
        "strip_reader_reference": {
            "white_tile_baseline": 0.98,
            "blue_baseline": 0.96,
            "ir_baseline": 0.95,
        },
        "meal_scanner_focus_distance_mm": 180,
        "safe_lunch_temp_offset_c": -0.2,
        "epipen_guard_temp_offset_c": 0.1,
    }
    out_dir = Path(__file__).resolve().parent / "artifacts"
    out_dir.mkdir(exist_ok=True)
    out_file = out_dir / "calibration_report.json"
    out_file.write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
