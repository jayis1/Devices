from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import IsolationForest


def synthetic(rows: int = 700) -> pd.DataFrame:
    rng = np.random.default_rng(21)
    return pd.DataFrame({
        'vibration_rms': rng.normal(1.1, 0.18, rows),
        'start_seconds': rng.normal(2.4, 0.5, rows),
        'fuel_level_pct': rng.uniform(15, 100, rows),
        'enclosure_temp_c': rng.normal(31, 6, rows),
        'runtime_minutes': rng.integers(5, 300, rows),
    })


def main() -> None:
    out_dir = Path(__file__).with_name('artifacts')
    out_dir.mkdir(exist_ok=True)
    df = synthetic()
    model = IsolationForest(n_estimators=160, contamination=0.06, random_state=21)
    model.fit(df)
    scores = model.decision_function(df)
    joblib.dump(model, out_dir / 'generator_anomaly.joblib')
    df.assign(score=scores).to_csv(out_dir / 'generator_anomaly_synth.csv', index=False)
    print({'score_mean': round(float(scores.mean()), 4), 'score_min': round(float(scores.min()), 4)})


if __name__ == '__main__':
    main()
