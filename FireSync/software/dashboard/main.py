#!/usr/bin/env python3
"""
FireSync — Cloud Backend (FastAPI + MQTT + InfluxDB + PostgreSQL)

Endpoints:
  /api/v1/auth/login              — JWT login
  /api/v1/devices                 — List devices
  /api/v1/sentinels               — List Room Sentinels
  /api/v1/sentinels/{id}          — Sentinel detail + telemetry
  /api/v1/stove                   — Stove Guard telemetry
  /api/v1/panel                   — Panel Monitor telemetry
  /api/v1/escape                  — Escape Controller status
  /api/v1/fire/events             — Fire event history
  /api/v1/fire/active              — Active fire event
  /api/v1/alarm/test               — Monthly test alarm
  /api/v1/alarm/silence            — Silence alarm
  /api/v1/occupants                — Room occupancy map
  /api/v1/route                    — Current escape route
  /api/v1/rooms                    — Room configuration
  /api/v1/risk/forecast             — 7-day fire risk forecast
  /api/v1/risk/weekly               — Weekly fire risk report
  /api/v1/alerts                   — Alerts list
  /api/v1/dispatch/cancel           — Cancel 911 dispatch
  /api/v1/dispatch/status           — 911 dispatch status
  /api/v1/suppression/status        — Suppression system status
  /api/v1/ml/flamenet/history      — FlameNet classification history
  /api/v1/ml/thermal/anomalies      — Thermal anomaly history
  /api/v1/ml/arcdetect/history      — Arc detection history
  /api/v1/ws                       — Real-time WebSocket
"""
from __future__ import annotations

import asyncio
import json
import time
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer
from pydantic import BaseModel


# ─── Models ────────────────────────────────────────────────────────────────

class Device(BaseModel):
    device_id: str
    device_type: str  # hub, sentinel, stove, panel, escape
    name: str
    firmware_version: str
    online: bool
    last_seen: datetime | None = None


class SentinelTelemetry(BaseModel):
    node_id: int
    room_id: int
    timestamp: datetime
    battery_v: float
    smoke_pm25: int
    co_ppm: int
    temp_c: float
    temp_rate: float
    thermal_max_c: float
    thermal_mean_c: float
    flame_class: int
    flame_confidence: int
    pir_occupant: bool
    flamenet_ms: int
    thermal_anomaly_score: int


class StoveTelemetry(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    thermal_max_c: float
    thermal_mean_c: float
    knob_positions: int
    pantemp_class: int
    timer_remaining_s: int
    valve_state: str  # open, closed
    buzzer_active: bool


class PanelTelemetry(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    main_current_a: float
    voltage_v: float
    power_w: float
    bus_bar_temp_c: float
    breaker_temp_c: float
    arc_fault_class: int
    arc_confidence: int
    shunt_tripped: bool


class EscapeTelemetry(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    led_zones_active: int
    speaker_active: bool
    doors_released: int
    route_active: bool


class FireEvent(BaseModel):
    id: int
    timestamp: datetime
    room_id: int
    fire_class: str  # smoldering, flaming, CO, arc, thermal
    confidence: int
    smoke_pm25: int
    co_ppm: int
    temp_c: float
    thermal_max_c: float
    occupant: bool
    confirmed: bool
    actions_taken: list[str]
    dispatch_911: bool
    resolved: bool
    false_alarm: bool = False


class Alert(BaseModel):
    id: int
    timestamp: datetime
    alert_type: str
    severity: str
    message: str
    acknowledged: bool = False


class RoomConfig(BaseModel):
    room_id: int
    name: str
    adjacent: list[int]
    has_exit: str  # none, front, back, garage, window
    sentinel_node: int


class RiskForecast(BaseModel):
    score: int  # 0-100
    level: str  # low, moderate, high, very_high
    factors: list[dict]  # SHAP attributions
    recommendation: str


class SuppressionStatus(BaseModel):
    stove_valve: str  # open, closed
    panel_shunt_tripped: bool
    hvac_off: bool
    hood_suppression: bool


# ─── FlameNet class names ──────────────────────────────────────────────────

FLAME_CLASSES = {
    0: "normal", 1: "cooking", 2: "steam", 3: "cigarette",
    4: "candle", 5: "smoldering", 6: "flaming_fire",
}

ARC_CLASSES = {
    0: "normal", 1: "series_arc", 2: "parallel_arc", 3: "overload",
}

PANTEMP_CLASSES = {
    0: "safe_cooking", 1: "overheating", 2: "oil_smoking", 3: "flaming",
}

SEVERITY_MAP = {0: "info", 1: "warning", 2: "critical", 3: "emergency"}


# ─── In-memory stores (production: PostgreSQL + InfluxDB) ──────────────────

class DataStore:
    def __init__(self) -> None:
        self.devices: dict[str, Device] = {}
        self.sentinel_telemetry: list[SentinelTelemetry] = []
        self.stove_telemetry: list[StoveTelemetry] = []
        self.panel_telemetry: list[PanelTelemetry] = []
        self.escape_telemetry: list[EscapeTelemetry] = []
        self.fire_events: list[dict[str, Any]] = []
        self.alerts: list[dict[str, Any]] = []
        self.rooms: dict[int, RoomConfig] = {}
        self.occupant_map: dict[int, bool] = {}
        self.suppression = SuppressionStatus(
            stove_valve="open", panel_shunt_tripped=False,
            hvac_off=False, hood_suppression=False
        )
        self.dispatch_active = False
        self.active_fire: dict | None = None

        # Seed default rooms
        self.rooms[0] = RoomConfig(room_id=0, name="Kitchen",
                                    adjacent=[1], has_exit="none", sentinel_node=1)
        self.rooms[1] = RoomConfig(room_id=1, name="Living Room",
                                    adjacent=[0, 2], has_exit="front", sentinel_node=2)
        self.rooms[2] = RoomConfig(room_id=2, name="Bedroom",
                                    adjacent=[1, 3], has_exit="window", sentinel_node=3)
        self.rooms[3] = RoomConfig(room_id=3, name="Bathroom",
                                    adjacent=[2], has_exit="none", sentinel_node=4)

    def add_alert(self, alert_type: str, severity: str, message: str) -> None:
        self.alerts.append({
            "id": len(self.alerts),
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "alert_type": alert_type,
            "severity": severity,
            "message": message,
            "acknowledged": False,
        })


store = DataStore()
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")


# ─── MQTT Bridge (simulated) ────────────────────────────────────────────────

class MQTTPublisher:
    def __init__(self) -> None:
        self.connected = False

    async def connect(self) -> None:
        self.connected = True
        print("[MQTT] Connected to broker")

    async def publish(self, topic: str, payload: dict) -> None:
        print(f"[MQTT] pub {topic}: {json.dumps(payload)[:120]}")

    async def publish_command(self, device_id: str, command: str, data: dict) -> None:
        topic = f"firesync/default/cloud/command"
        await self.publish(topic, {"device": device_id, "command": command, **data})


mqtt = MQTTPublisher()


# ─── WebSocket Connection Manager ──────────────────────────────────────────

class ConnectionManager:
    def __init__(self) -> None:
        self.active: list[WebSocket] = []

    async def connect(self, ws: WebSocket) -> None:
        await ws.accept()
        self.active.append(ws)

    def disconnect(self, ws: WebSocket) -> None:
        if ws in self.active:
            self.active.remove(ws)

    async def broadcast(self, message: dict) -> None:
        for ws in self.active:
            try:
                await ws.send_json(message)
            except Exception:
                self.disconnect(ws)


manager = ConnectionManager()


# ─── App ────────────────────────────────────────────────────────────────────

@asynccontextmanager
async def lifespan(app: FastAPI):
    await mqtt.connect()
    asyncio.create_task(telemetry_simulator())
    yield


app = FastAPI(title="FireSync", version="1.0.0", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)


async def telemetry_simulator() -> None:
    """Simulate periodic telemetry for demo. Production: MQTT subscriber."""
    while True:
        await asyncio.sleep(30)
        await manager.broadcast({
            "type": "heartbeat",
            "ts": datetime.now(timezone.utc).isoformat()
        })


# ─── Auth ───────────────────────────────────────────────────────────────────

@app.post("/api/v1/auth/login")
async def login(username: str = "demo", password: str = "demo"):
    from jose import jwt as jose_jwt
    import os
    token = jose_jwt.encode(
        {"sub": username, "exp": int(time.time()) + 86400},
        os.environ.get("JWT_SECRET", "firesync-dev-secret"),
        algorithm="HS256",
    )
    return {"access_token": token, "token_type": "bearer"}


# ─── Devices ────────────────────────────────────────────────────────────────

@app.get("/api/v1/devices")
async def list_devices():
    return list(store.devices.values()) if store.devices else [
        {"device_id": "hub-001", "device_type": "hub", "name": "FireSync Hub",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "sentinel-001", "device_type": "sentinel", "name": "Kitchen Sentinel",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "sentinel-002", "device_type": "sentinel", "name": "Living Room Sentinel",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "sentinel-003", "device_type": "sentinel", "name": "Bedroom Sentinel",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "stove-001", "device_type": "stove", "name": "Stove Guard",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "panel-001", "device_type": "panel", "name": "Panel Monitor",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "escape-001", "device_type": "escape", "name": "Escape Controller",
         "firmware_version": "1.0.0", "online": True},
    ]


@app.post("/api/v1/devices/{device_id}/ota")
async def trigger_ota(device_id: str, version: str = "1.1.0"):
    await mqtt.publish_command(device_id, "ota", {"version": version})
    return {"status": "ota_triggered", "device": device_id, "version": version}


# ─── Sentinels ───────────────────────────────────────────────────────────────

@app.get("/api/v1/sentinels")
async def list_sentinels():
    return [
        {"node_id": 1, "room_id": 0, "room_name": "Kitchen", "online": True, "battery_v": 4.15},
        {"node_id": 2, "room_id": 1, "room_name": "Living Room", "online": True, "battery_v": 4.10},
        {"node_id": 3, "room_id": 2, "room_name": "Bedroom", "online": True, "battery_v": 4.18},
        {"node_id": 4, "room_id": 3, "room_name": "Bathroom", "online": True, "battery_v": 4.12},
    ]


@app.get("/api/v1/sentinels/{node_id}")
async def get_sentinel_detail(node_id: int):
    return {
        "node_id": node_id,
        "latest_telemetry": {
            "smoke_pm25": 15, "co_ppm": 2, "temp_c": 23.5,
            "thermal_max_c": 25.1, "flame_class": 0,
            "flame_class_name": "normal", "flame_confidence": 95,
            "pir_occupant": False, "battery_v": 4.15,
        }
    }


# ─── Stove Guard ────────────────────────────────────────────────────────────

@app.get("/api/v1/stove", response_model=list[StoveTelemetry])
async def get_stove_telemetry():
    return store.stove_telemetry[-20:] if store.stove_telemetry else []


# ─── Panel Monitor ──────────────────────────────────────────────────────────

@app.get("/api/v1/panel", response_model=list[PanelTelemetry])
async def get_panel_telemetry():
    return store.panel_telemetry[-20:] if store.panel_telemetry else []


# ─── Escape Controller ──────────────────────────────────────────────────────

@app.get("/api/v1/escape", response_model=list[EscapeTelemetry])
async def get_escape_telemetry():
    return store.escape_telemetry[-20:] if store.escape_telemetry else []


# ─── Fire Events ────────────────────────────────────────────────────────────

@app.get("/api/v1/fire/events")
async def get_fire_events(limit: int = 50):
    return store.fire_events[-limit:]


@app.get("/api/v1/fire/active")
async def get_active_fire():
    return store.active_fire or {"active": False}


@app.post("/api/v1/fire/{event_id}/ack")
async def ack_fire_event(event_id: int):
    for e in store.fire_events:
        if e.get("id") == event_id:
            e["acknowledged"] = True
            return {"status": "acknowledged"}
    raise HTTPException(404, "Fire event not found")


@app.post("/api/v1/fire/{event_id}/false")
async def mark_false_alarm(event_id: int):
    for e in store.fire_events:
        if e.get("id") == event_id:
            e["false_alarm"] = True
            e["resolved"] = True
            store.active_fire = None
            store.dispatch_active = False
            await manager.broadcast({"type": "false_alarm", "event_id": event_id})
            return {"status": "marked_false_alarm"}
    raise HTTPException(404, "Fire event not found")


# ─── Alarm Control ──────────────────────────────────────────────────────────

@app.post("/api/v1/alarm/test")
async def test_alarm():
    await mqtt.publish_command("hub-001", "test_alarm", {})
    await manager.broadcast({"type": "test_alarm_started"})
    return {"status": "test_alarm_started"}


@app.post("/api/v1/alarm/silence")
async def silence_alarm():
    await mqtt.publish_command("hub-001", "silence", {})
    store.active_fire = None
    await manager.broadcast({"type": "alarm_silenced"})
    return {"status": "alarm_silenced"}


# ─── Occupants ──────────────────────────────────────────────────────────────

@app.get("/api/v1/occupants")
async def get_occupants():
    result = {}
    for room_id, occupied in store.occupant_map.items():
        room_name = store.rooms.get(room_id, RoomConfig(
            room_id=room_id, name=f"Room {room_id}", adjacent=[],
            has_exit="none", sentinel_node=0)).name
        result[room_name] = "occupied" if occupied else "empty"
    return result


# ─── Escape Route ──────────────────────────────────────────────────────────

@app.get("/api/v1/route")
async def get_route():
    if not store.active_fire:
        return {"active": False}
    return {
        "active": True,
        "fire_room": store.active_fire.get("room_id"),
        "safe_exit": "front",
        "avoid_rooms": [0, 1],
    }


# ─── Rooms ──────────────────────────────────────────────────────────────────

@app.get("/api/v1/rooms", response_model=list[RoomConfig])
async def get_rooms():
    return list(store.rooms.values())


@app.post("/api/v1/rooms", response_model=RoomConfig)
async def add_room(room: RoomConfig):
    store.rooms[room.room_id] = room
    return room


# ─── Risk Forecast ──────────────────────────────────────────────────────────

@app.get("/api/v1/risk/forecast", response_model=RiskForecast)
async def get_risk_forecast():
    return RiskForecast(
        score=28,
        level="low",
        factors=[
            {"feature": "ambient_humidity", "contribution": -5,
             "value": "45% RH (low humidity increases fire risk)"},
            {"feature": "electrical_load_peak", "contribution": 8,
             "value": "82nd percentile (higher than usual)"},
            {"feature": "cooking_events_7d", "contribution": 3,
             "value": "14 cooking sessions (normal)"},
            {"feature": "stove_max_temp_7d", "contribution": 12,
             "value": "195°C (elevated — check cookware)"},
            {"feature": "panel_max_temp", "contribution": 5,
             "value": "62°C (normal range)"},
            {"feature": "arcing_events_7d", "contribution": 5,
             "value": "1 minor event (monitor)"},
        ],
        recommendation="Fire risk is LOW. Stove temperatures have been slightly elevated "
                       "this week — ensure cookware is not left unattended."
    )


@app.get("/api/v1/risk/weekly")
async def get_weekly_report():
    return {
        "week": "2026-W32",
        "avg_risk_score": 25,
        "peak_risk_score": 42,
        "peak_day": "Wednesday",
        "trend": "stable",
        "top_factors": ["electrical_load", "stove_temperature", "low_humidity"],
        "recommendations": [
            "Ensure stove is never left unattended while cooking",
            "Check electrical panel for warm breakers quarterly",
            "Maintain indoor humidity above 30% in winter",
            "Test smoke alarms monthly (tap 'Test Alarm' in app)",
        ],
    }


# ─── Alerts ─────────────────────────────────────────────────────────────────

@app.get("/api/v1/alerts")
async def get_alerts(limit: int = 50):
    return store.alerts[-limit:]


@app.put("/api/v1/alerts/{alert_id}/ack")
async def ack_alert(alert_id: int):
    for a in store.alerts:
        if a["id"] == alert_id:
            a["acknowledged"] = True
            return {"status": "acknowledged", "alert_id": alert_id}
    raise HTTPException(404, "Alert not found")


# ─── Emergency Dispatch ──────────────────────────────────────────────────────

@app.post("/api/v1/dispatch/cancel")
async def cancel_dispatch():
    store.dispatch_active = False
    await mqtt.publish_command("hub-001", "cancel_dispatch", {})
    store.add_alert("dispatch_cancelled", "info", "911 dispatch cancelled (false alarm)")
    await manager.broadcast({"type": "dispatch_cancelled"})
    return {"status": "dispatch_cancelled"}


@app.get("/api/v1/dispatch/status")
async def dispatch_status():
    return {"active": store.dispatch_active, "dispatched_at": None}


# ─── Suppression Status ──────────────────────────────────────────────────────

@app.get("/api/v1/suppression/status", response_model=SuppressionStatus)
async def get_suppression_status():
    return store.suppression


# ─── ML Endpoints ────────────────────────────────────────────────────────────

@app.get("/api/v1/ml/flamenet/history")
async def flamenet_history(limit: int = 50):
    return [
        {"timestamp": datetime.now(timezone.utc).isoformat(),
         "room": "Kitchen", "class": "cooking", "confidence": 88,
         "smoke_pm25": 180, "co_ppm": 5, "thermal_max_c": 85.2},
    ]


@app.get("/api/v1/ml/thermal/anomalies")
async def thermal_anomalies(limit: int = 50):
    return [
        {"timestamp": datetime.now(timezone.utc).isoformat(),
         "room": "Kitchen", "anomaly_score": 60, "thermal_max_c": 28.5},
    ]


@app.get("/api/v1/ml/arcdetect/history")
async def arcdetect_history(limit: int = 50):
    return [
        {"timestamp": datetime.now(timezone.utc).isoformat(),
         "class": "normal", "confidence": 95,
         "current_a": 12.5, "voltage_v": 240.1},
    ]


# ─── WebSocket ────────────────────────────────────────────────────────────────

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            data = await ws.receive_json()
            msg_type = data.get("type")
            if msg_type == "silence_alarm":
                await silence_alarm()
            elif msg_type == "test_alarm":
                await test_alarm()
            elif msg_type == "cancel_dispatch":
                await cancel_dispatch()
    except WebSocketDisconnect:
        manager.disconnect(ws)