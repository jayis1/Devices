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
for _ in range(600):
    detergent_ml = RNG.uniform(20, 400)
    jobs_week = RNG.uniform(1, 20)
    mop_ratio = RNG.uniform(0, 1)
    pet_home = RNG.integers(0, 2)
    days_left = detergent_ml / (8 + jobs_week * (0.6 + mop_ratio) + 3 * pet_home) + RNG.normal(0, 1.2)
    rows.append((detergent_ml, jobs_week, mop_ratio, pet_home, max(1, days_left)))
df = pd.DataFrame(rows, columns=['detergent_ml', 'jobs_week', 'mop_ratio', 'pet_home', 'days_left'])
X = df[['detergent_ml', 'jobs_week', 'mop_ratio', 'pet_home']]
y = df['days_left']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
model = RandomForestRegressor(n_estimators=120, random_state=42)
model.fit(X_train, y_train)
preds = model.predict(X_test)
mae = mean_absolute_error(y_test, preds)
joblib.dump(model, ARTIFACTS / 'supplyflow_model.joblib')
df.to_csv(ARTIFACTS / 'supplyflow_synth.csv', index=False)
print({'model': 'supplyflow', 'mae': round(float(mae), 3), 'rows': len(df)})
