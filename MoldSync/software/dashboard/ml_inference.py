from __future__ import annotations

from dataclasses import dataclass


@dataclass
class RoomAssessment:
    room_id: str
    condensation_margin_c: float
    risk_level: str
    recommendation: str


def assess_room(room_id: str, air_temp_c: float, rh: float, surface_temp_c: float) -> RoomAssessment:
    dew_point = air_temp_c - ((100.0 - rh) / 5.0)
    margin = round(surface_temp_c - dew_point, 2)
    if margin < 0.5:
        return RoomAssessment(room_id, margin, "critical", "Run exhaust fan and inspect cold surfaces")
    if margin < 2.0:
        return RoomAssessment(room_id, margin, "elevated", "Extend dry-out and reduce RH target")
    return RoomAssessment(room_id, margin, "normal", "No action required")


def leak_risk(flow_ml_min: float, leak_signal: float, cold_pipe_c: float, room_temp_c: float) -> dict:
    score = 0.2
    score += min(flow_ml_min / 250.0, 0.35)
    score += min(leak_signal, 1.0) * 0.35
    score += 0.1 if cold_pipe_c + 2.5 < room_temp_c else 0.0
    level = "critical" if score >= 0.8 else "elevated" if score >= 0.55 else "normal"
    return {"score": round(min(score, 1.0), 3), "level": level}


def inspect_patch(thermal_delta_c: float, conductivity_score: float, spectral_mildew_index: float) -> dict:
    confidence = min(1.0, max(0.0, 0.4 + (-thermal_delta_c * 0.07) + conductivity_score * 0.003 + spectral_mildew_index * 0.2))
    status = "damp-likely" if confidence >= 0.7 else "watch" if confidence >= 0.45 else "clear"
    return {"confidence": round(confidence, 3), "status": status}
