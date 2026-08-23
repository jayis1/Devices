from pathlib import Path
import joblib
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error

rng = np.random.default_rng(44)
rows = 1400
X = pd.DataFrame({
    "hrv_rmssd": rng.normal(36, 10, rows),
    "skin_temp": rng.normal(36.7, 0.25, rows),
    "flow_score": rng.uniform(0, 10, rows),
    "prior_pain": rng.uniform(0, 10, rows),
    "sleep_interruptions": rng.poisson(2.0, rows),
})
y = (6.5 - X["hrv_rmssd"] * 0.07 + X["flow_score"] * 0.35 + X["prior_pain"] * 0.42 + X["sleep_interruptions"] * 0.2 + rng.normal(0, 0.5, rows)).clip(0, 10)
Xtr, Xte, ytr, yte = train_test_split(X, y, test_size=0.25, random_state=7)
model = GradientBoostingRegressor(random_state=7)
model.fit(Xtr, ytr)
pred = model.predict(Xte)
mae = mean_absolute_error(yte, pred)
out = Path(__file__).resolve().parent / "artifacts"
out.mkdir(exist_ok=True)
joblib.dump(model, out / "cramp_forecast.joblib")
print({"model": "cramp_forecast", "mae": round(float(mae), 4), "artifact": str(out / 'cramp_forecast.joblib')})
