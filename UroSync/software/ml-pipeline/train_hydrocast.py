from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.metrics import mean_absolute_error
from sklearn.model_selection import train_test_split


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', required=True)
    parser.add_argument('--out', default='artifacts/hydrocast.joblib')
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    features = df[['consumed_ml_day', 'sg_q1000', 'humidity_pct', 'nocturia_count']]
    target = df['dehydration_risk_24h']
    x_train, x_test, y_train, y_test = train_test_split(features, target, test_size=0.2, random_state=42)
    model = RandomForestRegressor(n_estimators=160, random_state=42)
    model.fit(x_train, y_train)
    preds = model.predict(x_test)
    mae = mean_absolute_error(y_test, preds)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    joblib.dump({'model': model, 'mae': mae}, args.out)
    print({'mae': round(float(mae), 4), 'rows': len(df)})


if __name__ == '__main__':
    main()
