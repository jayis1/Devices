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
for _ in range(900):
    reflectance_delta = RNG.uniform(0, 1)
    humidity = RNG.uniform(20, 95)
    occupancy = RNG.uniform(0, 100)
    post_clean = RNG.integers(0, 2)
    tile = RNG.integers(0, 2)
    score = 2.4 * reflectance_delta + 0.02 * humidity + 0.015 * occupancy + 0.5 * post_clean + 0.35 * tile
    label = int(score > 2.2)
    rows.append((reflectance_delta, humidity, occupancy, post_clean, tile, label))
df = pd.DataFrame(rows, columns=['reflectance_delta', 'humidity', 'occupancy', 'post_clean', 'tile', 'label'])
X = df.drop(columns=['label'])
y = df['label']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
model = RandomForestClassifier(n_estimators=120, random_state=42)
model.fit(X_train, y_train)
preds = model.predict(X_test)
acc = accuracy_score(y_test, preds)
joblib.dump(model, ARTIFACTS / 'sliprisk_model.joblib')
df.to_csv(ARTIFACTS / 'sliprisk_synth.csv', index=False)
print({'model': 'sliprisk', 'accuracy': round(float(acc), 3), 'rows': len(df)})
