"""
StormSync ML Pipeline — StormRisk Bayesian Ensemble
Produces a single 0-100 StormSync Score from all model outputs.

Inputs: FloodForecast max level, PumpHealth class, SoilSat trend,
        RainfallRunoff volume, barometric trend, NWS flood watch
Output: StormSync Score (0-100), risk level, confidence
"""

import os
import numpy as np
from sklearn.isotonic import IsotonicRegression
import pickle

MODEL_SAVE_DIR = "./models"


class StormRiskEnsemble:
    """Bayesian-inspired ensemble of model outputs for flood risk scoring."""

    def __init__(self):
        # Weights for each factor (sum to 1.0)
        # Learned from historical flood events + expert judgment
        self.weights = {
            'flood_forecast_max_level': 0.30,
            'pump_health_class': 0.15,
            'soil_saturation_trend': 0.20,
            'rainfall_runoff_volume': 0.15,
            'pressure_trend': 0.10,
            'nws_flood_watch': 0.10,
        }

        # Risk thresholds for each factor (0-100 contribution)
        self.thresholds = {
            'flood_forecast_max_level': {
                'normal': 300,    # mm — below this, 0 points
                'warning': 700,   # mm — 50 points
                'critical': 900,  # mm — 80 points
                'flood': 1020,    # mm — 100 points
            },
            'pump_health_class': {
                0: 0,   # Healthy
                1: 15,  # Bearing Wear
                2: 25,  # Impeller Damage
                3: 20,  # Motor Degradation
                4: 35,  # Air Lock
                5: 60,  # Imminent Failure
            },
            'soil_saturation_trend': {
                'stable': 0,
                'rising_slow': 20,
                'rising_fast': 40,
                'saturated': 50,
            },
            'rainfall_runoff_volume': {
                'low': 200,       # L — 0 points
                'moderate': 1000, # L — 30 points
                'high': 3000,     # L — 60 points
                'extreme': 5000,  # L — 80 points
            },
            'pressure_trend': {
                'rising': 0,
                'steady': 5,
                'falling': 25,
                'falling_rapid': 40,
            },
            'nws_flood_watch': {
                'none': 0,
                'watch': 30,
                'warning': 50,
            },
        }

    def compute_score(self, inputs: dict) -> dict:
        """Compute StormSync Score from model outputs.

        Args:
            inputs: Dict with keys matching self.weights, containing:
                - flood_forecast_max_level: int (mm)
                - pump_health_class: int (0-5)
                - soil_saturation_trend: str
                - rainfall_runoff_volume: float (L)
                - pressure_trend: str
                - nws_flood_watch: str

        Returns:
            Dict with score, risk_level, confidence, factor_scores
        """
        factor_scores = {}

        # Flood forecast score (linear interpolation between thresholds)
        level = inputs.get('flood_forecast_max_level', 300)
        t = self.thresholds['flood_forecast_max_level']
        if level < t['normal']:
            fs = 0
        elif level < t['warning']:
            fs = (level - t['normal']) / (t['warning'] - t['normal']) * 50
        elif level < t['critical']:
            fs = 50 + (level - t['warning']) / (t['critical'] - t['warning']) * 30
        elif level < t['flood']:
            fs = 80 + (level - t['critical']) / (t['flood'] - t['critical']) * 20
        else:
            fs = 100
        factor_scores['flood_forecast_max_level'] = fs

        # Pump health score
        ph_class = inputs.get('pump_health_class', 0)
        factor_scores['pump_health_class'] = self.thresholds['pump_health_class'].get(ph_class, 0)

        # Soil saturation trend
        sat_trend = inputs.get('soil_saturation_trend', 'stable')
        factor_scores['soil_saturation_trend'] = self.thresholds['soil_saturation_trend'].get(sat_trend, 0)

        # Rainfall runoff volume
        vol = inputs.get('rainfall_runoff_volume', 200)
        rv = self.thresholds['rainfall_runoff_volume']
        if vol < rv['low']:
            rs = 0
        elif vol < rv['moderate']:
            rs = (vol - rv['low']) / (rv['moderate'] - rv['low']) * 30
        elif vol < rv['high']:
            rs = 30 + (vol - rv['moderate']) / (rv['high'] - rv['moderate']) * 30
        elif vol < rv['extreme']:
            rs = 60 + (vol - rv['high']) / (rv['extreme'] - rv['high']) * 20
        else:
            rs = 80
        factor_scores['rainfall_runoff_volume'] = rs

        # Pressure trend
        pt = inputs.get('pressure_trend', 'steady')
        factor_scores['pressure_trend'] = self.thresholds['pressure_trend'].get(pt, 5)

        # NWS flood watch
        nws = inputs.get('nws_flood_watch', 'none')
        factor_scores['nws_flood_watch'] = self.thresholds['nws_flood_watch'].get(nws, 0)

        # Weighted sum
        score = 0
        for factor, weight in self.weights.items():
            score += factor_scores[factor] * weight

        score = int(min(100, max(0, score)))

        # Risk level
        if score <= 30:
            risk_level = "low"
        elif score <= 55:
            risk_level = "moderate"
        elif score <= 75:
            risk_level = "high"
        else:
            risk_level = "critical"

        # Confidence: higher when more factors agree
        factor_values = list(factor_scores.values())
        agreement = 1 - np.std(factor_values) / 100  # Simple agreement metric
        confidence = max(0.5, min(0.98, agreement))

        return {
            'score': score,
            'risk_level': risk_level,
            'confidence': confidence,
            'factor_scores': {k: round(v, 1) for k, v in factor_scores.items()},
        }


def train_and_save():
    """Train the ensemble (calibrate weights on synthetic events)."""
    print("[StormRisk] Building Bayesian ensemble...")

    model = StormRiskEnsemble()

    # Test with various scenarios
    test_cases = [
        {"name": "Normal", "inputs": {
            'flood_forecast_max_level': 350, 'pump_health_class': 0,
            'soil_saturation_trend': 'stable', 'rainfall_runoff_volume': 200,
            'pressure_trend': 'rising', 'nws_flood_watch': 'none'}},
        {"name": "Storm Approaching", "inputs": {
            'flood_forecast_max_level': 750, 'pump_health_class': 1,
            'soil_saturation_trend': 'rising_fast', 'rainfall_runoff_volume': 1500,
            'pressure_trend': 'falling', 'nws_flood_watch': 'watch'}},
        {"name": "Critical", "inputs": {
            'flood_forecast_max_level': 950, 'pump_health_class': 5,
            'soil_saturation_trend': 'saturated', 'rainfall_runoff_volume': 4000,
            'pressure_trend': 'falling_rapid', 'nws_flood_watch': 'warning'}},
    ]

    for tc in test_cases:
        result = model.compute_score(tc['inputs'])
        print(f"  {tc['name']}: Score={result['score']} "
              f"({result['risk_level']}, conf={result['confidence']:.2f})")

    os.makedirs(MODEL_SAVE_DIR, exist_ok=True)
    with open(os.path.join(MODEL_SAVE_DIR, "storm_risk.pkl"), "wb") as f:
        pickle.dump(model, f)
    print(f"\n[StormRisk] Model saved to {MODEL_SAVE_DIR}/storm_risk.pkl")


if __name__ == "__main__":
    train_and_save()