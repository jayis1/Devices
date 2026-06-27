"""
Pest Risk Predictor Training
Trains a logistic regression model to predict fruit fly and rodent pest risk.

Features (8):
  - ambient_temp, ambient_humidity, bin_temp, bin_moisture,
  - days_since_turn, days_since_start, has_meat_dairy, season
"""
import numpy as np
import pandas as pd
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, classification_report
from sklearn.preprocessing import StandardScaler
import joblib
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


def train():
    logger.info("Loading pest risk training data...")
    df = pd.read_csv("data/pest_risk_training.csv")
    logger.info(f"Loaded {len(df)} samples")

    features = ["ambient_temp", "ambient_humidity", "bin_temp", "bin_moisture",
                "days_since_turn", "days_since_start", "has_meat_dairy", "season"]
    X = df[features].values
    y = df["pest_detected"].values  # 0 = none, 1 = fruit flies, 2 = rodents

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)

    model = LogisticRegression(max_iter=1000, multi_class="multinomial")
    model.fit(X_train_scaled, y_train)

    y_pred = model.predict(X_test_scaled)
    acc = accuracy_score(y_test, y_pred)
    logger.info(f"Accuracy: {acc:.4f}")
    logger.info(classification_report(y_test, y_pred, target_names=["none", "fruit_flies", "rodents"]))

    joblib.dump({"model": model, "scaler": scaler}, "models/pest_risk.joblib")
    logger.info("Saved: models/pest_risk.joblib")


if __name__ == "__main__":
    import os
    os.makedirs("models", exist_ok=True)
    train()