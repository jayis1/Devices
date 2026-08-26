from __future__ import annotations

from pathlib import Path

import joblib
import pandas as pd
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.metrics import mean_absolute_error
from sklearn.model_selection import train_test_split

data_path = Path(__file__).resolve().parent / 'artifacts' / 'mobilitysync_synth.csv'
df = pd.read_csv(data_path)
X = df[['hr_bpm', 'hrv_proxy', 'transfer_count', 'slip_score', 'grip_force_n']]
y = df['fatigue_score']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
model = GradientBoostingRegressor(random_state=42)
model.fit(X_train, y_train)
pred = model.predict(X_test)
print({'mae': round(mean_absolute_error(y_test, pred), 3)})
out = Path(__file__).resolve().parent / 'artifacts' / 'fatigue_model.joblib'
joblib.dump(model, out)
print(f'saved {out}')
