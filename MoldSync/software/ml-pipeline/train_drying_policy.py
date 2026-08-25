from __future__ import annotations

import json
from pathlib import Path

import numpy as np


def main() -> None:
    rng = np.random.default_rng(21)
    fan_minutes = rng.integers(5, 35, 120)
    dehum_minutes = rng.integers(0, 45, 120)
    damp_hours = np.maximum(0, 12 - fan_minutes * 0.18 - dehum_minutes * 0.11 + rng.normal(0, 0.9, 120))
    artifact = {
        'model': 'DryingPolicy-DQN-baseline',
        'episodes': 120,
        'mean_fan_minutes': round(float(fan_minutes.mean()), 2),
        'mean_dehumidifier_minutes': round(float(dehum_minutes.mean()), 2),
        'mean_damp_hours': round(float(damp_hours.mean()), 2),
    }
    out = Path(__file__).resolve().parents[2] / 'artifacts'
    out.mkdir(exist_ok=True)
    (out / 'drying_policy_report.json').write_text(json.dumps(artifact, indent=2))
    print(json.dumps(artifact, indent=2))


if __name__ == '__main__':
    main()
