from __future__ import annotations

import json
from pathlib import Path

BASELINES = {
    "temp_patch_skin_offset_c": -0.18,
    "flow_clip_dry_capacitance": 7124,
    "relief_belt_left_offset_c": 0.21,
    "relief_belt_right_offset_c": 0.17,
    "strip_reader_white_reference": 0.982,
}


def main() -> None:
    out = Path(__file__).resolve().parent / "periodsync_calibration.json"
    out.write_text(json.dumps(BASELINES, indent=2), encoding="utf-8")
    print(f"wrote calibration to {out}")


if __name__ == "__main__":
    main()
