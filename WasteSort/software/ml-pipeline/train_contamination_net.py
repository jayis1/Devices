from __future__ import annotations

import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.metrics import roc_auc_score
from sklearn.model_selection import train_test_split


rng = np.random.default_rng(11)
rows = 500
frame = pd.DataFrame({
    "residue_score": rng.uniform(0, 1, rows),
    "confidence": rng.uniform(0.4, 0.99, rows),
    "municipality_strictness": rng.uniform(0, 1, rows),
    "household_error_rate": rng.uniform(0, 1, rows),
})
frame["target"] = ((frame["residue_score"] * 0.5 + frame["municipality_strictness"] * 0.3 + frame["household_error_rate"] * 0.2) > 0.56).astype(int)

X_train, X_test, y_train, y_test = train_test_split(frame.drop(columns=["target"]), frame["target"], test_size=0.25, random_state=42)
model = GradientBoostingClassifier(random_state=42)
model.fit(X_train, y_train)
proba = model.predict_proba(X_test)[:, 1]
print({"roc_auc": round(float(roc_auc_score(y_test, proba)), 4)})
