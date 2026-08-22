from __future__ import annotations

import argparse
import json
from statistics import mean


def main() -> None:
    parser = argparse.ArgumentParser(description='Generate calibration constants for UroSync nodes.')
    parser.add_argument('--strip-white-ref', nargs='+', type=float, required=True)
    parser.add_argument('--loadcell-zero', nargs='+', type=float, required=True)
    args = parser.parse_args()

    result = {
        'strip_white_reference': round(mean(args.strip_white_ref), 3),
        'loadcell_zero_offset': round(mean(args.loadcell_zero), 3),
    }
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
