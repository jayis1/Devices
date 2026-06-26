"""
Model 6: Caries-risk forecaster
  Input:  per-tooth weekly features (plaque trend, pH, nitrite, brushing coverage,
          lesion history, age, orthodontic) — 10 features
  Output: 90-day caries risk 0-100 per tooth
  Arch:   LightGBM (gradient-boosted trees) on per-tooth weekly features
  Export: ONNX for cloud inference service
"""
import numpy as np
import pandas as pd
import lightgbm as lgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, roc_auc_score
import onnxmltools as onnxml

FEATS = ["plaque_pct","plaque_slope","ph_mean","nitrite_mean","brush_coverage",
         "lesion_count","lesion_severity","age","orthodontic","weeks_since_cleaning"]
TARGET = "caries_90d"   # binary: new caries within 90 days

def load_data():
    try:
        df = pd.read_csv("data/caries_risk/features.csv")
    except FileNotFoundError:
        print("[warn] data/caries_risk/features.csv not found — synthetic placeholder")
        rng = np.random.default_rng(42)
        n = 5000
        df = pd.DataFrame(rng.normal(0, 1, (n, len(FEATS))), columns=FEATS)
        df["ph_mean"] = rng.normal(6.8, 0.3, n)
        df["brush_coverage"] = rng.uniform(0.4, 1.0, n)
        df["age"] = rng.integers(5, 80, n)
        df["orthodontic"] = rng.integers(0, 2, n)
        df[TARGET] = ((df["plaque_pct"] > 0.5) & (df["ph_mean"] < 6.5)).astype(int)
    return df

def main():
    df = load_data()
    X = df[FEATS].values
    y = df[TARGET].values
    X_tr, X_te, y_tr, y_te = train_test_split(X, y, test_size=0.2, stratify=y, random_state=42)
    train = lgb.Dataset(X_tr, label=y_tr, feature_name=FEATS)
    val = lgb.Dataset(X_te, label=y_te, feature_name=FEATS)
    params = dict(objective="binary", metric="auc", num_leaves=31, learning_rate=0.05,
                  feature_fraction=0.9, bagging_fraction=0.8, bagging_freq=5, verbose=-1)
    model = lgb.train(params, train, num_boost_round=300, valid_sets=[val],
                      callbacks=[lgb.early_stopping(20)])
    pred = model.predict(X_te)
    auc = roc_auc_score(y_te, pred)
    print(f"Test AUC: {auc:.3f}")
    # Export to ONNX
    onnx_model = onnxml.convert_lightgbm(model, name="CariesRisk", initial_types=[("input", onnxml.common.data_types.FloatTensorType([None, len(FEATS)]))])
    onnxml.utils.save_model(onnx_model, "artifacts/caries_risk_lightgbm.onnx")
    print("✓ Exported artifacts/caries_risk_lightgbm.onnx")

if __name__ == "__main__":
    main()