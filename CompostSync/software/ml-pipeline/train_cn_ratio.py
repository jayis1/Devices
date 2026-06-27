"""
C:N Ratio Estimator Training
Trains an XGBoost regressor to estimate compost C:N ratio from sensor features.

Features (18):
  - Current: temp[3], moisture[3], co2, methane, mass, battery
  - 24h: temp_slope, co2_slope, mass_slope
  - 7-day: mass_loss_rate, avg_co2, avg_temp, max_temp
  - Context: days_since_start, days_since_turn, bin_volume
"""
import numpy as np
import pandas as pd
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error, r2_score
import joblib
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def extract_features(df_row):
    """Extract 18 features from a data row."""
    features = np.array([
        df_row["temp1"], df_row["temp2"], df_row["temp3"],
        df_row["moist1"], df_row["moist2"], df_row["moist3"],
        df_row["co2"], df_row["methane"], df_row["mass_g"],
        df_row["battery_pct"],
        df_row["temp_slope_24h"], df_row["co2_slope_24h"], df_row["mass_slope_24h"],
        df_row["mass_loss_7d"], df_row["avg_co2_7d"], df_row["max_temp_7d"],
        df_row["days_since_start"], df_row["days_since_turn"],
    ])
    return features


def train():
    """Train C:N ratio XGBoost model."""
    logger.info("Loading training data...")
    df = pd.read_csv("data/cn_ratio_training.csv")
    logger.info(f"Loaded {len(df)} samples")

    X = np.array([extract_features(row) for _, row in df.iterrows()])
    y = df["cn_ratio"].values

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = xgb.XGBRegressor(
        n_estimators=300,
        max_depth=6,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        objective="reg:squarederror",
    )
    model.fit(X_train, y_train, eval_set=[(X_test, y_test)], verbose=False)

    y_pred = model.predict(X_test)
    mae = mean_absolute_error(y_test, y_pred)
    r2 = r2_score(y_test, y_pred)
    logger.info(f"MAE: {mae:.2f} | R²: {r2:.4f}")

    # Feature importance
    feat_names = ["t1","t2","t3","m1","m2","m3","co2","ch4","mass","batt",
                  "t_slope","co2_slope","m_slope","mass_loss7d","avg_co2","max_temp",
                  "days_start","days_turn"]
    imp = sorted(zip(feat_names, model.feature_importances_), key=lambda x: -x[1])
    logger.info("Feature importance:")
    for name, score in imp:
        logger.info(f"  {name}: {score:.4f}")

    joblib.dump(model, "models/cn_ratio_xgb.joblib")
    logger.info("Saved model: models/cn_ratio_xgb.joblib")


if __name__ == "__main__":
    import os
    os.makedirs("models", exist_ok=True)
    train()