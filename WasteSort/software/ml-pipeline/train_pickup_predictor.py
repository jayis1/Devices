from __future__ import annotations

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import accuracy_score
from sklearn.model_selection import train_test_split

rng = np.random.default_rng(17)
rows = 600
frame = pd.DataFrame({
    "fill_pct": rng.integers(10, 100, rows),
    "wind_kph": rng.uniform(0, 40, rows),
    "rain_mm": rng.uniform(0, 20, rows),
    "curb_placed_early": rng.integers(0, 2, rows),
    "past_missed_rate": rng.uniform(0, 0.8, rows),
})
frame["target"] = ((frame["fill_pct"] > 85) & (frame["curb_placed_early"] == 0) | (frame["past_missed_rate"] > 0.55)).astype(int)
X_train, X_test, y_train, y_test = train_test_split(frame.drop(columns=["target"]), frame["target"], test_size=0.25, random_state=42)
model = RandomForestClassifier(n_estimators=220, random_state=42)
model.fit(X_train, y_train)
preds = model.predict(X_test)
print({"accuracy": round(float(accuracy_score(y_test, preds)), 4)})
