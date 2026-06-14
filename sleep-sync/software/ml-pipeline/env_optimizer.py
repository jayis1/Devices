"""
env_optimizer.py — Bayesian environment optimization for SleepSync

Learns what environmental conditions (temperature, humidity) give each user
the best sleep quality. Uses Gaussian Process regression to model the
relationship between environment and sleep quality, and Bayesian
optimization to suggest better setpoints.

Input:  30-day history of sleep stages + environment conditions
Output: Personalized optimal setpoints for temperature, humidity per sleep stage

Runs nightly on the cloud after new sleep data is available.
"""

import argparse
import numpy as np
from typing import Dict, Tuple, List, Optional

try:
    from sklearn.gaussian_process import GaussianProcessRegressor
    from sklearn.gaussian_process.kernels import Matern, ConstantKernel, WhiteKernel
    from scipy.stats import norm
    from scipy.optimize import minimize
    HAS_SKLEARN = True
except ImportError:
    HAS_SKLEARN = False


# Population-level priors (from sleep research literature)
POPULATION_OPTIMAL = {
    "temperature": {"mean": 19.0, "std": 1.5, "range": (15.0, 24.0)},
    "humidity": {"mean": 45.0, "std": 8.0, "range": (25.0, 70.0)},
}

# Minimum nights of data before personalization kicks in
MIN_NIGHTS_FOR_PERSONALIZATION = 7


class SleepEnvironmentOptimizer:
    """Bayesian optimizer for sleep environment parameters"""

    def __init__(self, param_bounds: Dict[str, Tuple[float, float]] = None):
        self.param_bounds = param_bounds or {
            "temperature": (15.0, 24.0),
            "humidity": (25.0, 70.0),
        }
        self.gp = None
        self.X_observed = []  # Environment observations: [temp, humidity]
        self.y_observed = []  # Sleep quality scores
        self.nights = 0

    def add_night_observation(self, temperature: float, humidity: float,
                               sleep_score: float, deep_pct: float,
                               rem_pct: float, waso_count: int):
        """
        Add one night's observation.

        Sleep quality composite:
        - 40% overall sleep score
        - 25% deep sleep percentage (target: 15-20%)
        - 25% REM sleep percentage (target: 20-25%)
        - 10% low WASO penalty
        """
        deep_score = min(deep_pct / 17.5, 1.0) if deep_pct <= 20 else max(0, 1.0 - (deep_pct - 20) / 20)
        rem_score = min(rem_pct / 22.5, 1.0) if rem_pct <= 25 else max(0, 1.0 - (rem_pct - 25) / 20)
        waso_score = max(0, 1.0 - waso_count / 10)

        composite = (
            0.40 * (sleep_score / 100.0) +
            0.25 * deep_score +
            0.25 * rem_score +
            0.10 * waso_score
        )

        self.X_observed.append([temperature, humidity])
        self.y_observed.append(composite)
        self.nights += 1

    def fit_model(self):
        """Fit Gaussian Process to observed data"""
        if not HAS_SKLEARN:
            print("scikit-learn not available — using population priors only")
            return

        X = np.array(self.X_observed)
        y = np.array(self.y_observed)

        # Matern kernel (smooth but allows non-differentiable functions)
        kernel = ConstantKernel(1.0, constant_range_bounds="fixed") * \
                 Matern(length_scale=[2.0, 10.0], nu=2.5) + \
                 WhiteKernel(noise_level=0.01)

        self.gp = GaussianProcessRegressor(
            kernel=kernel,
            n_restarts_optimizer=5,
            normalize_y=True,
        )
        self.gp.fit(X, y)
        print(f"GP fitted on {len(X)} observations")
        print(f"Log-marginal-likelihood: {self.gp.log_marginal_likelihood():.2f}")

    def expected_improvement(self, X: np.ndarray) -> float:
        """Compute Expected Improvement acquisition function"""
        if self.gp is None:
            return 0.0

        mu, sigma = self.gp.predict(X, return_std=True)
        mu = mu.flatten()
        sigma = sigma.flatten()

        # Best observed value
        y_best = np.max(self.y_observed)

        # EI calculation
        xi = 0.01  # exploration-exploitation trade-off
        improvement = mu - y_best - xi
        with np.errstate(divide='ignore', invalid='ignore'):
            Z = improvement / sigma
            ei = improvement * norm.cdf(Z) + sigma * norm.pdf(Z)
            ei[sigma == 0] = 0

        return ei

    def suggest_setpoints(self, n_candidates: int = 1000) -> Dict[str, float]:
        """
        Suggest optimal environment setpoints using Bayesian optimization.

        Returns personalized setpoints or population priors if not enough data.
        """
        if self.nights < MIN_NIGHTS_FOR_PERSONALIZATION:
            print(f"Only {self.nights} nights — using population priors")
            return {
                "temperature": POPULATION_OPTIMAL["temperature"]["mean"],
                "humidity": POPULATION_OPTIMAL["humidity"]["mean"],
                "confidence": 0.3,
                "source": "population",
            }

        self.fit_model()

        # Generate candidate setpoints
        candidates = np.random.uniform(
            low=[self.param_bounds["temperature"][0], self.param_bounds["humidity"][0]],
            high=[self.param_bounds["temperature"][1], self.param_bounds["humidity"][1]],
            size=(n_candidates, 2)
        )

        # Evaluate EI for each candidate
        ei_values = np.array([self.expected_improvement(c.reshape(1, -1)) for c in candidates])

        # Also consider GP mean prediction (exploitation)
        mu_values = self.gp.predict(candidates).flatten()

        # Combined score: weighted EI + mean prediction
        combined = 0.5 * ei_values + 0.5 * mu_values
        best_idx = np.argmax(combined)

        best_temp = candidates[best_idx, 0]
        best_hum = candidates[best_idx, 1]
        best_mu, best_sigma = self.gp.predict(candidates[best_idx].reshape(1, -1), return_std=True)

        confidence = max(0.3, 1.0 - best_sigma[0] / 0.5)  # Higher confidence with more data

        return {
            "temperature": round(best_temp, 1),
            "humidity": round(best_hum, 1),
            "confidence": round(confidence, 2),
            "source": "personalized",
            "expected_score": round(best_mu[0], 3),
        }

    def suggest_stage_setpoints(self) -> Dict[str, Dict[str, float]]:
        """Suggest setpoints per sleep stage based on observed data"""
        base = self.suggest_setpoints()

        # Research-based offsets per stage
        stage_offsets = {
            "deep": {"temperature_offset": -1.5, "humidity_offset": 0},
            "rem": {"temperature_offset": 0.5, "humidity_offset": 0},
            "light": {"temperature_offset": -0.75, "humidity_offset": 0},
            "awake": {"temperature_offset": 0, "humidity_offset": 0},
        }

        result = {"base": base}

        for stage, offsets in stage_offsets.items():
            result[stage] = {
                "temperature": round(base["temperature"] + offsets["temperature_offset"], 1),
                "humidity": round(base["humidity"] + offsets["humidity_offset"], 1),
                "confidence": base.get("confidence", 0.3),
            }

        return result

    def generate_report(self) -> Dict:
        """Generate a summary report of environment optimization"""
        if self.nights == 0:
            return {"message": "No data yet"}

        X = np.array(self.X_observed)
        y = np.array(self.y_observed)

        # Find best and worst nights
        best_idx = np.argmax(y)
        worst_idx = np.argmin(y)

        report = {
            "nights_observed": self.nights,
            "best_night": {
                "temperature": X[best_idx, 0],
                "humidity": X[best_idx, 1],
                "score": round(y[best_idx], 3),
            },
            "worst_night": {
                "temperature": X[worst_idx, 0],
                "humidity": X[worst_idx, 1],
                "score": round(y[worst_idx], 3),
            },
            "average_conditions": {
                "temperature": round(np.mean(X[:, 0]), 1),
                "humidity": round(np.mean(X[:, 1]), 1),
            },
            "average_score": round(np.mean(y), 3),
            "recommendations": self.suggest_stage_setpoints(),
        }

        # Temperature correlation
        if self.nights >= 5:
            temp_corr = np.corrcoef(X[:, 0], y)[0, 1]
            report["temperature_correlation"] = round(temp_corr, 2)
            if abs(temp_corr) > 0.3:
                direction = "cooler" if temp_corr < 0 else "warmer"
                report["temperature_insight"] = (
                    f"Your sleep quality {'improves' if temp_corr > 0 else 'suffers'} "
                    f"when the room is {direction}."
                )

        return report


def demo():
    """Run a demo with synthetic data"""
    print("=== SleepSync Environment Optimizer Demo ===\n")

    optimizer = SleepEnvironmentOptimizer()

    # Simulate 30 nights of data for a user who sleeps best at 18.5°C, 42% RH
    np.random.seed(42)
    for night in range(30):
        temp = np.random.uniform(16.0, 23.0)
        hum = np.random.uniform(30.0, 60.0)

        # Simulated sleep quality: peaks at optimal conditions with noise
        optimal_temp = 18.5
        optimal_hum = 42.0
        temp_penalty = -0.05 * (temp - optimal_temp) ** 2
        hum_penalty = -0.01 * (hum - optimal_hum) ** 2
        noise = np.random.normal(0, 0.05)
        quality = max(0, min(1, 0.7 + temp_penalty + hum_penalty + noise))

        # Derived metrics
        deep_pct = 15 + 5 * quality + np.random.normal(0, 2)
        rem_pct = 20 + 5 * quality + np.random.normal(0, 2)
        waso = max(0, int(5 - 4 * quality + np.random.normal(0, 1)))

        optimizer.add_night_observation(temp, hum, quality * 100, deep_pct, rem_pct, waso)

    # Generate report
    report = optimizer.generate_report()
    print("Optimization Report:")
    print(f"  Nights observed: {report['nights_observed']}")
    print(f"  Average score: {report['average_score']}")
    print(f"  Best night: {report['best_night']}")
    print(f"  Recommendations: {json.dumps(report['recommendations'], indent=2)}")


import json

if __name__ == "__main__":
    demo()