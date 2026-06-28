"""
PestSync ML Pipeline — Infestation Risk Forecaster
software/ml-pipeline/train_infestation_risk.py

Trains XGBoost regressor to predict 30-day infestation risk per pest type
from 14-day activity trends, weather, season, and geographic location.
"""
import numpy as np
import xgboost as xgb
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score
import joblib
import os

# Features (18):
# - 14-day total detections
# - 14-day detection trend (slope)
# - 7-day total detections
# - 3-day total detections
# - Last 24h detections
# - Avg confidence of detections
# - Number of distinct species detected
# - Ambient temperature (°C)
# - Humidity (%)
# - Season (0=win, 1=spr, 2=sum, 3=fall)
# - Latitude
# - Urban density (0=rural, 1=suburban, 2=urban)
# - Household cleanliness score (1-10)
# - Previous treatment (binary)
# - Days since first detection
# - Days since last trap check
# - Number of active traps
# - Number of active deterrents

NUM_FEATURES = 18
MODEL_OUTPUT = "models/infestation_risk_xgb.joblib"


def generate_training_data(n_samples=50000):
    """Generate synthetic training data for infestation risk model."""
    np.random.seed(42)

    features = np.zeros((n_samples, NUM_FEATURES))
    targets = np.zeros(n_samples)

    for i in range(n_samples):
        # 14-day total detections (0-200)
        det_14d = np.random.poisson(20)
        features[i, 0] = det_14d

        # Detection trend slope (positive = increasing)
        features[i, 1] = np.random.uniform(-2, 3)

        # 7-day and 3-day
        features[i, 2] = det_14d * np.random.uniform(0.3, 0.7)
        features[i, 3] = det_14d * np.random.uniform(0.1, 0.3)

        # Last 24h
        features[i, 4] = np.random.poisson(max(det_14d / 14, 1))

        # Avg confidence
        features[i, 5] = np.random.uniform(0.4, 0.9)

        # Distinct species
        features[i, 6] = np.random.randint(1, 5)

        # Ambient temp
        features[i, 7] = np.random.uniform(-5, 35)

        # Humidity
        features[i, 8] = np.random.uniform(20, 90)

        # Season
        features[i, 9] = np.random.randint(0, 4)

        # Latitude
        features[i, 10] = np.random.uniform(25, 60)

        # Urban density
        features[i, 11] = np.random.randint(0, 3)

        # Cleanliness
        features[i, 12] = np.random.randint(1, 11)

        # Previous treatment
        features[i, 13] = np.random.randint(0, 2)

        # Days since first detection
        features[i, 14] = np.random.randint(1, 90)

        # Days since last trap check
        features[i, 15] = np.random.randint(0, 30)

        # Active traps
        features[i, 16] = np.random.randint(0, 6)

        # Active deterrents
        features[i, 17] = np.random.randint(0, 4)

        # Target: 30-day infestation risk (0-1)
        # Higher risk with: more detections, increasing trend, warm season,
        # urban area, low cleanliness, no treatment
        risk = 0.1
        risk += min(det_14d / 200, 0.3)
        risk += max(features[i, 1] / 10, 0) * 0.2  # increasing trend
        risk += (features[i, 7] > 20) * 0.1  # warm temp
        risk += (features[i, 9] in [1, 2]) * 0.1  # spring/summer
        risk += (features[i, 11] == 2) * 0.1  # urban
        risk -= (features[i, 12] / 10) * 0.15  # cleanliness reduces
        risk -= features[i, 13] * 0.1  # treatment reduces
        risk -= min(features[i, 16] / 5, 0.1)  # traps reduce
        risk -= min(features[i, 17] / 4, 0.1)  # deterrents reduce

        risk = max(0, min(1, risk + np.random.normal(0, 0.05)))
        targets[i] = risk

    return features, targets


def train():
    print("Generating training data...")
    X, y = generate_training_data(50000)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    print(f"Training set: {len(X_train)} samples")
    print(f"Test set: {len(X_test)} samples")

    # XGBoost regressor
    model = xgb.XGBRegressor(
        n_estimators=300,
        max_depth=6,
        learning_rate=0.05,
        subsample=0.8,
        colsample_bytree=0.8,
        reg_alpha=0.1,
        reg_lambda=1.0,
        random_state=42,
    )

    print("Training XGBoost infestation risk forecaster...")
    model.fit(X_train, y_train)

    # Evaluate
    y_pred = model.predict(X_test)
    rmse = np.sqrt(mean_squared_error(y_test, y_pred))
    r2 = r2_score(y_test, y_pred)

    print(f"\n{'='*60}")
    print(f"Infestation Risk Forecaster — Test Results:")
    print(f"  RMSE: {rmse:.4f}")
    print(f"  R²:   {r2:.4f}")
    print(f"{'='*60}")

    # Feature importance
    importance = model.feature_importances_
    feature_names = [
        "det_14d", "trend_slope", "det_7d", "det_3d", "det_24h",
        "avg_conf", "species_count", "temp_c", "humidity", "season",
        "latitude", "urban", "cleanliness", "treated", "days_since_start",
        "days_since_check", "num_traps", "num_deterrents"
    ]
    print("\nFeature Importance (top 5):")
    sorted_idx = np.argsort(importance)[::-1]
    for i in sorted_idx[:5]:
        print(f"  {feature_names[i]:25s}: {importance[i]:.4f}")

    # Save model
    os.makedirs("models", exist_ok=True)
    joblib.dump(model, MODEL_OUTPUT)
    print(f"\n✅ Model saved to {MODEL_OUTPUT}")


if __name__ == "__main__":
    train()