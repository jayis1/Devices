from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.metrics import mean_squared_error
from sklearn.model_selection import train_test_split


def synthetic(rows: int = 900) -> pd.DataFrame:
    rng = np.random.default_rng(13)
    df = pd.DataFrame({
        'product_temp_c': rng.uniform(-20, 9, rows),
        'ambient_temp_c': rng.uniform(15, 35, rows),
        'door_open_seconds': rng.integers(0, 600, rows),
        'volume_liters': rng.uniform(8, 400, rows),
        'freezer_mode': rng.integers(0, 2, rows),
    })
    df['hold_minutes'] = (
        np.where(df['freezer_mode'] == 1, 1600, 260)
        - np.maximum(df['product_temp_c'], -2) * 18
        - df['door_open_seconds'] * 0.7
        + df['volume_liters'] * 0.5
        + rng.normal(0, 35, rows)
    ).clip(0, 2000)
    return df


def main() -> None:
    out_dir = Path(__file__).with_name('artifacts')
    out_dir.mkdir(exist_ok=True)
    df = synthetic()
    X = df.drop(columns=['hold_minutes'])
    y = df['hold_minutes']
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=3)
    model = GradientBoostingRegressor(random_state=3)
    model.fit(X_train, y_train)
    preds = model.predict(X_test)
    rmse = float(mean_squared_error(y_test, preds) ** 0.5)
    joblib.dump(model, out_dir / 'cold_guard.joblib')
    df.to_csv(out_dir / 'cold_guard_synth.csv', index=False)
    print({'rmse_minutes': round(rmse, 2)})


if __name__ == '__main__':
    main()
