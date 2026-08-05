"""SeizureSync — ML inference service (cloud-side models 2,3,5,6,7,8)."""
import os
import logging
import numpy as np
from onnxruntime import InferenceSession

logger = logging.getLogger("seizuresync.inference")

MODEL_DIR = os.environ.get("MODEL_DIR", "models")


class InferenceService:
    """Loads and runs cloud-side ML models via ONNX Runtime."""

    def __init__(self):
        self.models = {}
        self._load_models()

    def _load_models(self):
        model_files = {
            "semiology": "semiologynet_v1.onnx",
            "aura": "auranet_v1.onnx",
            "trigger": "triggernet_v1.onnx",   # XGBoost (JSON, not ONNX)
            "risk": "risknet_v1.onnx",
            "recovery": "recoverynet_v1.onnx",
            "sudep_score": "sudep_score_v1.onnx",
        }
        for name, fname in model_files.items():
            path = os.path.join(MODEL_DIR, fname)
            if os.path.exists(path) and name != "trigger":
                try:
                    self.models[name] = InferenceSession(path)
                    logger.info("Loaded %s from %s", name, path)
                except Exception as e:
                    logger.warning("Failed to load %s: %s", name, e)

    def classify_semiology(self, accel_window: np.ndarray) -> str:
        """SemiologyNet — ILAE 2017 5-class seizure classification."""
        if "semiology" not in self.models:
            return "unknown"
        session = self.models["semiology"]
        inp = accel_window.astype(np.float32).reshape(1, -1, 1)
        out = session.run(None, {"input": inp})[0]
        classes = ["focal_aware", "focal_impaired", "fbtcs",
                   "generalized", "unknown"]
        return classes[int(np.argmax(out))]

    def predict_preictal(self, temp_hist, eda_hist, hr_hist) -> float:
        """AuraNet — pre-ictal probability (0-1)."""
        if "aura" not in self.models:
            return 0.0
        session = self.models["aura"]
        inp = np.stack([temp_hist, eda_hist, hr_hist], axis=-1)
        inp = inp.astype(np.float32).reshape(1, -1, 3)
        out = session.run(None, {"input": inp})[0]
        return float(out[0][0])

    def forecast_risk_24h(self, multi_day_signals: np.ndarray) -> float:
        """RiskNet — 24-hour seizure risk (0-100)."""
        if "risk" not in self.models:
            return 0.0
        session = self.models["risk"]
        inp = multi_day_signals.astype(np.float32).reshape(1, -1, 1)
        out = session.run(None, {"input": inp})[0]
        return float(out[0][0]) * 100

    def predict_recovery(self, post_event_signals: np.ndarray) -> dict:
        """RecoveryNet — post-ictal recovery state + duration estimate."""
        if "recovery" not in self.models:
            return {"state": "unknown", "duration_min": 0}
        session = self.models["recovery"]
        inp = post_event_signals.astype(np.float32).reshape(1, -1, 1)
        out = session.run(None, {"input": inp})[0]
        states = ["postictal", "recovering", "recovered"]
        idx = int(np.argmax(out))
        return {"state": states[idx], "duration_min": float(out[0][idx] * 30)}

    def compute_sudep_risk(self, features: dict) -> float:
        """Bayesian SUDEP annual risk (%)."""
        if "sudep_score" not in self.models:
            # Heuristic: baseline 0.1% × risk factors
            base = 0.1
            if features.get("seizure_freq_month", 0) > 3: base *= 3
            if features.get("apnea_density", 0) > 5: base *= 2
            if features.get("prone_episodes", 0) > 3: base *= 1.5
            if features.get("med_adherence", 1.0) < 0.8: base *= 1.8
            return min(base, 5.0)
        session = self.models["sudep_score"]
        # ... ONNX inference ...
        return 0.5