from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import roc_auc_score
from sklearn.model_selection import train_test_split

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)

rng = np.random.default_rng(42)
n = 800
missing_items = rng.integers(0, 4, n)
tray_delta = rng.normal(95, 55, n).clip(0, 300)
minutes_to_deadline = rng.integers(0, 45, n)
door_open = rng.integers(0, 2, n)
focus_fragmentation = rng.random(n)
late_week = rng.integers(0, 2, n)

logit = (
    -1.7
    + 0.75 * missing_items
    + 0.006 * tray_delta
    + 0.55 * (minutes_to_deadline < 10)
    + 0.45 * door_open
    + 0.9 * focus_fragmentation
    + 0.35 * late_week
)
prob = 1 / (1 + np.exp(-logit))
y = (rng.random(n) < prob).astype(int)

X = pd.DataFrame({
    'missing_items': missing_items,
    'tray_delta': tray_delta,
    'minutes_to_deadline': minutes_to_deadline,
    'door_open': door_open,
    'focus_fragmentation': focus_fragmentation,
    'late_week': late_week,
})

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.25, random_state=42)
model = RandomForestClassifier(n_estimators=160, max_depth=6, random_state=42)
model.fit(X_train, y_train)
auc = roc_auc_score(y_test, model.predict_proba(X_test)[:, 1])

joblib.dump(model, ARTIFACTS / 'routine_risk_model.joblib')
X.assign(target=y).to_csv(ARTIFACTS / 'routine_risk_synth.csv', index=False)
print({'artifact': 'routine_risk_model.joblib', 'auc': round(float(auc), 3), 'rows': int(n)})
