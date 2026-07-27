"""
GrillSync — GrillAnomaly Training Script
Isolation Forest for grill behavior anomaly detection.

Unsupervised model trained on normal grill sessions to detect
unusual patterns (abnormal temperature curves, unexpected gas levels,
unusual acoustic patterns).
"""
import argparse
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.preprocessing import StandardScaler
import pickle


def extract_features(session):
    """Extract summary features from a grill session."""
    thermal = session.get("thermal_history", [])
    if not thermal:
        return None
    temps = [t.get("max_temp_c", 0) for t in thermal]
    gas = [t.get("gas_ppm", 0) for t in thermal]
    acoustic = [t.get("acoustic_energy", 0) for t in thermal]

    return {
        "temp_mean": np.mean(temps),
        "temp_max": np.max(temps),
        "temp_std": np.std(temps),
        "temp_rate_max": np.max(np.abs(np.diff(temps))) if len(temps) > 1 else 0,
        "gas_mean": np.mean(gas),
        "gas_max": np.max(gas),
        "gas_std": np.std(gas),
        "acoustic_mean": np.mean(acoustic),
        "acoustic_max": np.max(acoustic),
        "duration_s": len(thermal),
    }


def train_model(data_path, output_path):
    """Train Isolation Forest on normal grill sessions."""
    print("Generating synthetic normal grill session data...")
    sessions = generate_synthetic_data(n_sessions=2000)

    # Extract features from all sessions
    feature_list = []
    for session in sessions:
        features = extract_features(session)
        if features:
            feature_list.append(list(features.values()))

    feature_names = list(extract_features(sessions[0]).keys())
    X = np.array(feature_list)

    # Normalize
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    # Train Isolation Forest
    model = IsolationForest(
        n_estimators=200,
        contamination=0.05,  # 5% expected anomaly rate
        random_state=42,
    )

    print(f"Training GrillAnomaly on {len(X)} sessions")
    model.fit(X_scaled)

    # Evaluate (on training data — should be mostly normal)
    predictions = model.predict(X_scaled)
    anomaly_rate = np.mean(predictions == -1)
    print(f"Anomaly rate on training data: {anomaly_rate:.2%}")

    # Save model + scaler
    with open(output_path, "wb") as f:
        pickle.dump({"model": model, "scaler": scaler, "features": feature_names}, f)
    print(f"Model saved to {output_path}")


def generate_synthetic_data(n_sessions=2000):
    """Generate synthetic normal grill sessions."""
    sessions = []
    for i in range(n_sessions):
        duration = np.random.randint(300, 3600)
        thermal = []
        base_temp = np.random.uniform(180, 280)
        for t in range(duration):
            temp = base_temp + np.random.normal(0, 15)
            thermal.append({
                "max_temp_c": float(temp),
                "gas_ppm": float(50 + np.random.normal(0, 30)),
                "acoustic_energy": float(np.random.exponential(30)),
            })
        sessions.append({"thermal_history": thermal})
    return sessions


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Train GrillAnomaly")
    parser.add_argument("--data", default="/data/grill-sessions")
    parser.add_argument("--output", default="models/grill_anomaly.pkl")
    args = parser.parse_args()
    train_model(args.data, args.output)