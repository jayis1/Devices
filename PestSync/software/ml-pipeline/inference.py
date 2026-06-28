"""
PestSync ML Pipeline — Batch Inference Service
software/ml-pipeline/inference.py

Runs hourly batch inference on new data using all trained models.
Publishes results to MQTT for real-time mobile app updates.
"""
import asyncio
import logging
import os
from datetime import datetime, timezone

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(message)s")
logger = logging.getLogger("pestsync.inference")

MODEL_DIR = os.environ.get("PESTSYNC_MODEL_DIR", "models")


class InferenceService:
    """Batch inference service that runs all ML models on new data."""

    def __init__(self):
        self.infestation_model = None
        self.activity_lstm = None
        self.deterrent_model = None
        self._load_models()

    def _load_models(self):
        """Load all trained models."""
        try:
            import joblib
            inf_path = os.path.join(MODEL_DIR, "infestation_risk_xgb.joblib")
            if os.path.exists(inf_path):
                self.infestation_model = joblib.load(inf_path)
                logger.info("Loaded infestation risk model")
        except Exception as e:
            logger.warning("Could not load infestation model: %s", e)

        try:
            import joblib
            deter_path = os.path.join(MODEL_DIR, "deterrent_effect_logreg.joblib")
            if os.path.exists(deter_path):
                self.deterrent_model = joblib.load(deter_path)
                logger.info("Loaded deterrent effectiveness model")
        except Exception as e:
            logger.warning("Could not load deterrent model: %s", e)

    async def run_hourly_inference(self):
        """Run all inference models on the latest data."""
        logger.info("Starting hourly inference cycle...")

        # 1. Infestation risk forecast (per user, per pest type)
        await self._run_infestation_forecast()

        # 2. Activity pattern analysis (per sentinel device)
        await self._run_activity_analysis()

        # 3. Deterrent effectiveness assessment (per deterrent device)
        await self._run_deterrent_assessment()

        logger.info("Hourly inference complete")

    async def _run_infestation_forecast(self):
        """Run infestation risk forecaster for all active users."""
        # In production: query DB for recent detections per user
        # For each user × pest type combination:
        #   Extract 18 features from 14-day activity
        #   Run XGBoost predict
        #   Store result in infestation_risk table
        #   If risk > 0.7, create critical alert + push notification
        logger.info("  Running infestation risk forecast...")

        # Placeholder: would query DB and run model
        # features = extract_features(user_id, pest_type)
        # risk = self.infestation_model.predict([features])
        # store_risk(user_id, pest_type, risk)
        # if risk > 0.7: send_alert(user_id, "critical", ...)

    async def _run_activity_analysis(self):
        """Run activity pattern LSTM for all sentinel devices."""
        logger.info("  Running activity pattern analysis...")
        # In production: load 7-day hourly detection counts per sentinel
        # Run LSTM → predict pattern type + peak hours
        # Update adaptive deterrent schedules

    async def _run_deterrent_assessment(self):
        """Run deterrent effectiveness model for all deterrent devices."""
        logger.info("  Running deterrent effectiveness assessment...")
        # In production: compare pre/post activity counts per deterrent
        # Run logistic regression → effectiveness score
        # Update deterrent status with effectiveness metric

    async def run_forever(self):
        """Run inference service indefinitely."""
        logger.info("PestSync inference service started")
        while True:
            try:
                await self.run_hourly_inference()
            except Exception as e:
                logger.error("Inference error: %s", e)

            # Wait 1 hour
            await asyncio.sleep(3600)


if __name__ == "__main__":
    service = InferenceService()
    asyncio.run(service.run_forever())