from __future__ import annotations

import json

def compute_handle_offset(raw_left: list[float], raw_right: list[float]) -> dict:
    left = sum(raw_left) / max(1, len(raw_left))
    right = sum(raw_right) / max(1, len(raw_right))
    return {'left_zero': round(left, 3), 'right_zero': round(right, 3)}

if __name__ == '__main__':
    sample = compute_handle_offset([0.12, 0.10, 0.11], [0.15, 0.14, 0.15])
    print(json.dumps(sample, indent=2))
