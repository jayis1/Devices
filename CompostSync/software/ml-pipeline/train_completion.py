"""
Completion Forecaster Training
Trains a gradient boosting regressor to predict days-to-ready for compost.

Input:  maturity_score, current_temp, co2, cn_ratio, days_elapsed, phase, bin_volume
Output: days_remaining until compost is cured (maturity >= 95)
"""
import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error
import joblib
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def train():
    logger.info("Loading completion training data...")
    df = pd.read_csv("data/completion_training.csv")
    logger.info(f"Loaded {len(df)} samples")

    features = ["maturity_score", "temp", "co2", "cn_ratio",
                "days_elapsed", "phase_id", "bin_volume"]
    X = df[features].values
    y = df["days_to_ready"].values

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = GradientBoostingRegressor(
        n_estimators=200,
        max_depth=4,
        learning_rate=0.1,
        subsample=0.8,
    )
    model.fit(X_train, y_train)

    y_pred = model.predict(X_test)
    mae = mean_absolute_error(y_test, y_pred)
    logger.info(f"MAE: {mae:.2f} days | R²: {model.score(X_test, y_test):.4f}")

    joblib.dump(model, "models/completion_forecaster.joblib")
    logger.info("Saved: models/completion_forecaster.joblib")


if __name__ == "__main__":
    import os
    os.makedirs("models", exist_ok=True)
    train()