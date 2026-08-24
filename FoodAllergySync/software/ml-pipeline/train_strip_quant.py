from __future__ import annotations

from pathlib import Path
import joblib
import pandas as pd
from sklearn.ensemble import RandomForestRegressor


def build_dataset() -> pd.DataFrame:
    return pd.DataFrame([
        {"white": 0.91, "blue": 0.88, "ir": 0.86, "temp_c": 22.0, "ratio": 0.02},
        {"white": 0.77, "blue": 0.64, "ir": 0.60, "temp_c": 22.0, "ratio": 0.28},
        {"white": 0.59, "blue": 0.43, "ir": 0.39, "temp_c": 24.0, "ratio": 0.61},
        {"white": 0.67, "blue": 0.52, "ir": 0.50, "temp_c": 21.0, "ratio": 0.42},
    ])


def main() -> None:
    df = build_dataset()
    X = df[["white", "blue", "ir", "temp_c"]]
    y = df["ratio"]
    model = RandomForestRegressor(n_estimators=32, random_state=42)
    model.fit(X, y)
    Path("artifacts").mkdir(exist_ok=True)
    joblib.dump(model, "artifacts/strip_quant_rf.joblib")
    print({"rows": len(df), "target": "ratio"})


if __name__ == "__main__":
    main()
