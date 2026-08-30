from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
from sklearn.ensemble import RandomForestClassifier

OUT = Path(__file__).resolve().parent / 'artifacts'
OUT.mkdir(exist_ok=True)

rng = np.random.default_rng(7)
X = []
y = []
for _ in range(900):
    duration = rng.normal(1600, 650)
    turbulence = rng.normal(150, 70)
    temp = rng.normal(36, 9)
    daily_events = rng.integers(3, 25)
    grease_index = rng.uniform(0, 1)
    label = int(duration > 2200 or (turbulence > 210 and grease_index > 0.55) or (temp > 45 and daily_events > 18))
    X.append([duration, turbulence, temp, daily_events, grease_index])
    y.append(label)

model = RandomForestClassifier(n_estimators=120, max_depth=7, random_state=7)
model.fit(np.array(X), np.array(y))
joblib.dump(model, OUT / 'clogcast.joblib')
print('saved', OUT / 'clogcast.joblib')
