from __future__ import annotations

from pathlib import Path
import joblib
import pandas as pd
from sklearn.compose import ColumnTransformer
from sklearn.ensemble import RandomForestClassifier
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import OneHotEncoder


def build_dataset() -> pd.DataFrame:
    return pd.DataFrame([
        {"zone": "counter", "hours_since_clean": 1, "strip_ratio": 0.05, "appliance": "knife", "risk": 0},
        {"zone": "counter", "hours_since_clean": 8, "strip_ratio": 0.38, "appliance": "board", "risk": 1},
        {"zone": "toaster", "hours_since_clean": 72, "strip_ratio": 0.61, "appliance": "toaster", "risk": 1},
        {"zone": "prep_sink", "hours_since_clean": 2, "strip_ratio": 0.02, "appliance": "colander", "risk": 0},
    ])


def main() -> None:
    df = build_dataset()
    X = df[["zone", "hours_since_clean", "strip_ratio", "appliance"]]
    y = df["risk"]
    model = Pipeline([
        ("prep", ColumnTransformer([
            ("cat", OneHotEncoder(handle_unknown="ignore"), ["zone", "appliance"]),
        ], remainder="passthrough")),
        ("rf", RandomForestClassifier(n_estimators=32, random_state=42)),
    ])
    model.fit(X, y)
    Path("artifacts").mkdir(exist_ok=True)
    joblib.dump(model, "artifacts/cross_contact_rf.joblib")
    print({"rows": len(df), "features": list(X.columns)})


if __name__ == "__main__":
    main()
