"""CleanSync FastAPI backend."""

from __future__ import annotations

import asyncio
import json
from datetime import datetime, timezone
from typing import Any

try:
    import aiomqtt  # type: ignore
except Exception:  # pragma: no cover
    aiomqtt = None

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import active_alerts, cleanliness_score, optimize_schedule, recommendations, supply_forecast
from models import DockState, DirtTelemetry, OverviewResponse, Recommendation, ScheduleRequest, ScheduleResponse, WandScan

app = FastAPI(title="CleanSync API", version="1.0.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

STORE: dict[str, Any] = {
    "dirt": [],
    "dock": None,
    "scans": [],
}
WEBSOCKETS: set[WebSocket] = set()

async def broadcast(event: dict) -> None:
    stale: list[WebSocket] = []
    for ws in list(WEBSOCKETS):
        try:
            await ws.send_json(event)
        except RuntimeError:
            stale.append(ws)
    for ws in stale:
        WEBSOCKETS.discard(ws)

async def mqtt_loop() -> None:
    if aiomqtt is None:  # pragma: no cover
        return
    try:  # pragma: no cover
        async with aiomqtt.Client("broker.cleansync.local", port=1883) as client:
            await client.subscribe("cleansync/#")
            async for message in client.messages:
                payload = json.loads(message.payload.decode())
                topic = str(message.topic)
                if "/dirt/" in topic:
                    await ingest_dirt(DirtTelemetry(**payload))
                elif "/dock/" in topic:
                    await ingest_dock(DockState(**payload))
                elif "/wand/" in topic:
                    await ingest_scan(WandScan(**payload))
    except Exception as exc:
        print(f"[mqtt] loop ended: {exc}")

@app.on_event("startup")
async def startup() -> None:
    if aiomqtt is not None:
        asyncio.create_task(mqtt_loop())

@app.get("/api/v1/health")
async def health() -> dict:
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat()}

async def ingest_dirt(payload: DirtTelemetry) -> dict:
    existing = [row for row in STORE["dirt"] if row["node_id"] != payload.node_id]
    existing.append(payload.model_dump())
    STORE["dirt"] = existing
    event = {"type": "dirt_update", "payload": payload.model_dump()}
    await broadcast(event)
    return event

async def ingest_dock(payload: DockState) -> dict:
    STORE["dock"] = payload.model_dump()
    event = {"type": "dock_update", "payload": STORE["dock"]}
    await broadcast(event)
    return event

async def ingest_scan(payload: WandScan) -> dict:
    STORE["scans"].append(payload.model_dump())
    STORE["scans"] = STORE["scans"][-100:]
    event = {"type": "scan_update", "payload": payload.model_dump()}
    await broadcast(event)
    return event

@app.post("/api/v1/telemetry/dirt")
async def post_dirt(payload: DirtTelemetry) -> dict:
    return await ingest_dirt(payload)

@app.post("/api/v1/dock/state")
async def post_dock(payload: DockState) -> dict:
    return await ingest_dock(payload)

@app.post("/api/v1/wand/scan")
async def post_scan(payload: WandScan) -> dict:
    return await ingest_scan(payload)

@app.get("/api/v1/overview", response_model=OverviewResponse)
async def overview() -> OverviewResponse:
    rooms = STORE["dirt"]
    scans = STORE["scans"]
    dock = STORE["dock"]
    return OverviewResponse(
        cleanliness_score=cleanliness_score(rooms, scans),
        active_alerts=active_alerts(rooms, dock),
        rooms=rooms,
        supply_forecast=supply_forecast(dock),
        recommendations=[Recommendation(**rec) for rec in recommendations(rooms, scans, dock)],
    )

@app.get("/api/v1/recommendations", response_model=list[Recommendation])
async def get_recommendations() -> list[Recommendation]:
    return [Recommendation(**rec) for rec in recommendations(STORE["dirt"], STORE["scans"], STORE["dock"])]

@app.post("/api/v1/schedule/optimize", response_model=ScheduleResponse)
async def schedule_optimize(request: ScheduleRequest) -> ScheduleResponse:
    window, reason, rooms = optimize_schedule(STORE["dirt"], request.available_windows)
    return ScheduleResponse(recommended_window=window, reason=reason, rooms=rooms)

@app.websocket("/ws/live")
async def ws_live(websocket: WebSocket) -> None:
    await websocket.accept()
    WEBSOCKETS.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        WEBSOCKETS.discard(websocket)
