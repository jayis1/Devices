from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)

rng = np.random.default_rng(11)
n = 1000
seat_exits = rng.integers(0, 8, n)
noise_db = rng.normal(50, 10, n).clip(20, 90)
session_minutes = rng.integers(0, 150, n)
co2_proxy = rng.integers(350, 1800, n)
low_light = rng.integers(0, 2, n)

labels = np.full(n, 1)
labels[(seat_exits > 4) | (noise_db > 63)] = 0           # distracted
labels[(session_minutes > 50) | (low_light == 1)] = 2    # transition-needed
labels[(session_minutes > 95) & (seat_exits == 0)] = 3   # hyperfocus-risk

X = pd.DataFrame({
    'seat_exits': seat_exits,
    'noise_db': noise_db,
    'session_minutes': session_minutes,
    'co2_proxy': co2_proxy,
    'low_light': low_light,
})

X_train, X_test, y_train, y_test = train_test_split(X, labels, test_size=0.25, random_state=42)
model = RandomForestClassifier(n_estimators=180, max_depth=7, random_state=42)
model.fit(X_train, y_train)
report = classification_report(y_test, model.predict(X_test), output_dict=True)

joblib.dump(model, ARTIFACTS / 'transition_classifier.joblib')
X.assign(target=labels).to_csv(ARTIFACTS / 'transition_classifier_synth.csv', index=False)
print({'artifact': 'transition_classifier.joblib', 'weighted_f1': round(float(report['weighted avg']['f1-score']), 3), 'rows': int(n)})
