#!/usr/bin/env python3
"""
VoiceSync — Cloud Backend (FastAPI + MQTT + InfluxDB + PostgreSQL)

Endpoints:
  /api/v1/auth/login            — JWT login
  /api/v1/devices                — List devices
  /api/v1/vocal-band              — Latest vocal band data
  /api/v1/vocal-band/history      — Historical vocal metrics
  /api/v1/room-sentinel           — Latest room sentinel data
  /api/v1/room-sentinel/history   — Historical voice quality
  /api/v1/hydration               — Current hydration status
  /api/v1/hydration/history       — Historical water intake
  /api/v1/humidity                 — Current humidity + humidifier status
  /api/v1/alerts                   — List alerts
  /api/v1/vocal-health             — Vocal Health Score (0-100)
  /api/v1/voice-disorder-risk      — 7-day disorder risk forecast
  /api/v1/vocal-load               — Today's cumulative vocal dose
  /api/v1/voice-quality            — Voice quality history + classification
  /api/v1/reflux-risk              — LPR reflux damage assessment
  /api/v1/humidifier/control       — Control humidifier (POST)
  /api/v1/ml/predict/risk          — ML risk prediction
  /api/v1/ml/predict/voice         — ML voice quality prediction
  /api/v1/reports/clinical         — Speech-pathologist-ready PDF report
  /api/v1/ws                       — Real-time WebSocket (telemetry, alerts)
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
    device_type: str  # hub, vocal_band, room, hydration, humidity
    name: str
    firmware_version: str
    online: bool
    last_seen: datetime | None = None


class VocalBandReading(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    f0_hz: float
    jitter_pct: float
    shimmer_pct: float
    hnr_db: float
    phonation_pct: int
    intensity_db: float
    skin_temp_c: float
    heart_rate: int
    hrv_rmssd: int
    stress_level: int


class RoomSentinelReading(BaseModel):
    node_id: int
    timestamp: datetime
    voice_quality_class: int
    voice_quality_name: str
    confidence_pct: float
    f0_hz: float
    phonation_pct: int
    temp_c: float
    humidity_pct: float
    voc_index: int
    db_spl: float
    talking_detected: bool


class HydrationReading(BaseModel):
    node_id: int
    timestamp: datetime
    battery_v: float
    water_mass_g: int
    sips_24h: int
    intake_ml: int
    last_sip_min: int
    hydration_pct: float


class HumidityReading(BaseModel):
    node_id: int
    timestamp: datetime
    temp_c: float
    humidity_pct: float
    tank_level_pct: int
    humidifier_on: bool
    fan_on: bool


class VocalHealth(BaseModel):
    score: int  # 0-100
    level: str  # Excellent, Good, Fair, Poor, Critical
    f0_hz: float
    jitter_pct: float
    shimmer_pct: float
    hnr_db: float
    phonation_pct: int
    recommendations: list[str]


class VoiceDisorderRisk(BaseModel):
    score: int  # 0-100
    level: str  # Low, Moderate, High, Critical
    nodules_risk: float  # 0-1
    reflux_risk: float  # 0-1
    fatigue_risk: float  # 0-1
    contributing_factors: dict[str, float]


class VocalLoad(BaseModel):
    phonation_time_min: int
    phonation_pct: float  # % of waking hours
    safe_dose_pct: float  # NCVS safe dose %
    intensity_avg_db: float
    rest_recommended: bool
    rest_minutes: int


class RefluxRisk(BaseModel):
    score: int  # 0-100
    detected: bool
    episodes_24h: int
    pattern_detected: bool
    recommendation: str


class VoiceRiskForecast(BaseModel):
    timestamps: list[datetime]
    risk_index: list[float]  # 0-1, 168 points
    confidence_low: list[float]
    confidence_high: list[float]


# ─── Voice quality names ───────────────────────────────────────────────────

VOICE_QUALITY_NAMES = [
    "Normal",       # 0
    "Hoarse",       # 1
    "Breathy",      # 2
    "Strained",     # 3
    "Tremor",       # 4
    "Fatigue",      # 5
    "Reflux",       # 6
    "Disorder",     # 7
]

CRITICAL_CLASSES = {1, 4, 6, 7}  # Hoarse, Tremor, Reflux, Disorder


# ─── In-memory stores (production: PostgreSQL + InfluxDB) ──────────────────

class DataStore:
    def __init__(self) -> None:
        self.devices: dict[str, Device] = {}
        self.vocal_band_readings: list[VocalBandReading] = []
        self.room_readings: list[RoomSentinelReading] = []
        self.hydration_readings: list[HydrationReading] = []
        self.humidity_readings: list[HumidityReading] = []
        self.alerts: list[dict[str, Any]] = []
        self.vocal_health: VocalHealth = VocalHealth(
            score=85, level="Good",
            f0_hz=140, jitter_pct=0.5, shimmer_pct=2.0, hnr_db=22,
            phonation_pct=15,
            recommendations=["Your vocal health is good. Keep up the hydration!"]
        )
        self.disorder_risk: VoiceDisorderRisk = VoiceDisorderRisk(
            score=15, level="Low", nodules_risk=0.05, reflux_risk=0.02,
            fatigue_risk=0.08, contributing_factors={"phonation": 0.3}
        )
        self.vocal_load: VocalLoad = VocalLoad(
            phonation_time_min=45, phonation_pct=8.0, safe_dose_pct=30.0,
            intensity_avg_db=65, rest_recommended=False, rest_minutes=0
        )
        self.reflux_risk: RefluxRisk = RefluxRisk(
            score=10, detected=False, episodes_24h=0,
            pattern_detected=False,
            recommendation="No reflux patterns detected."
        )
        self.risk_forecast: VoiceRiskForecast | None = None

    def add_vocal_band(self, reading: VocalBandReading) -> None:
        self.vocal_band_readings.append(reading)
        if len(self.vocal_band_readings) > 10000:
            self.vocal_band_readings = self.vocal_band_readings[-10000:]
        # Alert on elevated jitter (hoarseness marker)
        if reading.jitter_pct > 2.61:
            self.alerts.append({
                "id": len(self.alerts),
                "timestamp": reading.timestamp.isoformat(),
                "type": "hoarseness_detected",
                "severity": "warning",
                "message": f"Elevated jitter ({reading.jitter_pct:.2f}%) "
                           f"indicates vocal fold perturbation.",
                "jitter": reading.jitter_pct,
            })

    def add_room_reading(self, reading: RoomSentinelReading) -> None:
        self.room_readings.append(reading)
        if len(self.room_readings) > 10000:
            self.room_readings = self.room_readings[-10000:]
        # Alert on critical voice quality class
        if reading.voice_quality_class in CRITICAL_CLASSES:
            self.alerts.append({
                "id": len(self.alerts),
                "timestamp": reading.timestamp.isoformat(),
                "type": "voice_quality_critical",
                "severity": "critical",
                "message": f"Voice quality classified as "
                           f"'{reading.voice_quality_name}' "
                           f"({reading.confidence_pct}% confidence)",
                "class": reading.voice_quality_class,
            })


store = DataStore()

# OAuth2
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")


# ─── MQTT Bridge (simulated) ────────────────────────────────────────────────

class MQTTPublisher:
    """In production: connect to mosquitto broker, publish to:
      voicesync/{user}/hub/telemetry
      voicesync/{user}/hub/vocal_band
      voicesync/{user}/hub/room
    Subscribe to:
      voicesync/{user}/cloud/command
      voicesync/{user}/cloud/ota
    """
    def __init__(self) -> None:
        self.connected = False

    async def connect(self) -> None:
        self.connected = True
        print("[MQTT] Connected to broker")

    async def publish(self, topic: str, payload: dict) -> None:
        print(f"[MQTT] pub {topic}: {json.dumps(payload)[:120]}")

    async def publish_command(self, device_id: str, command: str, data: dict) -> None:
        topic = f"voicesync/default/cloud/command"
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


app = FastAPI(title="VoiceSync", version="1.0.0", lifespan=lifespan)
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
        os.environ.get("JWT_SECRET", "voicesync-dev-secret"),
        algorithm="HS256",
    )
    return {"access_token": token, "token_type": "bearer"}


# ─── Devices ────────────────────────────────────────────────────────────────

@app.get("/api/v1/devices")
async def list_devices():
    return list(store.devices.values()) if store.devices else [
        {"device_id": "hub-001", "device_type": "hub", "name": "Voice Hub",
         "firmware_version": "1.0.0", "online": True},
        {"device_id": "vocal-001", "device_type": "vocal_band",
         "name": "Vocal Band", "firmware_version": "1.0.0", "online": True},
        {"device_id": "room-001", "device_type": "room",
         "name": "Office Sentinel", "firmware_version": "1.0.0", "online": True},
        {"device_id": "hydration-001", "device_type": "hydration",
         "name": "Water Bottle", "firmware_version": "1.0.0", "online": True},
        {"device_id": "humidity-001", "device_type": "humidity",
         "name": "Humidity Node", "firmware_version": "1.0.0", "online": True},
    ]


@app.post("/api/v1/devices/{device_id}/ota")
async def trigger_ota(device_id: str, version: str = "1.1.0"):
    await mqtt.publish_command(device_id, "ota", {"version": version})
    return {"status": "ota_triggered", "device": device_id, "version": version}


# ─── Vocal Band ─────────────────────────────────────────────────────────────

@app.get("/api/v1/vocal-band", response_model=list[VocalBandReading])
async def get_vocal_band():
    return store.vocal_band_readings[-20:] if store.vocal_band_readings else []


@app.get("/api/v1/vocal-band/history")
async def get_vocal_band_history(hours: int = 24):
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    return [r.model_dump() for r in store.vocal_band_readings
            if r.timestamp.timestamp() > cutoff]


# ─── Room Sentinel ──────────────────────────────────────────────────────────

@app.get("/api/v1/room-sentinel", response_model=list[RoomSentinelReading])
async def get_room_sentinel():
    return store.room_readings[-20:] if store.room_readings else []


@app.get("/api/v1/room-sentinel/history")
async def get_room_sentinel_history(hours: int = 24):
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    return [r.model_dump() for r in store.room_readings
            if r.timestamp.timestamp() > cutoff]


# ─── Hydration ──────────────────────────────────────────────────────────────

@app.get("/api/v1/hydration", response_model=list[HydrationReading])
async def get_hydration():
    return store.hydration_readings[-20:] if store.hydration_readings else []


@app.get("/api/v1/hydration/history")
async def get_hydration_history(hours: int = 24):
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    return [r.model_dump() for r in store.hydration_readings
            if r.timestamp.timestamp() > cutoff]


# ─── Humidity ───────────────────────────────────────────────────────────────

@app.get("/api/v1/humidity", response_model=list[HumidityReading])
async def get_humidity():
    return store.humidity_readings[-20:] if store.humidity_readings else []


@app.post("/api/v1/humidifier/control")
async def control_humidifier(action: str = "on"):
    """action: 'on' or 'off'"""
    if action not in ("on", "off"):
        raise HTTPException(400, "Action must be 'on' or 'off'")
    cmd = "humidifier_on" if action == "on" else "humidifier_off"
    await mqtt.publish_command("humidity-001", cmd, {})
    await manager.broadcast({"type": "humidifier_command", "action": action})
    return {"status": f"humidifier_{action}"}


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


# ─── Vocal Health ────────────────────────────────────────────────────────────

@app.get("/api/v1/vocal-health", response_model=VocalHealth)
async def get_vocal_health():
    return store.vocal_health


@app.get("/api/v1/voice-disorder-risk", response_model=VoiceDisorderRisk)
async def get_disorder_risk():
    return store.disorder_risk


@app.get("/api/v1/vocal-load", response_model=VocalLoad)
async def get_vocal_load():
    return store.vocal_load


@app.get("/api/v1/voice-quality")
async def get_voice_quality(hours: int = 24):
    cutoff = datetime.now(timezone.utc).timestamp() - hours * 3600
    readings = [r for r in store.room_readings
                if r.timestamp.timestamp() > cutoff]
    counts: dict[str, int] = {}
    for r in readings:
        name = VOICE_QUALITY_NAMES[r.voice_quality_class]
        counts[name] = counts.get(name, 0) + 1
    return {"period": f"{hours}h", "quality_counts": counts}


@app.get("/api/v1/reflux-risk", response_model=RefluxRisk)
async def get_reflux_risk():
    return store.reflux_risk


# ─── ML Predictions ─────────────────────────────────────────────────────────

@app.get("/api/v1/ml/predict/risk", response_model=VoiceRiskForecast)
async def predict_risk():
    """Production: call ML pipeline inference service."""
    if store.risk_forecast is None:
        now = datetime.now(timezone.utc)
        import math
        ts = [now] * 168
        risk = []
        for i in range(168):
            # Risk follows daily pattern + weekly trend
            hour = i % 24
            base = 0.15 + 0.2 * math.exp(-((hour - 14) ** 2) / 8)
            # Weekly trend (increasing if heavy voice use)
            weekly = 0.001 * i
            risk.append(min(1.0, base + weekly))
        store.risk_forecast = VoiceRiskForecast(
            timestamps=ts,
            risk_index=risk,
            confidence_low=[r * 0.85 for r in risk],
            confidence_high=[min(1.0, r * 1.15) for r in risk],
        )
    return store.risk_forecast


@app.get("/api/v1/ml/predict/voice")
async def predict_voice():
    """Voice quality prediction from latest room sentinel data."""
    if store.room_readings:
        r = store.room_readings[-1]
        return {
            "predicted_class": r.voice_quality_class,
            "predicted_name": r.voice_quality_name,
            "confidence": r.confidence_pct,
            "f0_hz": r.f0_hz,
        }
    return {"predicted_class": 0, "predicted_name": "Normal",
            "confidence": 0, "f0_hz": 0}


# ─── Clinical Report ─────────────────────────────────────────────────────────

@app.get("/api/v1/reports/clinical")
async def get_clinical_report():
    """Generate speech-pathologist-ready clinical report."""
    vb = store.vocal_band_readings[-1] if store.vocal_band_readings else None
    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "patient": "Demo User",
        "vocal_health_score": store.vocal_health.score,
        "vocal_health_level": store.vocal_health.level,
        "disorder_risk_score": store.disorder_risk.score,
        "disorder_risk_level": store.disorder_risk.level,
        "vocal_load": {
            "phonation_pct": store.vocal_load.phonation_pct,
            "safe_dose_pct": store.vocal_load.safe_dose_pct,
            "rest_recommended": store.vocal_load.rest_recommended,
        },
        "acoustic_features": {
            "f0_hz": vb.f0_hz if vb else None,
            "jitter_pct": vb.jitter_pct if vb else None,
            "shimmer_pct": vb.shimmer_pct if vb else None,
            "hnr_db": vb.hnr_db if vb else None,
        },
        "clinical_thresholds": {
            "jitter_normal": "<1.04%",
            "jitter_mild": "1.04-2.61%",
            "jitter_moderate": "2.61-4.52%",
            "shimmer_normal": "<3.81%",
            "shimmer_mild": "3.81-7.62%",
            "hnr_normal": ">20 dB",
        },
        "reflux_assessment": {
            "detected": store.reflux_risk.detected,
            "episodes_24h": store.reflux_risk.episodes_24h,
        },
        "recommendations": store.vocal_health.recommendations,
        "notes": "Generated by VoiceSync. For clinical use by a "
                 "speech-language pathologist.",
    }
    return report


# ─── WebSocket ──────────────────────────────────────────────────────────────

@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await manager.connect(ws)
    try:
        while True:
            data = await ws.receive_text()
            msg = json.loads(data)
            if msg.get("type") == "humidifier_on":
                await control_humidifier("on")
            elif msg.get("type") == "humidifier_off":
                await control_humidifier("off")
    except WebSocketDisconnect:
        manager.disconnect(ws)


# ─── Run ────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8080)