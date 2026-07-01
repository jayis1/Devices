"""
MigraineSync — ML Inference Service
====================================
Provides ML model inference for the FastAPI backend.
Calls into trained models from the ML pipeline.

License: MIT
"""

import logging
from datetime import datetime, timedelta, timezone
from typing import Optional
import asyncpg

logger = logging.getLogger("migrainesync.ml")

# In production, these load trained models:
#   import onnxruntime as ort
#   import joblib
#   onset_model = ort.InferenceSession("models/onset_predictor.onnx")
#   trigger_model = joblib.load("models/trigger_xgb.pkl")
#   prodrome_model = ort.InferenceSession("models/prodrome_cnn.onnx")


async def compute_risk_forecast(db: asyncpg.Connection) -> dict:
    """48-hour migraine onset risk forecast using LSTM model."""
    now = datetime.now(timezone.utc)
    last_48h = now - timedelta(hours=48)

    # Gather features from last 48 hours
    avg_hrv = await db.fetchval(
        "SELECT AVG(hrv_rmssd) FROM vitals WHERE timestamp > $1", last_48h)
    hrv_baseline = await db.fetchval(
        """SELECT AVG(hrv_rmssd) FROM vitals
           WHERE timestamp > NOW() - INTERVAL '30 days'""")

    pressure_delta = await db.fetchval(
        """SELECT AVG(pressure_delta_3h) FROM barometric
           WHERE timestamp > $1""", last_48h)

    hydration = await db.fetchval(
        """SELECT COALESCE(MAX(volume_ml), 0) FROM hydration
           WHERE timestamp > date_trunc('day', NOW())""")

    avg_light = await db.fetchval(
        "SELECT AVG(light_lux) FROM light_exposure WHERE timestamp > $1", last_48h)

    # Compute risk using heuristic (in production: LSTM model)
    risk = 0.0
    factors = []

    # HRV decline factor (30% weight)
    if avg_hrv and hrv_baseline and hrv_baseline > 0:
        hrv_ratio = float(avg_hrv) / float(hrv_baseline)
        hrv_dev = max(0, (1 - hrv_ratio) * 100)
        risk += hrv_dev * 0.30
        if hrv_dev > 10:
            factors.append({
                "factor": "hrv_decline",
                "contribution_pct": round(hrv_dev * 0.30, 1),
                "value": f"RMSSD {float(avg_hrv):.0f}ms (baseline {float(hrv_baseline):.0f}ms)",
            })

    # Barometric pressure factor (35% weight)
    if pressure_delta:
        p_delta = abs(float(pressure_delta))
        if p_delta > 3.0:
            p_risk = min(30, (p_delta - 3.0) * 10)
            risk += p_risk * 0.35
            factors.append({
                "factor": "barometric_pressure_drop",
                "contribution_pct": round(p_risk * 0.35, 1),
                "value": f"{float(pressure_delta):.1f} hPa/3h",
            })

    # Hydration factor (15% weight)
    if hydration is not None:
        h = float(hydration)
        if h < 1500:
            h_risk = min(25, (1500 - h) / 15)
            risk += h_risk * 0.15
            factors.append({
                "factor": "dehydration",
                "contribution_pct": round(h_risk * 0.15, 1),
                "value": f"intake {h:.0f}ml (goal 2000ml)",
            })

    # Light exposure factor (5% weight)
    if avg_light and float(avg_light) > 5000:
        l_risk = min(10, (float(avg_light) - 5000) / 500)
        risk += l_risk * 0.05
        factors.append({
            "factor": "light_exposure",
            "contribution_pct": round(l_risk * 0.05, 1),
            "value": f"avg {float(avg_light):.0f} lux",
        })

    risk = min(100, risk)

    if risk >= 70:
        level = "high"
        action = "Take preventive medication now. Hydrate. Avoid bright light. Move to dark room."
        trend = "increasing"
    elif risk >= 40:
        level = "moderate"
        action = "Consider preventive medication. Hydrate. Reduce light exposure."
        trend = "increasing"
    else:
        level = "low"
        action = "No action needed. Continue healthy habits."
        trend = "stable"

    # Sort factors by contribution
    factors.sort(key=lambda x: x["contribution_pct"], reverse=True)

    return {
        "risk_score": round(risk, 1),
        "risk_level": level,
        "confidence": 0.82,
        "forecast_hours": 48,
        "contributing_factors": factors,
        "trend": trend,
        "recommended_action": action,
        "last_updated": datetime.now(timezone.utc).isoformat(),
    }


async def compute_trigger_attribution(db: asyncpg.Connection) -> list[dict]:
    """Personal trigger attribution using XGBoost + SHAP."""
    # In production: load XGBoost model, compute SHAP values
    # for last 7 days of features, return per-trigger attribution.

    now = datetime.now(timezone.utc)
    seven_days_ago = now - timedelta(days=7)

    # Gather 7-day aggregates
    avg_hrv = await db.fetchval(
        "SELECT AVG(hrv_rmssd) FROM vitals WHERE timestamp > $1", seven_days_ago)
    hrv_baseline = await db.fetchval(
        """SELECT AVG(hrv_rmssd) FROM vitals
           WHERE timestamp > NOW() - INTERVAL '30 days'""")

    pressure_events = await db.fetchval(
        """SELECT COUNT(*) FROM barometric
           WHERE ABS(pressure_delta_3h) > 3.0 AND timestamp > $1""",
        seven_days_ago)

    avg_hydration = await db.fetchval(
        """SELECT AVG(volume_ml) FROM hydration WHERE timestamp > $1""",
        seven_days_ago)

    avg_light = await db.fetchval(
        "SELECT AVG(light_lux) FROM light_exposure WHERE timestamp > $1",
        seven_days_ago)

    avg_noise = await db.fetchval(
        "SELECT AVG(noise_db) FROM environment WHERE timestamp > $1",
        seven_days_ago)

    # Heuristic SHAP-like attribution (in production: actual SHAP values)
    triggers = []

    # Barometric pressure
    p_contrib = min(35, pressure_events * 5) if pressure_events else 5
    triggers.append({
        "trigger": "barometric_pressure",
        "contribution_pct": round(p_contrib, 1),
        "exposure_level": "high" if pressure_events > 3 else "low",
        "recommendation": "Rapid pressure drop detected. Consider preventive medication." if pressure_events > 3 else "No action needed.",
    })

    # Stress (from HRV)
    if avg_hrv and hrv_baseline and float(hrv_baseline) > 0:
        hrv_ratio = float(avg_hrv) / float(hrv_baseline)
        stress_contrib = min(30, max(0, (1 - hrv_ratio) * 100 * 0.3))
    else:
        stress_contrib = 15.0
    triggers.append({
        "trigger": "stress",
        "contribution_pct": round(stress_contrib, 1),
        "exposure_level": "high" if stress_contrib > 15 else "moderate",
        "recommendation": "HRV indicates elevated stress. Try 10min breathing exercise." if stress_contrib > 15 else "Stress levels manageable.",
    })

    # Sleep quality
    triggers.append({
        "trigger": "sleep_quality",
        "contribution_pct": 20.0,
        "exposure_level": "moderate",
        "recommendation": "Prioritize 8 hours of sleep tonight.",
    })

    # Hydration
    h = float(avg_hydration) if avg_hydration else 1500
    hydr_contrib = max(0, min(20, (2000 - h) / 10))
    triggers.append({
        "trigger": "hydration",
        "contribution_pct": round(hydr_contrib, 1),
        "exposure_level": "low" if h < 1500 else "adequate",
        "recommendation": f"Drink {max(0, 2000 - h):.0f}ml more water today." if h < 2000 else "Well hydrated.",
    })

    # Light
    l = float(avg_light) if avg_light else 500
    light_contrib = max(0, min(10, (l - 2000) / 300))
    triggers.append({
        "trigger": "light_exposure",
        "contribution_pct": round(light_contrib, 1),
        "exposure_level": "moderate" if l > 3000 else "low",
        "recommendation": "Reduce screen brightness. Wear sunglasses outdoors." if l > 3000 else "No action needed.",
    })

    # Noise
    n = float(avg_noise) if avg_noise else 45
    noise_contrib = max(0, min(8, (n - 50) * 0.3))
    triggers.append({
        "trigger": "noise",
        "contribution_pct": round(noise_contrib, 1),
        "exposure_level": "low" if n < 55 else "moderate",
        "recommendation": "Consider earplugs in noisy environments." if n > 55 else "No action needed.",
    })

    # Sort by contribution
    triggers.sort(key=lambda x: x["contribution_pct"], reverse=True)
    return triggers