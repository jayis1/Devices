"""
MigraineSync — Barometric Change-Point Detector (Bayesian)
===========================================================
Implements a Bayesian online change-point detector for identifying
rapid barometric pressure changes that trigger migraines.

Uses the BOCPD algorithm (Adams & MacKay, 2007) to detect
change-points in pressure time-series in real-time.

Input:  Pressure time-series (1 Hz, 24h window)
Output: P(change-point at time t), estimated magnitude

License: MIT
"""

import os
import numpy as np
import pandas as pd
from scipy.stats import norm
import joblib

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
MODEL_DIR = os.path.join(os.path.dirname(__file__), "..", "models")
os.makedirs(MODEL_DIR, exist_ok=True)


class BayesianChangePointDetector:
    """Bayesian Online Change-Point Detection (Adams & MacKay 2007).

    Maintains a run-length distribution R(r_t) that represents
    the probability that the time since last change-point is r.
    When a change-point is likely, the mass shifts to r=0.
    """

    def __init__(self, hazard_rate=0.01, max_run_length=500,
                 mu0=1013.0, sigma0=5.0, obs_noise=0.5):
        self.hazard_rate = hazard_rate   # prior probability of change-point per step
        self.max_run_length = max_run_length
        self.mu0 = mu0                   # prior mean
        self.sigma0 = sigma0             # prior std
        self.obs_noise = obs_noise       # observation noise std

        # Run-length probabilities (normalized)
        self.R = np.zeros(max_run_length + 1)
        self.R[0] = 1.0  # start with run-length 0

        # Gaussian posterior parameters for each run-length
        self.mu = np.full(max_run_length + 1, mu0)
        self.sigma = np.full(max_run_length + 1, sigma0)

    def update(self, x):
        """Process one observation and update run-length distribution."""
        # 1. Compute predictive probabilities for each run-length
        predictive = np.zeros(self.max_run_length + 1)
        for r in range(self.max_run_length + 1):
            if self.R[r] > 0:
                predictive[r] = norm.pdf(x, self.mu[r], np.sqrt(self.sigma[r]**2 + self.obs_noise**2))

        # 2. Growth probabilities (run-length increases by 1)
        growth = self.R * predictive * (1 - self.hazard_rate)

        # 3. Change-point probability (run-length resets to 0)
        changepoint = np.sum(self.R * predictive * self.hazard_rate)

        # 4. New run-length distribution
        new_R = np.zeros(self.max_run_length + 1)
        new_R[0] = changepoint
        new_R[1:self.max_run_length + 1] = growth[:self.max_run_length]

        # 5. Normalize
        total = np.sum(new_R)
        if total > 0:
            new_R /= total
        self.R = new_R

        # 6. Update posterior parameters for each run-length
        new_mu = np.full(self.max_run_length + 1, self.mu0)
        new_sigma = np.full(self.max_run_length + 1, self.sigma0)

        for r in range(1, self.max_run_length + 1):
            if self.R[r] > 0:
                # Bayesian update for Gaussian
                prior_var = self.sigma[r-1]**2 + self.obs_noise**2
                posterior_var = 1.0 / (1.0 / prior_var + 1.0 / self.obs_noise**2)
                posterior_mean = posterior_var * (self.mu[r-1] / prior_var + x / self.obs_noise**2)
                new_mu[r] = posterior_mean
                new_sigma[r] = np.sqrt(posterior_var)

        self.mu = new_mu
        self.sigma = new_sigma

        return self.R[0]  # probability of change-point at this step

    def detect(self, series):
        """Run detection on a full time series."""
        cp_probs = np.zeros(len(series))
        for i, x in enumerate(series):
            cp_probs[i] = self.update(x)
        return cp_probs


def train_and_evaluate():
    """Train (calibrate) the change-point detector on synthetic data."""
    print("Loading data...")
    data_path = os.path.join(DATA_DIR, "synthetic_migraine_data.csv")
    df = pd.read_csv(data_path)

    # Get pressure data for first 10 patients
    results = []
    for pid in df["patient_id"].unique()[:10]:
        patient_df = df[df["patient_id"] == pid]
        pressure = patient_df["pressure_hpa"].values[:1000]  # first ~3.5 days

        detector = BayesianChangePointDetector(
            hazard_rate=0.005,  # expect change-points ~every 200 samples
            mu0=np.mean(pressure),
            sigma0=np.std(pressure),
            obs_noise=0.5,
        )

        cp_probs = detector.detect(pressure)

        # Find detected change-points (threshold = 0.3)
        threshold = 0.3
        detected_cps = np.where(cp_probs > threshold)[0]

        # Compare with actual pressure drops
        actual_drops = np.where(np.abs(patient_df["pressure_delta_3h"].values[:1000]) > 3.0)[0]

        results.append({
            "patient_id": pid,
            "n_detected": len(detected_cps),
            "n_actual": len(actual_drops),
            "max_cp_prob": np.max(cp_probs),
        })

        print(f"Patient {pid}: detected {len(detected_cps)} change-points, "
              f"{len(actual_drops)} actual pressure events")

    # Save detector
    detector = BayesianChangePointDetector()
    joblib.dump(detector, os.path.join(MODEL_DIR, "barometric_bocpd.pkl"))
    print(f"\nDetector saved: {os.path.join(MODEL_DIR, 'barometric_bocpd.pkl')}")
    print("Done!")


if __name__ == "__main__":
    train_and_evaluate()