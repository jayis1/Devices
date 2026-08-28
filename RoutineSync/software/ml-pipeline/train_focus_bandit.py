from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import f1_score
from sklearn.model_selection import train_test_split

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)

rng = np.random.default_rng(9)
n = 700
state = rng.integers(0, 4, n)
light = rng.integers(50, 900, n)
noise = rng.normal(48, 11, n).clip(20, 85)
annoyance_budget = rng.random(n)
action = rng.integers(0, 5, n)

ideal_action = np.select(
    [state == 0, state == 1, state == 2, state == 3],
    [0, 1, 3, 4],
    default=2,
)
reward = (action == ideal_action).astype(int)
reward |= ((state == 1) & (noise > 60) & (action == 2)).astype(int)
reward |= ((state == 2) & (annoyance_budget < 0.3) & (action == 0)).astype(int)
reward |= ((state == 3) & (light < 180) & (action == 3)).astype(int)

X = pd.DataFrame({
    'state': state,
    'light': light,
    'noise': noise,
    'annoyance_budget': annoyance_budget,
    'action': action,
})

X_train, X_test, y_train, y_test = train_test_split(X, reward, test_size=0.25, random_state=42)
model = RandomForestClassifier(n_estimators=160, max_depth=6, random_state=42)
model.fit(X_train, y_train)
pred = model.predict(X_test)
f1 = f1_score(y_test, pred)

joblib.dump(model, ARTIFACTS / 'focus_bandit_baseline.joblib')
X.assign(target=reward).to_csv(ARTIFACTS / 'focus_bandit_synth.csv', index=False)
print({'artifact': 'focus_bandit_baseline.joblib', 'f1': round(float(f1), 3), 'rows': int(n)})
