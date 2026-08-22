from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split


def load_dataset(path: Path) -> pd.DataFrame:
    if path.exists():
        return pd.read_csv(path)
    rng = np.random.default_rng(7)
    rows = 400
    df = pd.DataFrame({
        "rgb_mean": rng.uniform(0.1, 0.9, rows),
        "nir_mean": rng.uniform(0.0, 1.0, rows),
        "shape_score": rng.uniform(0.0, 1.0, rows),
        "barcode_present": rng.integers(0, 2, rows),
        "label": rng.choice(["recycle", "compost", "landfill"], rows, p=[0.45, 0.25, 0.30]),
    })
    return df


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", type=Path, default=Path("material_dataset.csv"))
    args = parser.parse_args()

    df = load_dataset(args.data)
    X = df[["rgb_mean", "nir_mean", "shape_score", "barcode_present"]]
    y = df["label"]
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)

    model = RandomForestClassifier(n_estimators=180, max_depth=8, random_state=42)
    model.fit(X_train, y_train)
    preds = model.predict(X_test)
    print(classification_report(y_test, preds))
    print("feature_importances", dict(zip(X.columns, model.feature_importances_)))


if __name__ == "__main__":
    main()
