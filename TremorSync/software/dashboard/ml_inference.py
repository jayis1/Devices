"""
TremorSync ML Inference Service
Loads trained models and provides inference functions for the FastAPI backend.
"""

import numpy as np
from typing import List, Dict, Optional

# In production: import onnxruntime / tensorflow
# import onnxruntime as ort

# ---- Model paths ----
MODEL_DIR = "/models"
TREMOR_NET_PATH = f"{MODEL_DIR}/tremor_net.tflite"
FREEZE_NET_PATH = f"{MODEL_DIR}/freeze_net.onnx"
MED_RESPONSE_PATH = f"{MODEL_DIR}/med_response.json"
FALL_RISK_PATH = f"{MODEL_DIR}/fall_risk.onnx"
PROGRESSION_PATH = f"{MODEL_DIR}/progression_net.onnx"

# Lazy-loaded model sessions
_sessions = {}

def _get_session(path: str):
    if path not in _sessions:
        # _sessions[path] = ort.InferenceSession(path)
        _sessions[path] = None  # placeholder
    return _sessions[path]


# ---- Fall risk prediction (30-day) ----
async def predict_fall_risk(gait_history: List[Dict]) -> float:
    """
    Input: 14-day gait metrics (stride variability, freeze frequency, etc.)
    Output: 30-day fall risk score (0-100)
    """
    if not gait_history:
        return 0.0

    # Feature extraction from gait history
    stride_values = [g["stride_length"] for g in gait_history if g.get("stride_length")]
    fi_values = [g["freeze_index"] for g in gait_history if g.get("freeze_index")]
    fog_count = sum(1 for g in gait_history if g.get("fog_detected"))
    fest_count = sum(1 for g in gait_history if g.get("festination"))

    if not stride_values:
        return 0.0

    stride_cv = np.std(stride_values) / (np.mean(stride_values) + 1e-6)
    avg_fi = np.mean(fi_values) if fi_values else 0
    fog_rate = fog_count / max(len(gait_history), 1) * 100
    fest_rate = fest_count / max(len(gait_history), 1) * 100

    # Heuristic risk score (in production: LSTM model)
    risk = (stride_cv * 30 + avg_fi * 40 + fog_rate * 2 + fest_rate * 2)
    return float(min(risk, 100.0))


# ---- Disease progression (MDS-UPDRS Part III estimate) ----
async def predict_disease_progression(tremor_trend: List[Dict],
                                       gait_trend: List[Dict]) -> float:
    """
    Input: 90-day tremor + gait trends
    Output: MDS-UPDRS Part III (motor) score estimate (0-132)
    """
    if not tremor_trend:
        return 0.0

    avg_tremor = np.mean([t["avg_tremor"] for t in tremor_trend if t.get("avg_tremor")])
    avg_brady = np.mean([t["avg_brady"] for t in tremor_trend if t.get("avg_brady")])

    # Map to MDS-UPDRS Part III (0-132 scale)
    # Tremor amplitude ~0-2 m/s² → UPDRS tremor items (0-4 per item, 5 items = 0-20)
    tremor_score = min(avg_tremor * 10, 20)
    # Bradykinesia 0-100 → UPDRS bradykinesia items (0-4 per item, ~10 items = 0-40)
    brady_score = min(avg_brady / 100 * 40, 40)
    # Gait decline → UPDRS gait items (0-4, 3 items = 0-12)
    gait_score = 0
    if gait_trend:
        stride_decline = 1.0 - (gait_trend[-1].get("avg_stride", 1.0) /
                                max(gait_trend[0].get("avg_stride", 1.0), 0.01))
        gait_score = min(stride_decline * 12, 12)

    total = tremor_score + brady_score + gait_score
    return float(min(total, 132.0))


# ---- OFF state prediction ----
async def predict_off_state(current_onoff: int,
                             minutes_since_dose: int) -> int:
    """
    Predict minutes until OFF state onset.
    Uses MedResponse XGBoost model in production.
    """
    if current_onoff == 0:  # already OFF
        return 0
    # Typical levodopa duration: 3-4 hours (180-240 min)
    # If approaching that window, predict OFF soon
    remaining = max(0, 210 - minutes_since_dose)  # 3.5h avg
    if current_onoff == 2:  # transition
        remaining = min(remaining, 15)
    return int(remaining)


# ---- Tremor summary ----
async def get_tremor_summary(conn) -> Dict:
    """Get 7-day tremor summary for clinical report"""
    rows = await conn.fetch(
        """SELECT tremor_class, AVG(tremor_amplitude) as avg_amp,
                  MAX(tremor_amplitude) as max_amp,
                  AVG(bradykinesia) as avg_brady
           FROM tremor_data
           WHERE timestamp > NOW() - INTERVAL '7 days'
           GROUP BY tremor_class ORDER BY tremor_class""")
    classes = {0: "None", 1: "Resting", 2: "Postural", 3: "Action"}
    return {
        "by_class": [{**dict(r), "class_name": classes.get(r["tremor_class"], "?")}
                     for r in rows],
        "resting_tremor_amplitude_avg": next(
            (r["avg_amp"] for r in rows if r["tremor_class"] == 1), 0),
        "bradykinesia_avg": next(
            (r["avg_brady"] for r in rows if r["tremor_class"] is not None), 0)
    }