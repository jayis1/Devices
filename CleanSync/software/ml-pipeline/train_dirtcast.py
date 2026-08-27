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

rooms = np.array(['kitchen', 'bathroom', 'entry', 'hall', 'bedroom'])
room_code = {name: idx for idx, name in enumerate(rooms)}
rows = []
for _ in range(800):
    room = RNG.choice(rooms)
    traffic = RNG.integers(5, 100)
    humidity = RNG.uniform(25, 85)
    rain = RNG.integers(0, 2)
    pets = RNG.integers(0, 2)
    last_clean_h = RNG.integers(1, 72)
    dust = 0.45 * traffic + 0.25 * humidity + 10 * rain + 12 * pets + 0.5 * last_clean_h + RNG.normal(0, 8)
    rows.append((room_code[room], traffic, humidity, rain, pets, last_clean_h, dust))
df = pd.DataFrame(rows, columns=['room', 'traffic', 'humidity', 'rain', 'pets', 'last_clean_h', 'dust_24h'])
X = df[['room', 'traffic', 'humidity', 'rain', 'pets', 'last_clean_h']]
y = df['dust_24h']
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
model = RandomForestRegressor(n_estimators=120, random_state=42)
model.fit(X_train, y_train)
preds = model.predict(X_test)
mae = mean_absolute_error(y_test, preds)
joblib.dump(model, ARTIFACTS / 'dirtcast_model.joblib')
df.to_csv(ARTIFACTS / 'dirtcast_synth.csv', index=False)
print({'model': 'dirtcast', 'mae': round(float(mae), 3), 'rows': len(df)})
