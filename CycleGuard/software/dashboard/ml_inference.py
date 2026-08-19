"""
CycleGuard ML Inference Service
Loads trained models and provides inference functions for the FastAPI backend.
"""

import numpy as np
from typing import List, Dict, Optional

# In production: import onnxruntime / tensorflow / xgboost
# import onnxruntime as ort
# import xgboost as xgb

MODEL_DIR = "/models"
CRASH_NET_PATH = f"{MODEL_DIR}/crash_net.tflite"
BLIND_SPOT_PATH = f"{MODEL_DIR}/blind_spot_net.onnx"
COLLISION_PREDICT_PATH = f"{MODEL_DIR}/collision_predict.onnx"
THEFT_PATTERN_PATH = f"{MODEL_DIR}/theft_pattern.onnx"
ROUTE_SAFETY_PATH = f"{MODEL_DIR}/route_safety.onnx"
CRASH_RISK_PATH = f"{MODEL_DIR}/crash_risk_forecast.json"

_sessions = {}


def _get_session(path: str):
    if path not in _sessions:
        # _sessions[path] = ort.InferenceSession(path)
        _sessions[path] = None  # placeholder
    return _sessions[path]


# ---- Theft pattern classification ----
async def predict_theft_pattern(imu_history: List[Dict],
                                 load_cell: float) -> Dict:
    """
    Classify lock activity: normal, accidental bump, tamper, theft.
    Uses TheftPattern LSTM in production.
    """
    if not imu_history:
        return {"class": "normal", "confidence": 0.9}

    # Feature extraction
    accel_values = [s.get("accel_mag", 0) for s in imu_history]
    max_accel = max(accel_values) if accel_values else 0
    duration = len(imu_history)

    # Heuristic classification (production: LSTM model)
    if max_accel > 5.0 and load_cell > 30:
        return {"class": "theft_in_progress", "confidence": 0.92}
    elif max_accel > 3.0 and load_cell > 15:
        return {"class": "tamper_attempt", "confidence": 0.85}
    elif max_accel > 1.0:
        return {"class": "accidental_bump", "confidence": 0.78}
    else:
        return {"class": "normal", "confidence": 0.95}


# ---- Route safety scoring ----
async def get_route_safety(start_lat: float, start_lon: float,
                            end_lat: float, end_lon: float) -> Dict:
    """
    Recommend safe route from A to B using RouteSafety GCN.
    Returns route segments with per-segment safety scores.
    """
    # In production: query OSM/Mapbox routing + GCN safety scoring
    # Simplified: return mock route with safety scores
    segments = [
        {"from": [start_lat, start_lon], "to": [start_lat + 0.01, start_lon],
         "safety_score": 85, "road_type": "bike_lane"},
        {"from": [start_lat + 0.01, start_lon],
         "to": [start_lat + 0.01, start_lon + 0.01],
         "safety_score": 72, "road_type": "residential"},
        {"from": [start_lat + 0.01, start_lon + 0.01],
         "to": [end_lat, end_lon],
         "safety_score": 91, "road_type": "greenway"},
    ]
    avg_safety = sum(s["safety_score"] for s in segments) / len(segments)
    return {
        "segments": segments,
        "avg_safety_score": avg_safety,
        "total_distance_km": 5.2,
        "estimated_time_min": 18,
        "safer_than_shortest": True,
    }


# ---- Crash risk forecast (48-hour) ----
async def get_crash_risk_forecast() -> List[Dict]:
    """
    48-hour crash risk forecast based on weather + historical data.
    Uses CrashRiskForecast (XGBoost) in production.
    """
    import datetime as dt
    now = dt.datetime.utcnow()
    forecast = []
    # Simplified: generate 3-hour windows for next 48 hours
    for i in range(16):
        hour = now + dt.timedelta(hours=i * 3)
        # Heuristic: rush hours (7-9, 17-19) higher risk, rain increases risk
        h = hour.hour
        base_risk = 20
        if 7 <= h <= 9 or 17 <= h <= 19:
            base_risk += 25
        if h >= 22 or h <= 5:
            base_risk += 10  # night riding risk
        # Weather would come from API (simplified)
        weather_risk = np.random.randint(0, 20)
        total = min(base_risk + weather_risk, 100)
        forecast.append({
            "time": hour.isoformat(),
            "risk_score": total,
            "risk_level": "high" if total > 60 else "moderate" if total > 30 else "low",
            "factors": ["rush_hour"] if 7 <= h <= 9 or 17 <= h <= 19 else [],
        })
    return forecast


# ---- Ride safety score ----
async def get_ride_safety_score(ride_data: List[Dict]) -> float:
    """Compute safety score for a completed ride (0-100)"""
    if not ride_data:
        return 100.0

    speeds = [r.get("speed_kmh", 0) for r in ride_data]
    avg_speed = np.mean(speeds)
    max_speed = max(speeds)

    # Penalize high speeds
    score = 100.0
    if max_speed > 40:
        score -= (max_speed - 40) * 1.5
    if avg_speed > 30:
        score -= (avg_speed - 30) * 0.5

    # Penalize hard braking events (would come from helmet data)
    # Penalize proximity warnings (would come from blindspot data)

    return float(max(0, min(100, score)))