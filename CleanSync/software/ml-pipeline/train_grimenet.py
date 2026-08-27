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

classes = np.array(['clean', 'dust', 'soap', 'grease', 'biofilm', 'mildew-risk'])
class_code = {name: idx for idx, name in enumerate(classes)}
rows = []
for _ in range(1200):
    fluorescence = RNG.uniform(0, 100)
    blue = RNG.uniform(0, 1)
    green = RNG.uniform(0, 1)
    nir = RNG.uniform(0, 1)
    texture = RNG.uniform(0, 1)
    humidity = RNG.uniform(20, 90)
    score = 0
    if fluorescence < 15 and texture < 0.3:
        label = 'clean'
    elif humidity > 75 and green > 0.55:
        label = 'mildew-risk'
    elif fluorescence > 70 and nir > 0.55:
        label = 'grease'
    elif blue > 0.6 and texture < 0.45:
        label = 'soap'
    elif texture > 0.65:
        label = 'dust'
    else:
        label = 'biofilm'
    rows.append((fluorescence, blue, green, nir, texture, humidity, class_code[label]))
df = pd.DataFrame(rows, columns=['fluorescence', 'blue', 'green', 'nir', 'texture', 'humidity', 'label'])
X = df.drop(columns=['label'])
y = df['label']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
model = RandomForestClassifier(n_estimators=140, random_state=42)
model.fit(X_train, y_train)
preds = model.predict(X_test)
acc = accuracy_score(y_test, preds)
joblib.dump({'model': model, 'classes': list(classes)}, ARTIFACTS / 'grimenet_model.joblib')
df.to_csv(ARTIFACTS / 'grimenet_synth.csv', index=False)
print({'model': 'grimenet', 'accuracy': round(float(acc), 3), 'rows': len(df)})
