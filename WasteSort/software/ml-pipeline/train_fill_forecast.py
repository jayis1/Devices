from __future__ import annotations

import numpy as np
import pandas as pd
from sklearn.linear_model import LinearRegression
from sklearn.metrics import mean_absolute_error


rng = np.random.default_rng(5)
hours = np.arange(0, 24 * 14)
frame = pd.DataFrame({
    "hour": hours,
    "dow": hours % 7,
    "holiday": ((hours // 24) % 10 == 0).astype(int),
})
frame["fill_pct"] = np.clip(18 + 0.22 * frame["hour"] + 4.5 * frame["holiday"] + rng.normal(0, 3, len(frame)), 0, 100)
frame["target_next_day"] = frame["fill_pct"].shift(-24).fillna(method="ffill")

train = frame.iloc[:-48]
test = frame.iloc[-48:]
model = LinearRegression()
model.fit(train[["hour", "dow", "holiday", "fill_pct"]], train["target_next_day"])
preds = model.predict(test[["hour", "dow", "holiday", "fill_pct"]])
print({"mae": round(float(mean_absolute_error(test["target_next_day"], preds)), 3)})
