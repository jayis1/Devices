"""UroSync FastAPI backend."""

from __future__ import annotations

import asyncio
import json
import os
from datetime import datetime, timezone
from typing import Optional

import aiomqtt
import asyncpg
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import fall_risk, hydration_risk, next_best_nudge, nocturia_forecast, uti_risk
from models import AlertEvent, BottleTelemetry, EnvironmentTelemetry, MatTelemetry, RiskOverview, TrendRequest, TrendResponse, VoidTelemetry

app = FastAPI(title="UroSync API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.urosync.local")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://urosync:***@localhost/urosync")

_db_pool: Optional[asyncpg.Pool] = None
_websockets: set[WebSocket] = set()
_recent_voids: list[dict] = []
_recent_mat: list[dict] = []
_recent_bottle: list[dict] = []
_recent_env: list[dict] = []


async def broadcast(event: dict) -> None:
    stale: list[WebSocket] = []
    for ws in list(_websockets):
        try:
            await ws.send_json(event)
        except RuntimeError:
            stale.append(ws)
    for ws in stale:
        _websockets.discard(ws)


@app.on_event("startup")
async def startup() -> None:
    global _db_pool
    try:
        _db_pool = await asyncpg.create_pool(DATABASE_URL, min_size=1, max_size=4)
        async with _db_pool.acquire() as conn:
            await conn.execute(
                """
                CREATE TABLE IF NOT EXISTS void_events (
                    id SERIAL PRIMARY KEY,
                    payload JSONB NOT NULL,
                    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                );
                CREATE TABLE IF NOT EXISTS mat_events (
                    id SERIAL PRIMARY KEY,
                    payload JSONB NOT NULL,
                    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                );
                CREATE TABLE IF NOT EXISTS bottle_events (
                    id SERIAL PRIMARY KEY,
                    payload JSONB NOT NULL,
                    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                );
                CREATE TABLE IF NOT EXISTS env_events (
                    id SERIAL PRIMARY KEY,
                    payload JSONB NOT NULL,
                    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                );
                """
            )
    except Exception as exc:  # pragma: no cover
        print(f"[db] startup degraded: {exc}")
        _db_pool = None
    asyncio.create_task(mqtt_loop())


@app.on_event("shutdown")
async def shutdown() -> None:
    if _db_pool is not None:
        await _db_pool.close()


async def persist(table: str, payload: dict) -> None:
    if _db_pool is None:
        return
    async with _db_pool.acquire() as conn:
        await conn.execute(f"INSERT INTO {table} (payload) VALUES ($1)", json.dumps(payload))


async def mqtt_loop() -> None:
    try:
        async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
            await client.subscribe("urosync/#")
            async for message in client.messages:
                topic = str(message.topic)
                try:
                    payload = json.loads(message.payload.decode())
                except (UnicodeDecodeError, json.JSONDecodeError):
                    continue
                await route_mqtt(topic, payload)
    except Exception as exc:  # pragma: no cover
        print(f"[mqtt] loop ended: {exc}")


async def route_mqtt(topic: str, payload: dict) -> None:
    if topic == "urosync/void":
        model = VoidTelemetry(**payload)
        data = model.model_dump(mode="json")
        _recent_voids.append(data)
        del _recent_voids[:-30]
        await persist("void_events", data)
        await broadcast({"type": "void", "payload": data})
    elif topic == "urosync/mat":
        model = MatTelemetry(**payload)
        data = model.model_dump(mode="json")
        _recent_mat.append(data)
        del _recent_mat[:-30]
        await persist("mat_events", data)
        await broadcast({"type": "mat", "payload": data})
    elif topic == "urosync/bottle":
        model = BottleTelemetry(**payload)
        data = model.model_dump(mode="json")
        _recent_bottle.append(data)
        del _recent_bottle[:-30]
        await persist("bottle_events", data)
        await broadcast({"type": "bottle", "payload": data})
    elif topic == "urosync/env":
        model = EnvironmentTelemetry(**payload)
        data = model.model_dump(mode="json")
        _recent_env.append(data)
        del _recent_env[:-30]
        await persist("env_events", data)
        await broadcast({"type": "env", "payload": data})


@app.get("/api/v1/health")
async def health() -> dict:
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat(), "cached_events": len(_recent_voids)}


@app.get("/api/v1/overview", response_model=RiskOverview)
async def overview() -> RiskOverview:
    humidity = _recent_env[-1]["humidity_pct"] if _recent_env else 54.0
    nocturia = max(1, len([v for v in _recent_voids if v.get("occurred_at")])) if _recent_voids else 1
    hydration = hydration_risk(_recent_voids, _recent_bottle)
    uti = uti_risk(_recent_voids, nocturia)
    fall = fall_risk(_recent_mat, humidity)
    return RiskOverview(
        hydration_risk=hydration,
        uti_risk=uti,
        fall_risk=fall,
        nocturia_forecast_next_7d=nocturia_forecast([1, 1, 2, 2, 3, 2, nocturia]),
        recommendations=[next_best_nudge(hydration, uti, fall)],
    )


@app.post("/api/v1/trend", response_model=TrendResponse)
async def trend(request: TrendRequest) -> TrendResponse:
    hydration = hydration_risk(
        [{"sg_q1000": v} for v in request.strip_sg_q1000],
        [{"consumed_ml_day": v} for v in request.bottle_intake_ml],
    )
    uti = uti_risk(
        [{"leukocyte": 1 if sg > 1022 else 0, "nitrite": 1 if n > 2 else 0, "blood": 0} for sg, n in zip(request.strip_sg_q1000, request.voids_per_night)],
        request.voids_per_night[-1] if request.voids_per_night else 1,
    )
    fall = fall_risk([
        {"sway_index": 52 + (request.voids_per_night[-1] if request.voids_per_night else 1) * 4, "transfer_latency_ms": 1200}
    ], request.room_temp_c[-1] + 30.0 if request.room_temp_c else 60.0)
    return TrendResponse(
        dehydration_risk_24h=hydration,
        uti_risk=uti,
        stone_adherence_score=round(max(0.0, 1.0 - hydration), 3),
        next_best_nudge=next_best_nudge(hydration, uti, fall),
    )


@app.get("/api/v1/alerts", response_model=list[AlertEvent])
async def alerts() -> list[AlertEvent]:
    current = await overview()
    active_alerts: list[AlertEvent] = []
    if current.hydration_risk > 0.5:
        active_alerts.append(AlertEvent(severity='warning', kind='hydration', message='Hydration risk elevated; prompt 300 mL intake now.'))
    if current.uti_risk > 0.45:
        active_alerts.append(AlertEvent(severity='warning', kind='uti', message='Chemistry and frequency suggest possible urinary irritation or infection.'))
    if current.fall_risk > 0.55:
        active_alerts.append(AlertEvent(severity='urgent', kind='fall', message='Night-trip fall risk elevated; enable guided lighting and caregiver check.'))
    if _recent_env and _recent_env[-1].get('leak_detected'):
        active_alerts.append(AlertEvent(severity='urgent', kind='leak', message='Bathroom leak detected at floor trace.'))
    return active_alerts


@app.websocket("/ws/live")
async def ws_live(websocket: WebSocket) -> None:
    await websocket.accept()
    _websockets.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        _websockets.discard(websocket)
