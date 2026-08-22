from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', required=True)
    parser.add_argument('--out', default='artifacts/nightsafe.joblib')
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    features = df[['transfer_latency_ms', 'sway_index', 'humidity_pct', 'left_temp_delta_c', 'trip_duration_s']]
    target = df['fall_risk_high']
    x_train, x_test, y_train, y_test = train_test_split(features, target, test_size=0.2, random_state=42, stratify=target)
    model = RandomForestClassifier(n_estimators=200, random_state=42)
    model.fit(x_train, y_train)
    report = classification_report(y_test, model.predict(x_test), output_dict=True)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    joblib.dump({'model': model, 'report': report}, args.out)
    print({'macro_f1': round(report['macro avg']['f1-score'], 4), 'rows': len(df)})


if __name__ == '__main__':
    main()
