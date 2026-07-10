"""
AllergySync — SymptomPredict & AllergenSensitivity Models
==========================================================
Two models:
  1. SymptomPredict: XGBoost for 12-hour symptom severity forecast
     Features: exposure history, weather, medication, day-of-week, pollen class
  2. AllergenSensitivity: Bayesian logistic regression for personal
     allergen profile learning (online update)
  3. AnomalyDetector: Isolation Forest for pollen spike detection
"""

import numpy as np
import xgboost as xgb
from sklearn.ensemble import IsolationForest
from sklearn.linear_model import BayesianRidge
from scipy import stats
import json
import pickle


# ---- SymptomPredict (XGBoost) ----

SYMPTOM_FEATURES = [
    "pm2_5_avg_6h", "pm10_avg_6h", "pollen_class", "pollen_conf",
    "temp", "humidity", "wind_speed", "pressure",
    "day_of_week", "hour_of_day", "antihistamine_taken",
    "symptom_severity_yesterday", "exposure_outdoor_minutes",
]

def train_symptom_predict(X, y):
    """
    Train XGBoost model for symptom severity prediction.
    X: (n, 13) feature matrix
    y: (n,) symptom severity 0-5
    """
    model = xgb.XGBRegressor(
        n_estimators=200,
        max_depth=6,
        learning_rate=0.1,
        subsample=0.8,
        colsample_bytree=0.8,
        objective="reg:squarederror",
        random_state=42
    )
    model.fit(X, y)
    return model


def predict_symptoms(model, features):
    """Predict 12-hour symptom severity (0-5 scale)."""
    pred = model.predict(features.reshape(1, -1))
    return float(max(0, min(5, pred[0])))


# ---- AllergenSensitivity (Bayesian) ----

class AllergenSensitivityModel:
    """
    Bayesian logistic regression for personal allergen sensitivity.
    Each allergen has a sensitivity coefficient learned from
    exposure-symptom pairs.

    P(high_symptoms | exposure_to_allergen_X) = σ(α + β_X × exposure_X)

    Uses Bayesian updating with conjugate priors (Beta distribution).
    """

    def __init__(self, n_allergens=8):
        self.n_allergens = n_allergens
        allergen_names = [
            "birch", "grass", "ragweed", "oak", "pine", "mold",
            "dust_mites", "pet_dander"
        ]
        self.allergen_names = allergen_names[:n_allergens]

        # Beta distribution parameters for each allergen
        # α = symptom count when exposed, β = symptom count when not exposed
        # Prior: uniform (α=1, β=1)
        self.alpha = np.ones(n_allergens)  # symptoms when exposed
        self.beta = np.ones(n_allergens)   # no symptoms when exposed
        self.exposure_history = []

    def update(self, exposure_vector, symptom_severity):
        """
        Update sensitivity estimates with a new observation.
        exposure_vector: (n_allergens,) binary or continuous 0-1
        symptom_severity: float 0-5 (threshold at >2 = "symptoms")
        """
        has_symptoms = symptom_severity > 2.0

        for i in range(self.n_allergens):
            if exposure_vector[i] > 0.1:  # Exposed to this allergen
                if has_symptoms:
                    self.alpha[i] += 1
                else:
                    self.beta[i] += 1
            # If not exposed, no update (can't learn about that allergen)

        self.exposure_history.append({
            "exposure": exposure_vector.tolist(),
            "severity": symptom_severity,
        })

    def get_sensitivity(self, allergen_idx):
        """Get sensitivity probability for an allergen (0-1)."""
        return self.alpha[allergen_idx] / (self.alpha[allergen_idx] + self.beta[allergen_idx])

    def get_all_sensitivities(self):
        """Get sensitivity scores for all allergens."""
        return {
            name: float(self.get_sensitivity(i))
            for i, name in enumerate(self.allergen_names)
        }

    def get_personal_risk(self, current_exposure):
        """
        Compute personalized risk score given current exposure levels.
        current_exposure: (n_allergens,) exposure levels 0-1
        Returns risk score 0-1.
        """
        sensitivities = np.array([self.get_sensitivity(i)
                                  for i in range(self.n_allergens)])
        risk = np.dot(sensitivities, current_exposure) / self.n_allergens
        return float(risk)

    def save(self, path):
        with open(path, "wb") as f:
            pickle.dump({
                "alpha": self.alpha,
                "beta": self.beta,
                "names": self.allergen_names,
                "history": self.exposure_history,
            }, f)

    @classmethod
    def load(cls, path):
        with open(path, "rb") as f:
            data = pickle.load(f)
        model = cls(len(data["names"]))
        model.alpha = data["alpha"]
        model.beta = data["beta"]
        model.allergen_names = data["names"]
        model.exposure_history = data["history"]
        return model


# ---- AnomalyDetector (Isolation Forest) ----

def train_anomaly_detector(X):
    """
    Train Isolation Forest for pollen anomaly detection.
    X: (n, 5) — [pm2_5, pm10, co2, voc, pollen_count]
    Returns trained model.
    """
    model = IsolationForest(
        n_estimators=100,
        contamination=0.05,  # 5% expected anomalies
        random_state=42
    )
    model.fit(X)
    return model


def detect_anomalies(model, X):
    """Detect anomalies in sensor data.
    Returns: (anomaly_labels, anomaly_scores)
    anomaly_labels: -1 = anomaly, 1 = normal
    """
    labels = model.predict(X)
    scores = model.decision_function(X)
    return labels, scores


# ---- Training script ----

if __name__ == "__main__":
    rng = np.random.default_rng(42)

    # 1. Train SymptomPredict on synthetic data
    print("Training SymptomPredict (XGBoost)...")
    n_samples = 5000
    X_symptom = rng.uniform(0, 100, (n_samples, len(SYMPTOM_FEATURES)))
    # Synthetic labels: severity increases with PM and pollen
    y_symptom = (
        0.02 * X_symptom[:, 0] +  # PM2.5
        0.01 * X_symptom[:, 1] +  # PM10
        0.5 * X_symptom[:, 3] +   # pollen confidence
        rng.normal(0, 0.5, n_samples)
    )
    y_symptom = np.clip(y_symptom, 0, 5)
    # Reduce effect if antihistamine taken
    y_symptom *= (1 - 0.5 * X_symptom[:, 10])

    symptom_model = train_symptom_predict(X_symptom, y_symptom)
    with open("symptom_predict_xgb.pkl", "wb") as f:
        pickle.dump(symptom_model, f)
    print("SymptomPredict saved. Feature importances:")
    for name, imp in zip(SYMPTOM_FEATURES, symptom_model.feature_importances_):
        print(f"  {name}: {imp:.4f}")

    # 2. Test AllergenSensitivity
    print("\nTraining AllergenSensitivity (Bayesian)...")
    sens_model = AllergenSensitivityModel(n_allergens=8)
    for _ in range(1000):
        exposure = rng.random(8) > 0.7  # ~30% chance of exposure per allergen
        # Simulate: birch (index 0) causes high symptoms
        severity = 0
        if exposure[0]:  # birch
            severity += rng.normal(3.5, 0.5)
        if exposure[1]:  # grass
            severity += rng.normal(2.0, 0.5)
        if exposure[5]:  # mold
            severity += rng.normal(1.5, 0.5)
        severity = max(0, min(5, severity + rng.normal(0, 0.3)))
        sens_model.update(exposure.astype(float), severity)

    print("Sensitivity scores:")
    for name, score in sens_model.get_all_sensitivities().items():
        print(f"  {name}: {score:.3f}")
    sens_model.save("allergen_sensitivity.pkl")

    # 3. Train AnomalyDetector
    print("\nTraining AnomalyDetector (Isolation Forest)...")
    X_anomaly = rng.uniform(0, 100, (2000, 5))
    # Add some anomalies
    X_anomaly[:100, 0] = rng.uniform(200, 500, 100)  # PM2.5 spikes
    X_anomaly[:100, 1] = rng.uniform(300, 600, 100)  # PM10 spikes
    anomaly_model = train_anomaly_detector(X_anomaly)
    with open("anomaly_detector.pkl", "wb") as f:
        pickle.dump(anomaly_model, f)
    labels, scores = detect_anomalies(anomaly_model, X_anomaly)
    print(f"Anomalies detected: {np.sum(labels == -1)} / {len(labels)}")