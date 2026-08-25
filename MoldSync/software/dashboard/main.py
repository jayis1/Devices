from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import FastAPI

from ml_inference import assess_room, inspect_patch, leak_risk
from models import InspectionIn, PlumbingTelemetryIn, RoomTelemetryIn

STATE = {
    "rooms": {},
    "alerts": [],
    "plumbing": {},
}


@asynccontextmanager
async def lifespan(app: FastAPI):
    yield


app = FastAPI(title="MoldSync Dashboard", version="0.1.0", lifespan=lifespan)


@app.get('/health')
def health() -> dict:
    return {"status": "ok", "service": "moldsync-dashboard"}


@app.get('/rooms')
def get_rooms() -> dict:
    return STATE["rooms"]


@app.post('/telemetry/room')
def ingest_room(payload: RoomTelemetryIn) -> dict:
    assessment = assess_room(payload.room_id, payload.air_temp_c, payload.rh, payload.surface_temp_c)
    record = payload.model_dump() | {
        "condensation_margin_c": assessment.condensation_margin_c,
        "risk_level": assessment.risk_level,
        "recommendation": assessment.recommendation,
    }
    STATE["rooms"][payload.room_id] = record
    if assessment.risk_level != "normal":
        STATE["alerts"].append({"type": "room", "room_id": payload.room_id, "risk": assessment.risk_level})
    return record


@app.post('/telemetry/plumbing')
def ingest_plumbing(payload: PlumbingTelemetryIn) -> dict:
    assessment = leak_risk(payload.flow_ml_min, payload.leak_signal, payload.cold_pipe_c, payload.room_temp_c)
    record = payload.model_dump() | assessment
    STATE["plumbing"][payload.branch_id] = record
    if assessment["level"] != "normal":
        STATE["alerts"].append({"type": "plumbing", "branch_id": payload.branch_id, "risk": assessment["level"]})
    return record


@app.post('/inspect')
def inspect(payload: InspectionIn) -> dict:
    result = inspect_patch(payload.thermal_delta_c, payload.conductivity_score, payload.spectral_mildew_index)
    if result["status"] != "clear":
        STATE["alerts"].append({"type": "inspection", "room_id": payload.room_id, "status": result["status"]})
    return {"room_id": payload.room_id} | result


@app.get('/alerts/active')
def alerts() -> list[dict]:
    return STATE["alerts"]


@app.get('/summary')
def summary() -> dict:
    elevated_rooms = len([r for r in STATE["rooms"].values() if r["risk_level"] != "normal"])
    active_alerts = len(STATE["alerts"])
    return {
        "mold_risk_score": max(0, 100 - elevated_rooms * 18 - active_alerts * 6),
        "drying_efficiency_score": max(0, 100 - elevated_rooms * 12),
        "hidden_leak_suspicion_score": min(100, len([p for p in STATE["plumbing"].values() if p["level"] != "normal"]) * 35),
        "active_alerts": active_alerts,
    }
