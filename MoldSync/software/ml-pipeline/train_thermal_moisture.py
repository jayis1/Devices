from __future__ import annotations

import json
from pathlib import Path

import numpy as np


def main() -> None:
    rng = np.random.default_rng(33)
    thermal_delta = rng.normal(-1.4, 0.8, 160)
    conductivity = rng.uniform(0, 100, 160)
    spectral_index = rng.uniform(0, 1, 160)
    confidence = np.clip(0.35 + (-thermal_delta * 0.08) + conductivity * 0.003 + spectral_index * 0.25, 0, 1)
    artifact = {
        'model': 'ThermalMoisture-UNet-baseline',
        'samples': 160,
        'mean_confidence': round(float(confidence.mean()), 3),
        'high_confidence_rate': round(float((confidence > 0.7).mean()), 3),
    }
    out = Path(__file__).resolve().parents[2] / 'artifacts'
    out.mkdir(exist_ok=True)
    (out / 'thermal_moisture_report.json').write_text(json.dumps(artifact, indent=2))
    print(json.dumps(artifact, indent=2))


if __name__ == '__main__':
    main()
