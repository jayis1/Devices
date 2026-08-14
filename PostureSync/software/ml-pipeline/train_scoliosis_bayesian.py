"""
ScoliosisScreen Bayesian Training Script
Early scoliosis detection using Bayesian change-point + Gaussian process

Input: 30-day multi-segment spinal curvature angles + EMG asymmetry
Output: Scoliosis risk score (0-100) + confidence interval
"""

import numpy as np
from scipy import stats
from scipy.optimize import minimize
import json
import os


class BayesianChangePoint:
    """Bayesian change-point detection for spinal curvature trends"""

    def __init__(self, threshold=10.0):
        self.threshold = threshold  # Cobb angle degrees
        self.prior_mean = 0.0
        self.prior_var = 25.0  # Prior variance

    def detect(self, curvature_series: np.ndarray) -> dict:
        """
        Detect change-point in curvature time series
        Returns probability of significant curvature change
        """
        n = len(curvature_series)
        if n < 7:
            return {"risk": 0, "confidence": 0, "change_point": None}

        # Compute cumulative mean and variance
        cumsum = np.cumsum(curvature_series)
        cummean = cumsum / np.arange(1, n + 1)

        # Bayesian change-point: find point where posterior is maximized
        log_likelihood = np.zeros(n)

        for t in range(2, n - 2):
            # Before change-point
            before = curvature_series[:t]
            after = curvature_series[t:]

            if len(before) > 1 and len(after) > 1:
                mean_before = np.mean(before)
                var_before = np.var(before) + 1e-6
                mean_after = np.mean(after)
                var_after = np.var(after) + 1e-6

                # Log-likelihood ratio
                ll_before = np.sum(stats.norm.logpdf(before, mean_before, np.sqrt(var_before)))
                ll_after = np.sum(stats.norm.logpdf(after, mean_after, np.sqrt(var_after)))
                log_likelihood[t] = ll_before + ll_after

        # Find maximum
        cp = np.argmax(log_likelihood)
        max_ll = log_likelihood[cp]

        # Compute risk score
        before_mean = np.mean(curvature_series[:cp]) if cp > 0 else 0
        after_mean = np.mean(curvature_series[cp:])
        change = abs(after_mean - before_mean)

        # Risk based on curvature magnitude and change rate
        risk = min(100, change * 10)
        confidence = min(1.0, max_ll / 100)

        return {
            "risk": int(risk),
            "confidence": float(confidence),
            "change_point": int(cp),
            "before_mean": float(before_mean),
            "after_mean": float(after_mean),
            "change_magnitude": float(change),
        }


class GaussianProcessScoliosis:
    """Gaussian process regression for curvature trajectory prediction"""

    def __init__(self, length_scale=10.0, noise=1.0):
        self.length_scale = length_scale
        self.noise = noise
        self.X_train = None
        self.y_train = None

    def _kernel(self, x1, x2):
        """RBF kernel"""
        sq_dist = (x1 - x2) ** 2
        return np.exp(-sq_dist / (2 * self.length_scale ** 2))

    def fit(self, X: np.ndarray, y: np.ndarray):
        """Fit GP to curvature data"""
        self.X_train = X
        self.y_train = y

    def predict(self, X_test: np.ndarray) -> tuple:
        """Predict curvature trajectory with uncertainty"""
        if self.X_train is None:
            return np.zeros(len(X_test)), np.ones(len(X_test)) * 10

        n_train = len(self.X_train)
        n_test = len(X_test)

        # Compute kernel matrices
        K = np.zeros((n_train, n_train))
        for i in range(n_train):
            for j in range(n_train):
                K[i, j] = self._kernel(self.X_train[i], self.X_train[j])
        K += self.noise * np.eye(n_train)

        K_s = np.zeros((n_train, n_test))
        for i in range(n_train):
            for j in range(n_test):
                K_s[i, j] = self._kernel(self.X_train[i], X_test[j])

        K_ss = np.zeros((n_test, n_test))
        for i in range(n_test):
            for j in range(n_test):
                K_ss[i, j] = self._kernel(X_test[i], X_test[j])

        # GP posterior
        K_inv = np.linalg.inv(K)
        mu = K_s.T @ K_inv @ self.y_train
        sigma = np.diag(K_ss - K_s.T @ K_inv @ K_s)

        return mu, np.sqrt(np.maximum(sigma, 0))


def generate_synthetic_data(n_samples=1000, days=30):
    """Generate synthetic curvature data for training"""
    np.random.seed(42)

    data = []
    labels = []

    for _ in range(n_samples):
        # Generate 30-day curvature series
        t = np.arange(days)

        # Random baseline curvature
        baseline = np.random.uniform(-5, 5)

        # Some have progressive curvature (scoliosis)
        is_scoliotic = np.random.random() < 0.15
        if is_scoliotic:
            progression = np.random.uniform(0.2, 0.8)
            curvature = baseline + progression * t + np.random.normal(0, 1, days)
            risk = min(100, progression * 30 + abs(baseline) * 5)
        else:
            curvature = baseline + np.random.normal(0, 1, days)
            risk = max(0, abs(baseline) * 3)

        # Add EMG asymmetry features
        emg_asymmetry = np.random.uniform(5, 30) if is_scoliotic else np.random.uniform(0, 15)

        data.append({
            "curvature": curvature.tolist(),
            "emg_asymmetry": emg_asymmetry,
            "days": days,
        })
        labels.append(min(100, max(0, risk + np.random.normal(0, 5))))

    return data, np.array(labels)


def train_model(save_dir: str = "models"):
    """Train ScoliosisScreen Bayesian model"""

    print("Generating synthetic scoliosis data...")
    data, labels = generate_synthetic_data(n_samples=2000)

    # Train change-point detector
    cp_detector = BayesianChangePoint(threshold=10.0)

    # Evaluate on synthetic data
    predictions = []
    confidences = []

    for sample in data:
        curvature = np.array(sample["curvature"])
        result = cp_detector.detect(curvature)
        predictions.append(result["risk"])
        confidences.append(result["confidence"])

    predictions = np.array(predictions)
    mse = np.mean((predictions - labels) ** 2)
    mae = np.mean(np.abs(predictions - labels))

    print(f"MSE: {mse:.2f}, MAE: {mae:.2f}")

    # Train GP for trajectory prediction
    gp = GaussianProcessScoliosis(length_scale=10.0, noise=1.0)

    # Use first sample as example
    example = data[0]
    X = np.arange(len(example["curvature"]))
    y = np.array(example["curvature"])
    gp.fit(X, y)

    # Predict future 30 days
    X_future = np.arange(30, 60)
    mu, sigma = gp.predict(X_future)

    print(f"GP forecast: mean={mu[-1]:.2f}, std={sigma[-1]:.2f}")

    # Save model parameters
    os.makedirs(save_dir, exist_ok=True)
    model_params = {
        "threshold": 10.0,
        "length_scale": 10.0,
        "noise": 1.0,
        "mse": float(mse),
        "mae": float(mae),
    }
    with open(os.path.join(save_dir, "scoliosis_bayesian.json"), "w") as f:
        json.dump(model_params, f, indent=2)

    print(f"Model saved to {save_dir}/scoliosis_bayesian.json")
    return cp_detector, gp


if __name__ == "__main__":
    train_model()