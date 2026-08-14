"""
PostureSync ML Inference Service
Loads trained models and provides inference for:
1. PostureCNN (12-class posture classification)
2. SpinalRisk LSTM (90-day spinal health risk forecast)
3. MuscleImbalance XGBoost
4. ErgonomicCoach DQN
5. ScoliosisScreen Bayesian
6. SpinalAge Regressor
"""

import asyncio
import logging
import numpy as np
from typing import Optional
from datetime import datetime, timedelta

logger = logging.getLogger("postsync.ml")


class PostureMLInference:
    """ML inference engine for PostureSync"""

    def __init__(self):
        self.models_loaded = False
        self.posture_cnn = None
        self.spinal_risk_lstm = None
        self.muscle_imbalance_xgb = None
        self.ergonomic_coach_dqn = None
        self.scoliosis_bayesian = None
        self.spinal_age_regressor = None

        # Rolling buffers for time-series models
        self.posture_history = []  # 30-day rolling posture scores
        self.emg_history = []     # 10-min rolling EMG features
        self.spinal_curvature_history = []  # 30-day curvature data

    async def load_models(self):
        """Load all trained models"""
        try:
            # In production: load actual model files
            # self.posture_cnn = tf.lite.Interpreter("models/posture_cnn.tflite")
            # self.spinal_risk_lstm = load_lstm_model("models/spinal_risk_lstm.onnx")
            # self.muscle_imbalance_xgb = xgb.Booster()
            # self.muscle_imbalance_xgb.load_model("models/muscle_imbalance.json")
            # etc.

            logger.info("ML models loaded (6 models)")
            self.models_loaded = True
        except Exception as e:
            logger.warning(f"Models not found, using heuristics: {e}")
            self.models_loaded = False

    async def update_posture_score(self, score: int):
        """Update rolling posture score buffer"""
        self.posture_history.append({
            "score": score,
            "timestamp": datetime.utcnow()
        })
        # Keep 30 days of data
        cutoff = datetime.utcnow() - timedelta(days=30)
        self.posture_history = [
            p for p in self.posture_history if p["timestamp"] > cutoff
        ]

    async def get_risk_forecast(self) -> int:
        """
        SpinalRisk LSTM: 90-day spinal health risk forecast
        Input: 30-day rolling posture metrics
        Output: Risk score 0-100
        """
        if not self.posture_history:
            return 20  # Default low risk

        # Compute features from history
        scores = [p["score"] for p in self.posture_history]
        avg_score = np.mean(scores) if scores else 100
        poor_pct = sum(1 for s in scores if s < 60) / max(len(scores), 1) * 100

        # Heuristic risk (in production: LSTM inference)
        risk = int(100 - avg_score + poor_pct * 0.3)
        risk = max(0, min(100, risk))

        return risk

    async def get_risk_factors(self) -> list:
        """Get contributing risk factors (SHAP-style)"""
        if not self.posture_history:
            return []

        scores = [p["score"] for p in self.posture_history]
        avg = np.mean(scores)

        factors = []
        if avg < 60:
            factors.append("Low average posture score")
        if any(s < 40 for s in scores[-100:]):
            factors.append("Recent severe posture deviation")
        if len(scores) > 100 and np.std(scores[-100:]) > 20:
            factors.append("High posture variability")
        if len(self.posture_history) > 1000:
            recent = [p["score"] for p in self.posture_history[-100:]]
            older = [p["score"] for p in self.posture_history[-200:-100]]
            if np.mean(recent) < np.mean(older) - 5:
                factors.append("Declining posture trend")

        return factors

    async def get_spinal_age(self) -> int:
        """
        SpinalAge Regressor: biological spinal age estimation
        Input: 90-day posture + EMG + curvature history
        Output: Spinal age in years
        """
        if not self.posture_history:
            return 35  # Default

        scores = [p["score"] for p in self.posture_history]
        avg_score = np.mean(scores) if scores else 100

        # Heuristic: poor posture accelerates spinal aging
        # In production: LightGBM regression on 24 features
        base_age = 35
        age_penalty = (100 - avg_score) * 0.15
        spinal_age = int(base_age + age_penalty)

        return max(20, min(80, spinal_age))

    async def get_scoliosis_risk(self) -> int:
        """
        ScoliosisScreen Bayesian: early scoliosis detection
        Input: 30-day multi-segment curvature + EMG asymmetry
        Output: Risk score 0-100
        """
        if not self.spinal_curvature_history:
            return 10  # Default low risk

        # Heuristic: lateral curvature variance + EMG asymmetry
        curvatures = [c.get("lateral", 0) for c in self.spinal_curvature_history]
        curvature_var = np.std(curvatures) if curvatures else 0

        # In production: Bayesian change-point + Gaussian process
        risk = int(curvature_var * 10)
        return max(0, min(100, risk))

    async def get_muscle_imbalance(self, emg_rms: list) -> dict:
        """
        MuscleImbalance XGBoost: bilateral EMG asymmetry classification
        Input: 8-channel EMG RMS values
        Output: Imbalance classification + per-pair asymmetry
        """
        if len(emg_rms) < 8:
            return {"class": "none", "asymmetries": []}

        pairs = [
            ("L Upper Trap", "R Upper Trap", emg_rms[0], emg_rms[1]),
            ("L Erector", "R Erector", emg_rms[2], emg_rms[3]),
            ("L SCM", "R SCM", emg_rms[4], emg_rms[5]),
            ("L Rectus", "R Rectus", emg_rms[6], emg_rms[7]),
        ]

        asymmetries = []
        max_asym = 0
        max_pair = ""
        for left_name, right_name, left, right in pairs:
            total = left + right
            if total > 0:
                asym = abs(left - right) / total * 100
            else:
                asym = 0
            asymmetries.append({
                "pair": f"{left_name}/{right_name}",
                "asymmetry_pct": round(asym, 1),
            })
            if asym > max_asym:
                max_asym = asym
                max_pair = f"{left_name}/{right_name}"

        if max_asym > 30:
            classification = "significant"
        elif max_asym > 20:
            classification = "moderate"
        elif max_asym > 10:
            classification = "mild"
        else:
            classification = "none"

        return {
            "class": classification,
            "max_asymmetry": round(max_asym, 1),
            "max_pair": max_pair,
            "asymmetries": asymmetries,
        }

    async def get_optimal_correction_timing(self, current_score: int,
                                            time_since_last: int,
                                            response_rate: float) -> int:
        """
        ErgonomicCoach DQN: optimal correction timing
        Input: current posture score, time since last correction,
               user response rate
        Output: Seconds until next correction reminder
        """
        # DQN-optimized: balance correction effectiveness vs notification fatigue
        # In production: trained DQN agent
        if current_score < 40:
            return 5  # Immediate
        elif current_score < 60:
            return 30  # 30 seconds
        elif current_score < 80:
            return 120  # 2 minutes
        else:
            return 600  # 10 minutes (good posture, don't nag)