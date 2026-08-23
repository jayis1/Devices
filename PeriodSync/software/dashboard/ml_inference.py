from __future__ import annotations

from statistics import mean


def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def dashboard_risk(latest_metrics: dict, symptom_events: list[dict]) -> dict:
    skin_temp = float(latest_metrics.get("skin_temp_c", 36.5))
    hrv = float(latest_metrics.get("hrv_rmssd", 40.0))
    leak_score = float(latest_metrics.get("leak_score", 20.0))
    resting_hr = float(latest_metrics.get("resting_hr", 65.0))
    pain_scores = [float(evt.get("pain_score", 0.0)) for evt in symptom_events[-7:]] or [0.0]

    phase_conf = clamp(0.55 + (skin_temp - 36.4) * 0.18 + (42.0 - hrv) * 0.006, 0.05, 0.99)
    cramp_score = clamp(mean(pain_scores) * 8.5 + max(0.0, 42.0 - hrv) * 0.7, 0.0, 100.0)
    iron_watch = clamp((resting_hr - 58.0) * 2.2 + mean(pain_scores) * 4.5 + leak_score * 0.35, 0.0, 100.0)
    onset_7d = clamp(phase_conf * 100.0, 0.0, 100.0)

    return {
        "phase_confidence": round(phase_conf, 3),
        "period_onset_7d": round(onset_7d, 1),
        "leak_risk_60m": round(clamp(leak_score, 0.0, 100.0), 1),
        "cramp_score_120m": round(cramp_score, 1),
        "iron_watch": round(iron_watch, 1),
    }
