from __future__ import annotations

import argparse


def calibrate_load_cell(raw_empty: int, raw_full: int, known_mass_g: int) -> dict[str, float]:
    if raw_full == raw_empty:
        raise ValueError("raw_full must differ from raw_empty")
    scale = known_mass_g / float(raw_full - raw_empty)
    offset = raw_empty
    return {"scale_g_per_count": scale, "offset": float(offset)}


def main() -> None:
    parser = argparse.ArgumentParser(description="WasteSort bin dock calibration helper")
    parser.add_argument("--raw-empty", type=int, required=True)
    parser.add_argument("--raw-full", type=int, required=True)
    parser.add_argument("--known-mass-g", type=int, required=True)
    args = parser.parse_args()
    print(calibrate_load_cell(args.raw_empty, args.raw_full, args.known_mass_g))


if __name__ == "__main__":
    main()
