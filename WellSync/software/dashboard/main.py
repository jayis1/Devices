"""WellSync FastAPI backend."""

from __future__ import annotations

from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import (
    contamination_risk,
    derive_state,
    dry_well_risk,
    explain_risks,
    pump_failure_risk,
    treatment_integrity_risk,
)
from models import OverviewResponse, RiskScores, ServiceLogEntry, TelemetryEvent

app = FastAPI(title="WellSync API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

_STORE: dict[str, Any] = {
    "latest": {},
    "service_log": [],
    "events": [],
}
_WEBSOCKETS: set[WebSocket] = set()


async def _broadcast(payload: dict[str, Any]) -> None:
    stale: list[WebSocket] = []
    for ws in list(_WEBSOCKETS):
        try:
            await ws.send_json(payload)
        except RuntimeError:
            stale.append(ws)
    for ws in stale:
        _WEBSOCKETS.discard(ws)


@app.get("/api/v1/health")
async def health() -> dict[str, str]:
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat()}


@app.post("/api/v1/telemetry")
async def ingest_telemetry(event: TelemetryEvent) -> dict[str, Any]:
    _STORE["latest"][event.kind] = event.model_dump(mode="json")
    _STORE["events"].append(event.model_dump(mode="json"))
    state, advisories = derive_state(_STORE["latest"])
    payload = {"type": "telemetry", "state": state, "advisories": advisories, "event": event.model_dump(mode="json")}
    await _broadcast(payload)
    return payload


@app.post("/api/v1/service-log")
async def add_service_log(entry: ServiceLogEntry) -> dict[str, Any]:
    item = entry.model_dump(mode="json")
    _STORE["service_log"].append(item)
    await _broadcast({"type": "service_log", "entry": item})
    return {"accepted": True, "count": len(_STORE["service_log"])}


@app.get("/api/v1/overview", response_model=OverviewResponse)
async def overview() -> OverviewResponse:
    latest = _STORE["latest"]
    state, advisories = derive_state(latest)
    risks = RiskScores(
        contamination=contamination_risk(latest),
        pump_failure=pump_failure_risk(latest),
        dry_well=dry_well_risk(latest),
        treatment_integrity=treatment_integrity_risk(latest),
    )
    return OverviewResponse(
        state=state,
        risks=risks,
        latest=latest,
        advisories=advisories,
        service_log_count=len(_STORE["service_log"]),
    )


@app.get("/api/v1/risk/explain")
async def risk_explain() -> dict[str, str]:
    return explain_risks(_STORE["latest"])


@app.post("/api/v1/reset")
async def reset_demo_store() -> dict[str, bool]:
    _STORE["latest"].clear()
    _STORE["service_log"].clear()
    _STORE["events"].clear()
    return {"reset": True}


@app.websocket("/ws/live")
async def ws_live(websocket: WebSocket) -> None:
    await websocket.accept()
    _WEBSOCKETS.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        _WEBSOCKETS.discard(websocket)
