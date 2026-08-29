from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.metrics import mean_absolute_error
from sklearn.model_selection import train_test_split


def synthetic(rows: int = 800) -> pd.DataFrame:
    rng = np.random.default_rng(42)
    df = pd.DataFrame({
        'vrms': rng.normal(226, 9, rows),
        'freq_hz': rng.normal(59.9, 0.18, rows),
        'thd_pct': rng.uniform(1.4, 8.0, rows),
        'storm_index': rng.uniform(0, 1, rows),
        'grid_stress': rng.uniform(0, 1, rows),
        'battery_soc': rng.uniform(15, 100, rows),
    })
    df['outage_minutes'] = (
        20
        + (230 - df['vrms']).clip(lower=0) * 2.8
        + (60 - df['freq_hz']).clip(lower=0) * 140
        + df['thd_pct'] * 11
        + df['storm_index'] * 180
        + df['grid_stress'] * 130
        + (100 - df['battery_soc']) * 0.35
        + rng.normal(0, 18, rows)
    ).clip(15, 1440)
    return df


def main() -> None:
    out_dir = Path(__file__).with_name('artifacts')
    out_dir.mkdir(exist_ok=True)
    df = synthetic()
    X = df.drop(columns=['outage_minutes'])
    y = df['outage_minutes']
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=7)
    model = RandomForestRegressor(n_estimators=220, max_depth=10, random_state=7)
    model.fit(X_train, y_train)
    preds = model.predict(X_test)
    mae = mean_absolute_error(y_test, preds)
    joblib.dump(model, out_dir / 'outage_duration.joblib')
    df.to_csv(out_dir / 'outage_duration_synth.csv', index=False)
    print({'mae_minutes': round(float(mae), 2), 'rows': len(df)})


if __name__ == '__main__':
    main()
