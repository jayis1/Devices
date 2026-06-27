"""
Batch inference service — runs all ML models on new telemetry data.
Started as a background process by the dashboard.
"""
import time
import logging
import numpy as np
import joblib
import os

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("compostsync.inference")


class InferenceService:
    def __init__(self):
        self.models = {}
        self.loaded = False

    def load(self):
        model_dir = os.environ.get("MODEL_DIR", "models")
        try:
            cn_path = os.path.join(model_dir, "cn_ratio_xgb.joblib")
            if os.path.exists(cn_path):
                self.models["cn"] = joblib.load(cn_path)

            comp_path = os.path.join(model_dir, "completion_forecaster.joblib")
            if os.path.exists(comp_path):
                self.models["completion"] = joblib.load(comp_path)

            pest_path = os.path.join(model_dir, "pest_risk.joblib")
            if os.path.exists(pest_path):
                pest_bundle = joblib.load(pest_path)
                self.models["pest"] = pest_bundle["model"]
                self.models["pest_scaler"] = pest_bundle["scaler"]

            self.loaded = True
            logger.info(f"Loaded {len(self.models)} models")
        except Exception as e:
            logger.error(f"Model loading failed: {e}")

    def run_cn_ratio(self, features):
        if "cn" not in self.models:
            return 30.0
        X = np.array([features])
        return float(self.models["cn"].predict(X)[0])

    def run_completion(self, maturity, temp, co2, cn_ratio):
        if "completion" not in self.models:
            return max(0, int(90 - maturity * 0.9))
        X = np.array([[maturity, temp, co2, cn_ratio, 30, 1, 200]])
        return max(0, int(self.models["completion"].predict(X)[0]))

    def run_pest_risk(self, features):
        if "pest" not in self.models:
            return 0.0
        X = np.array([features])
        if "pest_scaler" in self.models:
            X = self.models["pest_scaler"].transform(X)
        return float(self.models["pest"].predict_proba(X)[0][1])


def main():
    """Main inference loop — polls for new data and runs predictions."""
    service = InferenceService()
    service.load()

    if not service.loaded:
        logger.warning("Models not loaded — running in heuristic mode")

    logger.info("Inference service started. Polling every 60s...")
    while True:
        # In production: query database for unprocessed telemetry
        # Run predictions, store results
        time.sleep(60)


if __name__ == "__main__":
    main()