from pathlib import Path
import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score

rng = np.random.default_rng(45)
rows = 1300
X = pd.DataFrame({
    "avg_flow_score": rng.uniform(1, 10, rows),
    "cycle_duration_days": rng.integers(3, 10, rows),
    "resting_hr": rng.normal(67, 8, rows),
    "fatigue_score": rng.uniform(0, 10, rows),
    "headache_days": rng.integers(0, 7, rows),
})
logit = X["avg_flow_score"] * 0.45 + X["cycle_duration_days"] * 0.3 + (X["resting_hr"] - 60) * 0.08 + X["fatigue_score"] * 0.35 - 6.5
prob = 1 / (1 + np.exp(-logit))
y = (prob > 0.5).astype(int)
Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.25, random_state=7)
model = HistGradientBoostingClassifier(random_state=7)
model.fit(Xtr, ytr)
pred = model.predict_proba(Xte)[:, 1]
auc = roc_auc_score(yte, pred)
out = Path(__file__).resolve().parent / "artifacts"
out.mkdir(exist_ok=True)
joblib.dump(model, out / "iron_watch.joblib")
print({"model": "iron_watch", "auc": round(float(auc), 4), "artifact": str(out / 'iron_watch.joblib')})
