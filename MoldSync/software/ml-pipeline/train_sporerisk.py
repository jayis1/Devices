from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import pandas as pd


def main() -> None:
    rng = np.random.default_rng(11)
    df = pd.DataFrame({
        'wet_hours_7d': rng.integers(0, 70, 180),
        'voc_shift': rng.normal(0.0, 0.8, 180),
        'margin_violations': rng.integers(0, 20, 180),
    })
    df['spore_risk'] = np.clip(0.12 * df['wet_hours_7d'] + 1.4 * df['margin_violations'] + 6 * np.maximum(df['voc_shift'], 0), 0, 100)
    artifact = {
        'model': 'SporeRisk-TFT-baseline',
        'rows': int(len(df)),
        'mean_spore_risk': round(float(df['spore_risk'].mean()), 2),
        'p95_spore_risk': round(float(df['spore_risk'].quantile(0.95)), 2),
    }
    out = Path(__file__).resolve().parents[2] / 'artifacts'
    out.mkdir(exist_ok=True)
    (out / 'sporerisk_report.json').write_text(json.dumps(artifact, indent=2))
    print(json.dumps(artifact, indent=2))


if __name__ == '__main__':
    main()
