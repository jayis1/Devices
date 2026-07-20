#!/usr/bin/env python3
"""
MosquitoSync — Cloud Backend (FastAPI + MQTT + InfluxDB + PostgreSQL)

Endpoints:
  /api/v1/auth/login            — JWT login
  /api/v1/devices                — List devices
  /api/v1/acoustic               — Latest acoustic sentinel data
  /api/v1/acoustic/history       — Historical detections
  /api/v1/trap                   — Latest CO2 trap data
  /api/v1/trap/images            — Trap camera capture images
  /api/v1/barrier/status         — Window barrier status
  /api/v1/barrier/close           — Close all barriers (POST)
  /api/v1/barrier/open            — Open all barriers (POST)
  /api/v1/weather                 — Current weather
  /api/v1/alerts                  — List alerts
  /api/v1/bite-risk               — BiteRisk Score (0-100)
  /api/v1/disease-risk            — DiseaseRisk Score + per-disease
  /api/v1/activity-forecast        — 72-hour activity forecast
  /api/v1/species                 — Species detected (24h/7d/30d)
  /api/v1/trap-count              — Daily capture count history
  /api/v1/ml/predict/activity     — Activity forecast prediction
  /api/v1/ml/predict/disease       — Disease risk prediction
  /api/v1/ml/predict/bite         — Personal bite risk prediction
  /api/v1/ws                      — Real-time WebSocket (telemetry, alerts)
"""
from __future__ import annotations

import asyncio
import json
import time
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from typing import Any

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer
from pydantic import BaseModel


# ─── Models ────────────────────────────────────────────────────────────────

class Device(BaseModel):
    device_id: str
    device_type: str  # hub, acoustic, trap, barrier, weather
    name: str
    firmware_version: str
    online: bool
    last_seen: datetime | None = None


class AcousticReading(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    temp_c: float
    humidity_pct: float
    mosquito_detected: bool
    species_class: int
    species_name: str
    confidence_pct: float
    wingbeat_freq_hz: float
    detections_24h: int
    audio_energy: float


class TrapReading(BaseModel):
    node_id: int
    timestamp: datetime
    temp_c: float
    humidity_pct: float
    pressure_hpa: float
    rain_mm: float
    ir_breaks: int
    capture_24h: int
    trap_fullness_pct: float
    co2_on: bool
    propane_pct: float
    fan_pct: float
    dominant_species: int
    dominant_species_name: str


class BarrierStatus(BaseModel):
    node_id: int
    timestamp: datetime
    screen_status: str  # open, closed, moving
    last_trigger: str  # manual, hub, auto-detected
    cycles_24h: int
    battery_v: float


class WeatherReading(BaseModel):
    temp_c: float
    humidity_pct: float
    pressure_hpa: float
    wind_speed_ms: float
    wind_dir_deg: int
    rain_mm: float


class BiteRisk(BaseModel):
    score: int  # 0-100
    level: str  # Low, Moderate, High, Critical
    dominant_species: str
    recommendations: list[str]


class DiseaseRisk(BaseModel):
    score: int  # 0-100
    level: str  # Low, Moderate, High, Critical
    dengue_risk: float  # 0-1
    west_nile_risk: float  # 0-1
    malaria_risk: float  # 0-1
    contributing_factors: dict[str, float]


class ActivityForecast(BaseModel):
    timestamps: list[datetime]
    activity_index: list[float]  # 0-1, 72 points
    confidence_low: list[float]
    confidence_high: list[float]


# ─── Species names ──────────────────────────────────────────────────────────

SPECIES_NAMES = [
    "Aedes aegypti",      # 0 — Dengue, Zika, Yellow Fever
    "Aedes albopictus",   # 1 — Dengue, Chikungunya
    "Anopheles gambiae",  # 2 — Malaria
    "Anopheles stephensi", # 3 — Malaria
    "Culex quinquefasciatus", # 4 — West Nile, Lymphatic Filariasis
    "Culex pipiens",      # 5 — West Nile
    "Mansonia uniformis", # 6 — Lymphatic Filariasis
    "Non-mosquito",       # 7
]

DISEASE_VECTORS = {0, 1, 2, 3, 4, 5}


# ─── In-memory stores (production: PostgreSQL + InfluxDB) ──────────────────

class DataStore:
    def __init__(self) -> None:
        self.devices: dict[str, Device] = {}
        self.acoustic_readings: list[AcousticReading] = []
        self.trap_readings: list[TrapReading] = []
        self.barrier_statuses: dict[int, BarrierStatus] = {}
        self.weather: WeatherReading | None = None
        self.alerts: list[dict[str, Any]] = []
        self.trap_images: list[dict[str, Any]] = []
        self.capture_counts: list[dict[str, Any]] = []
        self.bite_risk: BiteRisk = BiteRisk(
            score=15, level="Low", dominant_species="None detected",
            recommendations=["No action needed"]
        )
        self.disease_risk: DiseaseRisk = DiseaseRisk(
            score=10, level="Low", dengue_risk=0.02, west_nile_risk=0.01,
            malaria_risk=0.005, contributing_factors={"temperature": 0.3}
        )
        self.activity_forecast: ActivityForecast | None = None

    def add_acoustic(self, reading: AcousticReading) -> None:
        self.acoustic_readings.append(reading)
        # Keep last 10000
        if len(self.acoustic_readings) > 10000:
            self.acoustic_readings = self.acoustic_readings[-10000:]
        # Generate alert if disease vector detected
        if reading.mosquito_detected and reading.species_class in DISEASE_VECTORS:
            self.alerts.append({
                "id": len(self.alerts),
                "timestamp": reading.timestamp.isoformat(),
                "type": "disease_vector_detected",
                "severity": "critical",
                "message": f"Disease vector {SPECIES_NAMES[reading.species_class]} "
                           f"detected (confidence: {reading.confidence_pct}%)",
                "species": SPECIES_NAMES[reading.species_class],
                "confidence": reading.confidence_pct,
            })


store = DataStore()

# OAuth2
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")

# ─── MQTT Bridge (simulated — production: paho-mqtt async) ──────────────────

class MQTTPublisher:
    """In production: connect to mosquitto broker, publish to:
      mosquitosync/{user}/hub/telemetry
      mosquitosync/{user}/hub/acoustic
      mosquitosync/{user}/hub/trap
    Subscribe to:
      mosquitosync/{user}/cloud/command
      mosquitosync/{user}/cloud/ota
    """
    def __init__(self) -> None:
        self.connected = False

    async def connect(self) -> None:
        # In production: paho.mqtt.Client + connect
        self.connected = True
        print("[MQTT] Connected to broker")

    async def publish(self, topic: str, payload: dict) -> None:
        # In production: mqtt_client.publish(topic, json.dumps(payload))
        print(f"[MQTT] pub {topic}: {json.dumps(payload)[:120]}")

    async def publish_command(self, device_id: str, command: str, data: dict) -> None:
        topic = f"mosquitosync/default/cloud/command"
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
    # Start background telemetry simulation
    asyncio.create_task(telemetry_simulator())
    yield
    # Shutdown


app = FastAPI(title="MosquitoSync", version="1.0.0", lifespan=lifespan)
app.add_middleware(
    CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"]
)


async def telemetry_simulator() -> None:
    """Simulate periodic telemetry for demo. Production: MQTT subscriber."""
    while True:
        await asyncio.sleep(30)
        if store.weather is None:
            store.weather = WeatherReading(
                temp_c=27.0, humidity_pct=65.0, pressure_hpa=1013.0,
                wind_speed_ms=1.5, wind_dir_deg=180, rain_mm=0.0
            )
        await manager.broadcast({"type": "heartbeat", "ts": datetime.now(timezone.utc).isoformat()})


# ─── Auth ───────────────────────────────────────────────────────────────────

@app.post("/api/v1/auth/login")
async def login(username: str = "demo", password: str = "demo"):
    """Simple JWT login. Production: verify against DB."""
    from jose import jwt as jose_jwt
    import os
    token = jose_jwt.encode(
        {"sub": username, "exp": int(time.time()) + 86400},
        os.environ.get("JWT_SECRET", "mosquitosync-dev-secret"),
        algorithm="HS256",
    )
    return {"access_token": token, "token_type": "bearer"}


# ─── Devices ────────────────────────────────────────────────────────────────

@app.get("/api/v1/devices")
async def list_devices():
    return list(store.devices.values()) if store.devices else [
        {"device_id": "hub-001", "device_type": "hub", "name": "Hub",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "acoustic-001", "device_type": "acoustic",
         "name": "Living Room Sentinel", "firmware_version": "1.0.0", "online": True},
        {"device_id": "acoustic-002", "device_type": "acoustic",
         "name": "Bedroom Sentinel", "firmware_version": "1.0.0", "online": True},
        {"device_id": "trap-001", "device_type": "trap",
         "name": "Garden Trap", "firmware_version": "1.0.0", "online": True},
        {"device_id": "barrier-001", "device_type": "barrier",
         "name": "Living Room Window", "firmware_version": "1.0.0", "online": True},
    ]


@app.post("/api/v1/devices/{device_id}/ota")
async def trigger_ota(device_id: str, version: str = "1.1.0"):
    await mqtt.publish_command(device_id, "ota", {"version": version})
    return {"status": "ota_triggered", "device": device_id, "version": version}


# ─── Acoustic ──────────────────────────────────────────────────────────────

@app.get("/api/v1/acoustic", response_model=list[AcousticReading])
async def get_acoustic():
    return store.acoustic_readings[-20:] if store.acoustic_readings else []


@app.get("/api/v1/acoustic/history")
async def get_acoustic_history(hours: int = 24):
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    return [r.model_dump() for r in store.acoustic_readings
            if r.timestamp.timestamp() > cutoff]


# ─── Trap ──────────────────────────────────────────────────────────────────

@app.get("/api/v1/trap", response_model=list[TrapReading])
async def get_trap():
    return store.trap_readings[-20:] if store.trap_readings else []


@app.get("/api/v1/trap/images")
async def get_trap_images(limit: int = 10):
    return store.trap_images[-limit:]


# ─── Barrier ────────────────────────────────────────────────────────────────

@app.get("/api/v1/barrier/status", response_model=list[BarrierStatus])
async def get_barrier_status():
    return list(store.barrier_statuses.values()) if store.barrier_statuses else []


@app.post("/api/v1/barrier/close")
async def close_all_barriers():
    await mqtt.publish_command("all", "barrier_close", {"trigger": "user"})
    await manager.broadcast({"type": "barrier_command", "action": "close_all"})
    return {"status": "closing_all_barriers"}


@app.post("/api/v1/barrier/open")
async def open_all_barriers():
    await mqtt.publish_command("all", "barrier_open", {"trigger": "user"})
    await manager.broadcast({"type": "barrier_command", "action": "open_all"})
    return {"status": "opening_all_barriers"}


# ─── Weather ───────────────────────────────────────────────────────────────

@app.get("/api/v1/weather", response_model=WeatherReading)
async def get_weather():
    if store.weather is None:
        raise HTTPException(404, "No weather data yet")
    return store.weather


# ─── Alerts ────────────────────────────────────────────────────────────────

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


# ─── Risk Scores ────────────────────────────────────────────────────────────

@app.get("/api/v1/bite-risk", response_model=BiteRisk)
async def get_bite_risk():
    return store.bite_risk


@app.get("/api/v1/disease-risk", response_model=DiseaseRisk)
async def get_disease_risk():
    return store.disease_risk


@app.get("/api/v1/activity-forecast", response_model=ActivityForecast)
async def get_activity_forecast():
    if store.activity_forecast is None:
        raise HTTPException(404, "No forecast available yet")
    return store.activity_forecast


# ─── Species Stats ─────────────────────────────────────────────────────────

@app.get("/api/v1/species")
async def get_species_stats(period: str = "24h"):
    """Species detection breakdown for 24h/7d/30d."""
    hours = {"24h": 24, "7d": 168, "30d": 720}.get(period, 24)
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    counts: dict[str, int] = {}
    for r in store.acoustic_readings:
        if r.timestamp.timestamp() > cutoff and r.mosquito_detected:
            name = SPECIES_NAMES[r.species_class]
            counts[name] = counts.get(name, 0) + 1
    return {"period": period, "species_counts": counts}


@app.get("/api/v1/trap-count")
async def get_trap_count(days: int = 7):
    return store.capture_counts[-days:] if store.capture_counts else []


# ─── ML Predictions ────────────────────────────────────────────────────────

@app.get("/api/v1/ml/predict/activity", response_model=ActivityForecast)
async def predict_activity():
    """Production: call ML pipeline inference service.
    Here: return cached forecast or generate stub.
    """
    if store.activity_forecast is None:
        now = datetime.now(timezone.utc)
        import math
        ts = [now.replace(minute=0, second=0)]
        for i in range(71):
            ts.append(ts[0])
        # Generate 72-hour synthetic forecast
        activity = []
        for i in range(72):
            # Peak at dusk (hour 18) and dawn (hour 6)
            hour = (i % 24)
            base = 0.3 + 0.4 * max(
                math.exp(-((hour - 18) ** 2) / 8),
                math.exp(-((hour - 6) ** 2) / 8),
            )
            activity.append(min(1.0, base))
        store.activity_forecast = ActivityForecast(
            timestamps=[now] * 72,
            activity_index=activity,
            confidence_low=[a * 0.85 for a in activity],
            confidence_high=[min(1.0, a * 1.15) for a in activity],
        )
    return store.activity_forecast


@app.get("/api/v1/ml/predict/disease", response_model=DiseaseRisk)
async def predict_disease():
    return store.disease_risk


@app.get("/api/v1/ml/predict/bite", response_model=BiteRisk)
async def predict_bite():
    return store.bite_risk


# ─── WebSocket ─────────────────────────────────────────────────────────────

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            data = await ws.receive_text()
            msg = json.loads(data)
            # Handle commands from client
            if msg.get("type") == "close_barriers":
                await close_all_barriers()
    except WebSocketDisconnect:
        manager.disconnect(ws)


# ─── Run ────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)