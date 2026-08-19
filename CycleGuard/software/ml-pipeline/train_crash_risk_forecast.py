"""
CrashRiskForecast Training Script
48-hour crash risk forecasting based on weather + historical crash correlation.

Input: Weather forecast (wind, rain, temp, visibility) + historical crash data
       + route features + time of day
Output: 48-hour crash risk score (0-100) per 3-hour window
Architecture: XGBoost (Gradient Boosted Trees) + temporal features
"""

import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import roc_auc_score
import joblib


def generate_training_data(n=10000):
    """Generate synthetic training data."""
    np.random.seed(42)
    # Features: [hour, day_of_week, temp_c, wind_kmh, rain_mm,
    #            visibility_km, humidity, is_rush_hour, is_night,
    #            bike_traffic_count, road_wet, route_difficulty]
    X = np.zeros((n, 12), dtype=np.float32)

    X[:, 0] = np.random.randint(0, 24, n)          # hour
    X[:, 1] = np.random.randint(0, 7, n)           # day of week
    X[:, 2] = np.random.uniform(0, 40, n)          # temp
    X[:, 3] = np.random.uniform(0, 40, n)          # wind
    X[:, 4] = np.random.exponential(2, n)          # rain (mm)
    X[:, 5] = np.random.uniform(1, 20, n)          # visibility
    X[:, 6] = np.random.uniform(20, 100, n)        # humidity
    X[:, 7] = ((X[:, 0] >= 7) & (X[:, 0] <= 9) |
               (X[:, 0] >= 17) & (X[:, 0] <= 19)).astype(float)  # rush hour
    X[:, 8] = ((X[:, 0] >= 22) | (X[:, 0] <= 5)).astype(float)   # night
    X[:, 9] = np.random.randint(0, 100, n)         # bike traffic
    X[:, 10] = (X[:, 4] > 0.5).astype(float)       # road wet
    X[:, 11] = np.random.uniform(0, 1, n)          # route difficulty

    # Target: crash risk score (0-100)
    # Higher risk: rush hour, night, rain, wind, low visibility
    y = (
        X[:, 7] * 25 +      # rush hour
        X[:, 8] * 15 +      # night
        X[:, 4] * 8 +       # rain
        X[:, 3] * 0.5 +     # wind
        (20 - X[:, 5]) * 2 + # low visibility
        X[:, 10] * 10 +     # wet road
        X[:, 11] * 15 +     # route difficulty
        np.random.randn(n) * 5
    )
    y = np.clip(y, 0, 100).astype(np.float32)
    # Binary classification: high risk if score > 60
    y_binary = (y > 60).astype(int)
    return X, y, y_binary


def train():
    X, y_reg, y_binary = generate_training_data()
    X_train, X_test, y_train, y_test = train_test_split(
        X, y_binary, test_size=0.2, random_state=42)

    # XGBoost classifier for high-risk prediction
    model = xgb.XGBClassifier(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        eval_metric='auc',
    )
    model.fit(X_train, y_train, eval_set=[(X_test, y_test)], verbose=False)

    # Evaluate
    y_pred_proba = model.predict_proba(X_test)[:, 1]
    auc = roc_auc_score(y_test, y_pred_proba)
    print(f"CrashRiskForecast XGBoost trained. Test AUC: {auc:.4f}")

    # Also train a regressor for continuous risk score
    X_train_r, X_test_r, y_train_r, y_test_r = train_test_split(
        X, y_reg, test_size=0.2, random_state=42)
    regressor = xgb.XGBRegressor(
        n_estimators=200, max_depth=6, learning_rate=0.1,
    )
    regressor.fit(X_train_r, y_train_r, eval_set=[(X_test_r, y_test_r)],
                  verbose=False)

    # Save models
    joblib.dump(model, "crash_risk_classifier.json")
    joblib.dump(regressor, "crash_risk_regressor.json")
    print("Exported: crash_risk_classifier.json, crash_risk_regressor.json")
    print("Target: cloud inference (FastAPI backend)")


if __name__ == "__main__":
    train()