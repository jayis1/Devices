from __future__ import annotations

from typing import Iterable, Dict, Any

from .models import TelemetryEvent


def summarize_risk(events: Iterable[TelemetryEvent]) -> Dict[str, Any]:
    rows = list(events)
    if not rows:
        return {
            "risk_level": "green",
            "score": 0.0,
            "spread_score": 0.0,
            "hydration_risk": 0.0,
            "fever_forecast": 36.8,
        }

    latest = rows[-1]
    spread_score = min(100.0, (latest.co2_ppm / 20.0) + (latest.coughs_per_hour * 1.8) + max(0.0, 45.0 - latest.humidity_pct))
    hydration_risk = min(100.0, max(0.0, (latest.fever_c - 37.2) * 18.0 + (85.0 - latest.spo2) * 1.3 + max(0, 600 - latest.hydration_ml) / 8.0))
    fever_forecast = latest.fever_c + 0.25 + (latest.resting_hr - 70.0) * 0.01
    score = min(100.0, (spread_score * 0.4) + (hydration_risk * 0.35) + max(0.0, latest.fever_c - 37.0) * 25.0)

    if score >= 70.0 or latest.spo2 < 92.0:
        risk = "red"
    elif score >= 35.0 or latest.fever_c >= 38.0:
        risk = "amber"
    else:
        risk = "green"

    return {
        "risk_level": risk,
        "score": round(score, 2),
        "spread_score": round(spread_score, 2),
        "hydration_risk": round(hydration_risk, 2),
        "fever_forecast": round(fever_forecast, 2),
    }


def recommend(patient_id: str, events: Iterable[TelemetryEvent]) -> Dict[str, Any]:
    summary = summarize_risk(events)
    actions = []
    if summary["spread_score"] > 55:
        actions.append("Increase fresh-air exchange or HEPA mode in the assigned sick room.")
    if summary["hydration_risk"] > 45:
        actions.append("Offer fluids now and recheck intake within 30 minutes.")
    if summary["fever_forecast"] >= 38.5:
        actions.append("Prepare a temperature recheck and review the antipyretic timing window.")
    if not actions:
        actions.append("Continue quiet monitoring and maintain current comfort settings.")

    summary_text = (
        f"{patient_id} is {summary['risk_level']} risk with spread score {summary['spread_score']} "
        f"and projected temperature {summary['fever_forecast']} C."
    )
    return {
        "patient_id": patient_id,
        "risk_level": summary["risk_level"],
        "summary": summary_text,
        "actions": actions,
    }
