from __future__ import annotations

from pathlib import Path

import joblib
import pandas as pd
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.metrics import accuracy_score
from sklearn.model_selection import train_test_split

data_path = Path(__file__).resolve().parent / 'artifacts' / 'mobilitysync_synth.csv'
df = pd.read_csv(data_path)
X = df[['door_range_m', 'obstruction', 'slip_score', 'fatigue_score']]
y = df['door_open_label']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
model = HistGradientBoostingClassifier(random_state=42)
model.fit(X_train, y_train)
pred = model.predict(X_test)
print({'accuracy': round(accuracy_score(y_test, pred), 3)})
out = Path(__file__).resolve().parent / 'artifacts' / 'route_model.joblib'
joblib.dump(model, out)
print(f'saved {out}')
