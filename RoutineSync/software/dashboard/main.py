"""RoutineSync FastAPI backend."""

from __future__ import annotations

import asyncio
import json
import os
import sqlite3
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from ml_inference import evaluate_departure, evaluate_focus, find_item, overview_payload, routine_statuses
from models import DepartureRequest, DepartureResponse, FindItemRequest, FindItemResponse, FocusRequest, FocusResponse, OverviewResponse, RoutineStatus

try:  # pragma: no cover - optional dependency
    import paho.mqtt.client as mqtt
except Exception:  # pragma: no cover
    mqtt = None

DB_PATH = Path(os.getenv("ROUTINESYNC_DB", Path(__file__).with_name("routinesync.db")))
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
app = FastAPI(title="RoutineSync API", version="1.0.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])
_websockets: set[WebSocket] = set()
_mqtt_client: Optional["mqtt.Client"] = None


def init_db() -> None:
    with closing(sqlite3.connect(DB_PATH)) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                topic TEXT NOT NULL,
                payload TEXT NOT NULL,
                created_at TEXT NOT NULL
            )
            """
        )
        conn.commit()


def log_event(topic: str, payload: dict) -> None:
    init_db()
    with closing(sqlite3.connect(DB_PATH)) as conn:
        conn.execute(
            "INSERT INTO events(topic, payload, created_at) VALUES (?, ?, ?)",
            (topic, json.dumps(payload, sort_keys=True), datetime.now(timezone.utc).isoformat()),
        )
        conn.commit()


async def broadcast(message: dict) -> None:
    stale: list[WebSocket] = []
    for ws in list(_websockets):
        try:
            await ws.send_json(message)
        except RuntimeError:
            stale.append(ws)
    for ws in stale:
        _websockets.discard(ws)


@app.on_event("startup")
async def startup() -> None:
    global _mqtt_client
    init_db()
    if mqtt is not None:
        _mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        try:
            _mqtt_client.connect(MQTT_BROKER, MQTT_PORT, keepalive=30)
            _mqtt_client.loop_start()
        except Exception:
            _mqtt_client = None
    asyncio.create_task(_heartbeat())


@app.on_event("shutdown")
async def shutdown() -> None:
    global _mqtt_client
    if _mqtt_client is not None:
        _mqtt_client.loop_stop()
        _mqtt_client.disconnect()
        _mqtt_client = None


async def _heartbeat() -> None:
    while True:  # pragma: no branch
        await asyncio.sleep(30)
        await broadcast({"type": "heartbeat", "time": datetime.now(timezone.utc).isoformat()})


@app.get("/api/v1/health")
async def health() -> dict:
    return {"status": "ok", "time": datetime.now(timezone.utc).isoformat(), "db": str(DB_PATH)}


@app.get("/api/v1/overview", response_model=OverviewResponse)
async def overview() -> OverviewResponse:
    payload = overview_payload()
    return OverviewResponse(**payload)


@app.get("/api/v1/routines", response_model=list[RoutineStatus])
async def routines() -> list[RoutineStatus]:
    return [RoutineStatus(**entry) for entry in routine_statuses()]


@app.post("/api/v1/departure/evaluate", response_model=DepartureResponse)
async def departure_evaluate(request: DepartureRequest) -> DepartureResponse:
    result = evaluate_departure(request)
    log_event("departure.evaluate", result)
    if _mqtt_client is not None:
        _mqtt_client.publish("routinesync/departure/evaluate", json.dumps(result), qos=1)
    await broadcast({"type": "departure", "payload": result})
    return DepartureResponse(**result)


@app.post("/api/v1/find-item", response_model=FindItemResponse)
async def find_item_route(request: FindItemRequest) -> FindItemResponse:
    result = find_item(
        request.item_name,
        request.last_seen_room,
        request.minutes_since_seen,
        request.movement_events,
        request.doorway_seen,
    )
    log_event("find-item", result)
    return FindItemResponse(**result)


@app.post("/api/v1/focus/evaluate", response_model=FocusResponse)
async def focus_evaluate(request: FocusRequest) -> FocusResponse:
    result = evaluate_focus(request)
    log_event("focus.evaluate", result)
    await broadcast({"type": "focus", "payload": result})
    return FocusResponse(**result)


@app.websocket("/api/v1/live/ws")
async def live_ws(websocket: WebSocket) -> None:
    await websocket.accept()
    _websockets.add(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        _websockets.discard(websocket)
