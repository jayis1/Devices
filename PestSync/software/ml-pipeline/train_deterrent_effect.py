"""
PestSync ML Pipeline — Deterrent Effectiveness Model
software/ml-pipeline/train_deterrent_effect.py

Logistic regression model to assess deterrent effectiveness.
Compares pre/post pest activity to measure impact.
"""
import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, classification_report
import joblib
import os

NUM_FEATURES = 12
MODEL_OUTPUT = "models/deterrent_effect_logreg.joblib"


def generate_data(n_samples=10000):
    """Generate synthetic deterrent effectiveness data."""
    np.random.seed(42)
    X = np.zeros((n_samples, NUM_FEATURES))
    y = np.zeros(n_samples, dtype=int)

    for i in range(n_samples):
        # Pre-activity count (1 week before deterrent)
        pre_count = np.random.poisson(20)
        X[i, 0] = pre_count

        # Post-activity count (1 week after deterrent)
        post_count = np.random.poisson(max(pre_count * np.random.uniform(0.1, 0.8), 0.5))
        X[i, 1] = post_count

        # Reduction percentage
        X[i, 2] = (pre_count - post_count) / max(pre_count, 1) * 100

        # Deterrent type: 0=ultrasonic, 1=strobe, 2=diffuser, 3=combo
        deter_type = np.random.randint(0, 4)
        X[i, 3] = deter_type

        # Ultrasonic frequency band
        X[i, 4] = np.random.randint(0, 3)  # rodent, insect, both

        # Duration per day (hours)
        X[i, 5] = np.random.uniform(2, 24)

        # Days active
        X[i, 6] = np.random.randint(1, 30)

        # Pest type (0=rodent, 1=cockroach, 2=ant, 3=fly)
        X[i, 7] = np.random.randint(0, 4)

        # Room type (0=kitchen, 1=garage, 2=attic, 3=basement)
        X[i, 8] = np.random.randint(0, 4)

        # Room size (sqm)
        X[i, 9] = np.random.uniform(5, 50)

        # Number of deterrents in room
        X[i, 10] = np.random.randint(1, 4)

        # Ambient noise level (dB)
        X[i, 11] = np.random.uniform(30, 70)

        # Effectiveness label: 1 if reduction > 40%
        y[i] = 1 if X[i, 2] > 40 else 0

    return X, y


def train():
    print("Generating deterrent effectiveness data...")
    X, y = generate_data(10000)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    model = LogisticRegression(
        max_iter=1000,
        C=1.0,
        class_weight="balanced",
        random_state=42,
    )

    print("Training deterrent effectiveness logistic regression...")
    model.fit(X_train, y_train)

    y_pred = model.predict(X_test)
    acc = accuracy_score(y_test, y_pred)

    print(f"\n{'='*60}")
    print(f"Deterrent Effectiveness Model — Test Results:")
    print(f"  Accuracy: {acc:.4f}")
    print(f"\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=["ineffective", "effective"]))
    print(f"{'='*60}")

    # Feature importance
    feature_names = [
        "pre_count", "post_count", "reduction_pct", "deter_type", "freq_band",
        "hours_per_day", "days_active", "pest_type", "room_type", "room_size",
        "num_deterrents", "noise_level"
    ]
    print("\nFeature coefficients:")
    for name, coef in sorted(zip(feature_names, model.coef_[0]), key=lambda x: abs(x[1]), reverse=True):
        print(f"  {name:20s}: {coef:+.4f}")

    os.makedirs("models", exist_ok=True)
    joblib.dump(model, MODEL_OUTPUT)
    print(f"\n✅ Model saved to {MODEL_OUTPUT}")


if __name__ == "__main__":
    train()