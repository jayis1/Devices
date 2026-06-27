"""
CompostSync Backend — ML Inference Service
Loads trained models and runs predictions on incoming telemetry.
"""
import logging
import os
import pickle
import numpy as np
from datetime import datetime

from config import settings

logger = logging.getLogger("compostsync.ml")


class MLInference:
    def __init__(self):
        self.models = {}
        self.loaded = False

    def load_models(self):
        """Load all trained models from disk."""
        model_dir = settings.model_dir
        try:
            # Load maturity LSTM (PyTorch or ONNX)
            maturity_path = os.path.join(model_dir, "maturity_lstm.onnx")
            if os.path.exists(maturity_path):
                import onnxruntime as ort
                self.models["maturity"] = ort.InferenceSession(maturity_path)
                logger.info("Loaded maturity LSTM model")

            # Load C:N ratio estimator (XGBoost via joblib)
            cn_path = os.path.join(model_dir, "cn_ratio_xgb.joblib")
            if os.path.exists(cn_path):
                self.models["cn_ratio"] = pickle.load(open(cn_path, "rb"))
                logger.info("Loaded C:N ratio model")

            # Load completion forecaster
            completion_path = os.path.join(model_dir, "completion_forecaster.joblib")
            if os.path.exists(completion_path):
                self.models["completion"] = pickle.load(open(completion_path, "rb"))
                logger.info("Loaded completion forecaster")

            # Load pest risk predictor
            pest_path = os.path.join(model_dir, "pest_risk.joblib")
            if os.path.exists(pest_path):
                self.models["pest_risk"] = pickle.load(open(pest_path, "rb"))
                logger.info("Loaded pest risk model")

            self.loaded = True
            logger.info("All ML models loaded successfully")
        except Exception as e:
            logger.error(f"Failed to load models: {e}")
            self.loaded = False

    async def predict(self, telemetry_data):
        """Run all applicable models on incoming telemetry data."""
        if not self.loaded:
            return self._heuristic_predict(telemetry_data)

        result = {
            "timestamp": datetime.utcnow().isoformat(),
            "maturity_score": None,
            "cn_ratio": None,
            "days_to_ready": None,
            "phase": None,
            "recommendation": None,
            "pest_risk": None,
        }

        try:
            # Extract features
            temps = telemetry_data.get("t1", 0) / 10.0
            co2 = telemetry_data.get("co2", 0)
            methane = telemetry_data.get("ch4", 0)
            moisture = telemetry_data.get("m2", 0)
            mass = telemetry_data.get("mass", 0)

            # C:N ratio prediction
            if "cn_ratio" in self.models:
                features = np.array([[temps, co2, methane, moisture, mass]])
                cn = self.models["cn_ratio"].predict(features)[0]
                result["cn_ratio"] = round(float(cn), 1)

            # Maturity prediction (requires time series — simplified for single point)
            result["maturity_score"] = self._estimate_maturity(temps, co2, methane)

            # Phase classification
            result["phase"] = self._classify_phase(temps, co2, methane)

            # Completion forecast
            if "completion" in self.models and result["maturity_score"] is not None:
                features = np.array([[result["maturity_score"], temps, co2, result["cn_ratio"] or 30]])
                days = self.models["completion"].predict(features)[0]
                result["days_to_ready"] = max(0, int(days))

            # Recommendation
            result["recommendation"] = self._generate_recommendation(
                result["phase"], result["cn_ratio"], temps, moisture, methane, co2
            )

        except Exception as e:
            logger.error(f"ML prediction error: {e}")

        return result

    def _heuristic_predict(self, data):
        """Fallback heuristic prediction when models aren't loaded."""
        temps = data.get("t2", data.get("t1", 0)) / 10.0
        co2 = data.get("co2", 0)
        methane = data.get("ch4", 0)
        moisture = data.get("m2", 0)

        phase = self._classify_phase(temps, co2, methane)
        maturity = self._estimate_maturity(temps, co2, methane)
        cn_ratio = 30.0  # default

        return {
            "timestamp": datetime.utcnow().isoformat(),
            "maturity_score": maturity,
            "cn_ratio": cn_ratio,
            "days_to_ready": max(0, int(90 - maturity * 0.9)),
            "phase": phase,
            "recommendation": self._generate_recommendation(phase, cn_ratio, temps, moisture, methane, co2),
            "pest_risk": 0.1,
        }

    def _classify_phase(self, temp, co2, methane):
        if temp < 5 and co2 < 200:
            return "dormant"
        if temp > 50 and co2 > 1000:
            return "thermophilic"
        if temp > 30 and temp < 55 and co2 > 500:
            return "cooling"
        if temp < 30 and co2 < 800 and co2 > 200:
            return "maturation"
        if temp < 25 and co2 < 300 and methane < 50:
            return "cured"
        return "mesophilic"

    def _estimate_maturity(self, temp, co2, methane):
        """Heuristic maturity score 0-100."""
        if temp < 5 and co2 < 200:
            return 0.0
        if temp > 50:
            return 20.0 + min(30, (temp - 50) * 2)
        if temp > 30 and temp <= 50:
            return 50.0 + (50 - temp)
        if temp <= 30 and co2 < 800:
            return 70.0 + min(20, (800 - co2) / 40)
        if co2 < 300 and methane < 50:
            return 95.0
        return 40.0

    def _generate_recommendation(self, phase, cn_ratio, temp, moisture, methane, co2):
        if methane > 1000:
            return "🚨 TURN PILE NOW — methane indicates anaerobic conditions. Mix in dry browns."
        if moisture > 70:
            return "Add dry carbon: shredded cardboard, sawdust, or dry leaves."
        if moisture < 30:
            return "Add water — moisture is too low for microbial activity."
        if phase == "thermophilic":
            return f"🔥 Thermophilic phase! Temp {temp:.1f}°C. Turn in 3-5 days."
        if phase == "cooling":
            return "Cooling phase. Turn once more to finish. C:N est: {:.0f}:1".format(cn_ratio)
        if phase == "maturation":
            return "Maturation phase. Almost done! Let rest 2-3 more weeks."
        if phase == "cured":
            return "🎉 CURED! Your compost is ready to harvest."
        if phase == "dormant":
            return "💤 Pile is dormant. Add nitrogen-rich greens to restart."
        return f"Mesophilic phase. Keep adding materials. C:N est: {cn_ratio:.0f}:1"


# Global instance
ml_inference = MLInference()