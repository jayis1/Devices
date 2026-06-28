"""
PestSync Backend — ML Inference Service
software/dashboard/ml/inference.py
"""
import logging
from datetime import datetime, timezone
from typing import Optional

logger = logging.getLogger("pestsync.ml")

# Pest class names
PEST_NAMES = {
    0: "House Mouse", 1: "Norway Rat", 2: "German Cockroach", 3: "American Cockroach",
    4: "Argentine Ant", 5: "Carpenter Ant", 6: "Mosquito", 7: "House Fly",
    8: "Fruit Fly", 9: "Bedbug", 10: "Termite (worker)", 11: "Termite (swarmer)",
    12: "Spider", 13: "Silverfish", 14: "Carpet Beetle", 255: "None",
}


class MLInferenceService:
    """Cloud ML inference service for infestation risk, activity patterns, and recommendations."""

    def __init__(self):
        self._models_loaded = False

    async def load_models(self):
        """Load trained models from disk."""
        # In production: load XGBoost, LSTM, and other models
        logger.info("Loading ML models...")
        self._models_loaded = True
        logger.info("ML models loaded: infestation_risk, activity_lstm, deterrent_effectiveness")

    async def predict_infestation_risk(
        self, user_id: int, pest_type: str, days: int = 30
    ) -> dict:
        """
        Predict 30-day infestation risk for a specific pest type.
        Uses XGBoost regressor on 14-day activity trends + weather + season.
        """
        if not self._models_loaded:
            await self.load_models()

        # Placeholder: in production, run actual model inference
        risk_score = 0.35  # 0-1
        risk_level = "moderate"
        if risk_score > 0.7:
            risk_level = "critical"
        elif risk_score > 0.4:
            risk_level = "high"
        elif risk_score > 0.15:
            risk_level = "moderate"
        else:
            risk_level = "low"

        recommendation = self._generate_recommendation(pest_type, risk_score, risk_level)

        return {
            "pest_type": pest_type,
            "risk_score": risk_score,
            "risk_level": risk_level,
            "forecast_days": days,
            "recommendation": recommendation,
            "timestamp": datetime.now(timezone.utc).isoformat(),
        }

    async def predict_activity_pattern(self, device_id: str, days: int = 7) -> dict:
        """
        Predict peak activity hours using LSTM on 7-day hourly detection data.
        Used for adaptive deterrent scheduling.
        """
        if not self._models_loaded:
            await self.load_models()

        # Placeholder: LSTM predicts peak hours
        return {
            "device_id": device_id,
            "pattern_type": "nocturnal",
            "peak_hours": [2, 3, 4],
            "confidence": 0.82,
            "recommended_deterrent_schedule": {
                "ultrasonic_on": "22:00",
                "ultrasonic_off": "06:00",
                "strobe_bursts": "01:00,03:00,05:00",
            },
        }

    async def assess_deterrent_effectiveness(
        self, device_id: str, days: int = 7
    ) -> dict:
        """Assess how effective a deterrent has been over the past N days."""
        if not self._models_loaded:
            await self.load_models()

        # Placeholder: logistic regression on pre/post activity
        pre_count = 45
        post_count = 12
        reduction = (pre_count - post_count) / pre_count if pre_count > 0 else 0

        if reduction > 0.5:
            verdict = "highly_effective"
        elif reduction > 0.2:
            verdict = "effective"
        elif reduction > 0:
            verdict = "partially_effective"
        else:
            verdict = "ineffective"

        return {
            "device_id": device_id,
            "days": days,
            "pre_activity_count": pre_count,
            "post_activity_count": post_count,
            "reduction_pct": round(reduction * 100, 1),
            "effectiveness_score": round(reduction, 2),
            "verdict": verdict,
        }

    def _generate_recommendation(self, pest_type: str, risk: float, level: str) -> str:
        """Generate treatment recommendation based on pest type and risk."""
        recs = {
            "House Mouse": "Set snap traps along walls (mice travel edges). "
                           "Seal gaps >6mm. Activate ultrasonic deterrent 20-30 kHz at night.",
            "Norway Rat": "Use large snap traps or electronic traps. "
                          "Seal all entry points. Consider professional extermination.",
            "German Cockroach": "Deploy gel bait stations in kitchen/bathroom. "
                                "Eliminate water sources. Activate ultrasonic 40-60 kHz. "
                                "Seal cracks. If count >20, call professional.",
            "American Cockroach": "Seal drain entry points. Use glue boards. "
                                  "Reduce humidity. Ultrasonic 40-60 kHz deterrent.",
            "Argentine Ant": "Find and seal entry point. Use ant bait stations. "
                             "Clean food surfaces. Remove moisture sources.",
            "Termite (swarmer)": "🚨 CRITICAL: Call a licensed pest professional immediately "
                                  "for inspection. Termites cause $5B/year in US property damage.",
        }
        base_rec = recs.get(pest_type, "Monitor and set traps as needed.")

        if level == "critical":
            return f"⚠️ CRITICAL RISK: {base_rec} Consider professional treatment."
        elif level == "high":
            return f"🔴 HIGH RISK: {base_rec}"
        elif level == "moderate":
            return f"🟡 MODERATE: {base_rec}"
        else:
            return f"🟢 LOW RISK: Continue monitoring. {base_rec}"


ml_service = MLInferenceService()