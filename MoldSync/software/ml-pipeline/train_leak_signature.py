from __future__ import annotations

import json
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier


def main() -> None:
    rng = np.random.default_rng(7)
    rows = 240
    frame = pd.DataFrame({
        'flow_ml_min': rng.normal(90, 50, rows).clip(0),
        'leak_signal': rng.uniform(0, 1, rows),
        'co2_ppm': rng.normal(650, 180, rows).clip(350),
        'label': np.where(rng.uniform(0, 1, rows) > 0.7, 1, 0),
    })
    model = RandomForestClassifier(n_estimators=40, random_state=7)
    model.fit(frame[['flow_ml_min', 'leak_signal', 'co2_ppm']], frame['label'])
    artifact = {
        'model': 'LeakSignature-baseline',
        'rows': rows,
        'feature_importances': dict(zip(['flow_ml_min', 'leak_signal', 'co2_ppm'], model.feature_importances_.round(3).tolist())),
    }
    out = Path(__file__).resolve().parents[2] / 'artifacts'
    out.mkdir(exist_ok=True)
    (out / 'leak_signature_report.json').write_text(json.dumps(artifact, indent=2))
    print(json.dumps(artifact, indent=2))


if __name__ == '__main__':
    main()
