from __future__ import annotations

from pathlib import Path
import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier, RandomForestRegressor
from sklearn.linear_model import LinearRegression
from sklearn.metrics import accuracy_score, mean_absolute_error
from sklearn.model_selection import train_test_split

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)
RNG = np.random.default_rng(42)

rows = []
for _ in range(500):
    dirt = RNG.uniform(0, 100)
    occupancy = RNG.uniform(0, 100)
    quiet = RNG.integers(0, 2)
    battery = RNG.uniform(20, 100)
    reward = 0.7 * dirt - 0.5 * occupancy - 25 * quiet + 0.2 * battery + RNG.normal(0, 5)
    rows.append((dirt, occupancy, quiet, battery, reward))
df = pd.DataFrame(rows, columns=['dirt', 'occupancy', 'quiet', 'battery', 'reward'])
X = df[['dirt', 'occupancy', 'quiet', 'battery']]
y = df['reward']
model = LinearRegression().fit(X, y)
joblib.dump(model, ARTIFACTS / 'routebandit_model.joblib')
df.to_csv(ARTIFACTS / 'routebandit_synth.csv', index=False)
print({'model': 'routebandit', 'coef': model.coef_.round(3).tolist(), 'rows': len(df)})
