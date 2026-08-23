from pathlib import Path
import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score

rng = np.random.default_rng(42)
rows = 1200
X = pd.DataFrame({
    "skin_temp_delta": rng.normal(0.15, 0.22, rows),
    "hrv_rmssd": rng.normal(38, 12, rows),
    "cycle_len_var": rng.normal(2.8, 1.4, rows),
    "lh_intensity": rng.uniform(0, 1, rows),
    "sleep_interruptions": rng.poisson(1.5, rows),
})
y = ((X["skin_temp_delta"] > 0.12) & (X["lh_intensity"] > 0.55)).astype(int)
Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.25, random_state=7)
model = GradientBoostingClassifier(random_state=7)
model.fit(Xtr, ytr)
pred = model.predict_proba(Xte)[:, 1]
auc = roc_auc_score(yte, pred)
out = Path(__file__).resolve().parent / "artifacts"
out.mkdir(exist_ok=True)
joblib.dump(model, out / "phase_forecast.joblib")
print({"model": "phase_forecast", "auc": round(float(auc), 4), "artifact": str(out / 'phase_forecast.joblib')})
