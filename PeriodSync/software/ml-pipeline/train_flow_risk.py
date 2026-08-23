from pathlib import Path
import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score

rng = np.random.default_rng(43)
rows = 1600
X = pd.DataFrame({
    "cap_delta": rng.normal(180, 75, rows),
    "humidity": rng.uniform(25, 95, rows),
    "posture": rng.integers(0, 3, rows),
    "cycle_day": rng.integers(1, 8, rows),
    "heavy_history": rng.uniform(0, 1, rows),
})
logit = (X["cap_delta"] * 0.012 + X["humidity"] * 0.03 + X["posture"] * 0.5 + X["heavy_history"] * 2.0 - 5.0)
prob = 1 / (1 + np.exp(-logit))
y = (prob > 0.5).astype(int)
Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.25, random_state=7)
model = RandomForestClassifier(n_estimators=120, random_state=7)
model.fit(Xtr, ytr)
pred = model.predict_proba(Xte)[:, 1]
auc = roc_auc_score(yte, pred)
out = Path(__file__).resolve().parent / "artifacts"
out.mkdir(exist_ok=True)
joblib.dump(model, out / "flow_risk.joblib")
print({"model": "flow_risk", "auc": round(float(auc), 4), "artifact": str(out / 'flow_risk.joblib')})
