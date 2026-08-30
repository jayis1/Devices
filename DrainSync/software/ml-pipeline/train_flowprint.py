from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
from sklearn.linear_model import LogisticRegression

OUT = Path(__file__).resolve().parent / 'artifacts'
OUT.mkdir(exist_ok=True)

rng = np.random.default_rng(42)
X = []
y = []
for _ in range(800):
    duration = rng.normal(1500, 600)
    turbulence = rng.normal(140, 60)
    vibration = rng.normal(70, 30)
    gas = rng.normal(25, 10)
    reverse = rng.integers(0, 2)
    label = int(duration > 2100 or turbulence > 220 or gas > 40 or reverse == 1)
    X.append([duration, turbulence, vibration, gas, reverse])
    y.append(label)

model = LogisticRegression(max_iter=1000)
model.fit(np.array(X), np.array(y))
joblib.dump(model, OUT / 'flowprint.joblib')
print('saved', OUT / 'flowprint.joblib')
