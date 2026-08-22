from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import pandas as pd
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.metrics import mean_squared_error
from sklearn.model_selection import train_test_split


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', required=True)
    parser.add_argument('--out', default='artifacts/nocturia_trend.joblib')
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    features = df[['night_1', 'night_2', 'night_3', 'night_4', 'night_5', 'night_6', 'night_7', 'avg_sg_q1000']]
    target = df['next_7d_mean_nocturia']
    x_train, x_test, y_train, y_test = train_test_split(features, target, test_size=0.2, random_state=42)
    model = GradientBoostingRegressor(random_state=42)
    model.fit(x_train, y_train)
    pred = model.predict(x_test)
    rmse = mean_squared_error(y_test, pred) ** 0.5
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    joblib.dump({'model': model, 'rmse': rmse}, args.out)
    print({'rmse': round(float(rmse), 4), 'rows': len(df)})


if __name__ == '__main__':
    main()
