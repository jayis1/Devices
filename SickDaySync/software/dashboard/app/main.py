from __future__ import annotations

from fastapi import FastAPI, HTTPException

from .ml_inference import recommend, summarize_risk
from .models import EventStore, RecommendationResponse, TelemetryEvent

app = FastAPI(title="SickDaySync Dashboard", version="0.1.0")
store = EventStore()


@app.get("/health")
def health() -> dict:
    return {"status": "ok", "service": "sickdaysync-dashboard"}


@app.post("/telemetry")
def ingest(event: TelemetryEvent) -> dict:
    saved = store.add(event)
    return {"accepted": True, "patient_id": saved.patient_id, "node": saved.node}


@app.get("/risk/summary")
def risk_summary(patient_id: str) -> dict:
    events = store.list_for_patient(patient_id)
    if not events:
        raise HTTPException(status_code=404, detail="patient not found")
    result = summarize_risk(events)
    result["patient_id"] = patient_id
    return result


@app.get("/patients/{patient_id}/timeline")
def timeline(patient_id: str) -> list[dict]:
    events = store.list_for_patient(patient_id)
    if not events:
        raise HTTPException(status_code=404, detail="patient not found")
    return [event.model_dump(mode="json") for event in events]


@app.get("/recommendations", response_model=RecommendationResponse)
def recommendations(patient_id: str) -> RecommendationResponse:
    events = store.list_for_patient(patient_id)
    if not events:
        raise HTTPException(status_code=404, detail="patient not found")
    return RecommendationResponse(**recommend(patient_id, events))
