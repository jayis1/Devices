from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import pandas as pd
from sklearn.metrics import roc_auc_score
from sklearn.model_selection import train_test_split
from xgboost import XGBClassifier


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', required=True)
    parser.add_argument('--out', default='artifacts/utiwatch.joblib')
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    features = df[['leukocyte', 'nitrite', 'blood', 'protein', 'sg_q1000', 'voids_day', 'nocturia_count']]
    target = df['uti_label']
    x_train, x_test, y_train, y_test = train_test_split(features, target, test_size=0.25, random_state=42, stratify=target)
    model = XGBClassifier(max_depth=4, n_estimators=120, learning_rate=0.08, eval_metric='logloss')
    model.fit(x_train, y_train)
    proba = model.predict_proba(x_test)[:, 1]
    auc = roc_auc_score(y_test, proba)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    joblib.dump({'model': model, 'auc': auc}, args.out)
    print({'auc': round(float(auc), 4), 'rows': len(df)})


if __name__ == '__main__':
    main()
