from __future__ import annotations

from pathlib import Path

import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.metrics import accuracy_score
from sklearn.model_selection import train_test_split

ARTIFACTS = Path(__file__).resolve().parent / 'artifacts'
ARTIFACTS.mkdir(exist_ok=True)

rng = np.random.default_rng(7)
rooms = np.array(['entryway', 'office', 'bedroom', 'kitchen', 'car'])
room_index = rng.integers(0, len(rooms), 900)
minutes_since_seen = rng.integers(0, 300, 900)
movement_events = rng.integers(0, 8, 900)
doorway_seen = rng.integers(0, 2, 900)
active_routine = rng.integers(0, 4, 900)

labels = room_index.copy()
labels[(doorway_seen == 1) & (minutes_since_seen < 40)] = 0
labels[(active_routine == 1) & (movement_events < 2)] = 1

X = pd.DataFrame({
    'room_index': room_index,
    'minutes_since_seen': minutes_since_seen,
    'movement_events': movement_events,
    'doorway_seen': doorway_seen,
    'active_routine': active_routine,
})

X_train, X_test, y_train, y_test = train_test_split(X, labels, test_size=0.25, random_state=42)
model = GradientBoostingClassifier(random_state=42)
model.fit(X_train, y_train)
acc = accuracy_score(y_test, model.predict(X_test))

joblib.dump({'model': model, 'rooms': rooms.tolist()}, ARTIFACTS / 'item_presence_model.joblib')
X.assign(target=labels).to_csv(ARTIFACTS / 'item_presence_synth.csv', index=False)
print({'artifact': 'item_presence_model.joblib', 'accuracy': round(float(acc), 3), 'rows': 900})
