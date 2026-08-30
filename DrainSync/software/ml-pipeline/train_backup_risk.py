from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
from sklearn.ensemble import GradientBoostingClassifier

OUT = Path(__file__).resolve().parent / 'artifacts'
OUT.mkdir(exist_ok=True)

rng = np.random.default_rng(99)
X = []
y = []
for _ in range(1000):
    level = rng.normal(280, 120)
    pressure = rng.normal(25, 30)
    surges = rng.integers(0, 30)
    rainfall = rng.uniform(0, 80)
    valve_health = rng.uniform(0, 1)
    label = int(level > 420 or pressure > 70 or (rainfall > 45 and surges > 15) or valve_health < 0.25)
    X.append([level, pressure, surges, rainfall, valve_health])
    y.append(label)

model = GradientBoostingClassifier(random_state=99)
model.fit(np.array(X), np.array(y))
joblib.dump(model, OUT / 'backup_risk.joblib')
print('saved', OUT / 'backup_risk.joblib')
