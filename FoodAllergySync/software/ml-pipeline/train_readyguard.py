from __future__ import annotations

from pathlib import Path
import joblib
import pandas as pd
from sklearn.ensemble import RandomForestClassifier


def build_dataset() -> pd.DataFrame:
    return pd.DataFrame([
        {"weekday": 1, "present": 1, "temp_excursions": 0, "days_to_expiry": 120, "missed": 0},
        {"weekday": 1, "present": 0, "temp_excursions": 0, "days_to_expiry": 120, "missed": 1},
        {"weekday": 5, "present": 1, "temp_excursions": 3, "days_to_expiry": 8, "missed": 1},
        {"weekday": 6, "present": 1, "temp_excursions": 0, "days_to_expiry": 200, "missed": 0},
    ])


def main() -> None:
    df = build_dataset()
    X = df[["weekday", "present", "temp_excursions", "days_to_expiry"]]
    y = df["missed"]
    model = RandomForestClassifier(n_estimators=32, random_state=42)
    model.fit(X, y)
    Path("artifacts").mkdir(exist_ok=True)
    joblib.dump(model, "artifacts/readyguard_rf.joblib")
    print({"rows": len(df), "positive_rate": float(y.mean())})


if __name__ == "__main__":
    main()
