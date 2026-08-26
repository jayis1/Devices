from __future__ import annotations

from pathlib import Path

import joblib
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split

data_path = Path(__file__).resolve().parent / 'artifacts' / 'mobilitysync_synth.csv'
df = pd.read_csv(data_path)
X = df[['asymmetry_pct', 'unload_rate', 'retries', 'grip_force_n', 'hrv_proxy']]
y = df['transfer_label']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
model = RandomForestClassifier(n_estimators=180, random_state=42)
model.fit(X_train, y_train)
pred = model.predict(X_test)
print(classification_report(y_test, pred))
out = Path(__file__).resolve().parent / 'artifacts' / 'transfer_model.joblib'
joblib.dump(model, out)
print(f'saved {out}')
