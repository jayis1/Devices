from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split


def synthetic(rows: int = 1200) -> pd.DataFrame:
    rng = np.random.default_rng(9)
    df = pd.DataFrame({
        'reserve_minutes': rng.integers(10, 480, rows),
        'watts': rng.integers(5, 1600, rows),
        'priority': rng.integers(0, 6, rows),
        'medical': rng.integers(0, 2, rows),
        'thermal_load': rng.integers(0, 2, rows),
        'occupancy': rng.integers(0, 2, rows),
    })
    labels = []
    for row in df.itertuples(index=False):
        if row.medical:
            labels.append('keep_on')
        elif row.reserve_minutes < 45 and row.priority >= 1:
            labels.append('shed')
        elif row.reserve_minutes < 150 and row.watts > 120 and row.priority >= 2:
            labels.append('cycle')
        else:
            labels.append('keep_on')
    df['label'] = labels
    return df


def main() -> None:
    out_dir = Path(__file__).with_name('artifacts')
    out_dir.mkdir(exist_ok=True)
    df = synthetic()
    X = df.drop(columns=['label'])
    y = df['label']
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=11, stratify=y)
    model = RandomForestClassifier(n_estimators=160, max_depth=9, random_state=11)
    model.fit(X_train, y_train)
    preds = model.predict(X_test)
    print(classification_report(y_test, preds))
    joblib.dump(model, out_dir / 'load_shed_policy.joblib')
    df.to_csv(out_dir / 'load_shed_policy_synth.csv', index=False)


if __name__ == '__main__':
    main()
