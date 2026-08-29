from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split


def synthetic(rows: int = 850) -> pd.DataFrame:
    rng = np.random.default_rng(33)
    df = pd.DataFrame({
        'room_temp_c': rng.uniform(17, 34, rows),
        'humidity_pct': rng.uniform(25, 85, rows),
        'outside_temp_c': rng.uniform(-5, 41, rows),
        'solar_gain': rng.uniform(0, 1, rows),
        'occupancy': rng.integers(0, 2, rows),
        'fan_available': rng.integers(0, 2, rows),
    })
    df['comfort_minutes'] = (
        540
        - np.abs(df['room_temp_c'] - 23) * 24
        - np.abs(df['humidity_pct'] - 50) * 2.2
        - np.maximum(df['outside_temp_c'] - 28, 0) * 6
        - df['solar_gain'] * 50
        + df['fan_available'] * 55
        - df['occupancy'] * 18
        + rng.normal(0, 22, rows)
    ).clip(20, 720)
    return df


def main() -> None:
    out_dir = Path(__file__).with_name('artifacts')
    out_dir.mkdir(exist_ok=True)
    df = synthetic()
    X = df.drop(columns=['comfort_minutes'])
    y = df['comfort_minutes']
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=5)
    model = RandomForestRegressor(n_estimators=180, random_state=5)
    model.fit(X_train, y_train)
    score = model.score(X_test, y_test)
    joblib.dump(model, out_dir / 'comfort_decay.joblib')
    df.to_csv(out_dir / 'comfort_decay_synth.csv', index=False)
    print({'r2': round(float(score), 3), 'rows': len(df)})


if __name__ == '__main__':
    main()
