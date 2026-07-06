"""
SightSync Cloud Backend — FastAPI Application
==============================================

Endpoints: fatigue, distance, blink, light, myopia forecast,
optometrist reports, lamp control, MQTT webhook.

License: MIT
"""

import os
import json
import time
import uuid
from datetime import datetime, timedelta
from typing import Optional

import asyncpg
import paho.mqtt.client as mqtt
from fastapi import FastAPI, HTTPException, Depends, Header
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import Response
from pydantic import BaseModel

# ── Configuration ────────────────────────────────────────────────────

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://sightsync:sightsync@localhost:5432/sightsync")
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
JWT_SECRET = os.getenv("JWT_SECRET", "sightsync-secret-key-change-me")

# ── FastAPI App ──────────────────────────────────────────────────────

app = FastAPI(
    title="SightSync Cloud API",
    version="1.0.0",
    description="AI-powered eye health & digital eye-strain prevention system",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ── Database Pool ─────────────────────────────────────────────────────

_db_pool: Optional[asyncpg.Pool] = None


async def get_db():
    global _db_pool
    if _db_pool is None:
        _db_pool = asyncpg.create_pool(DATABASE_URL, min_size=2, max_size=10)
        await _db_pool
    return _db_pool


# ── MQTT Client ──────────────────────────────────────────────────────

mqtt_client = mqtt.Client()


def on_mqtt_connect(client, userdata, flags, rc):
    client.subscribe("sightsync/+/hub/#")


def on_mqtt_message(client, userdata, msg):
    """Forward MQTT messages to the API via internal HTTP webhook."""
    topic = msg.topic
    payload = json.loads(msg.payload.decode())
    # In production: insert into TimescaleDB directly
    print(f"MQTT: {topic} → {payload}")


mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message


@app.on_event("startup")
async def startup():
    try:
        mqtt_client.connect(MQTT_HOST, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"MQTT connect failed: {e}")


@app.on_event("shutdown")
async def shutdown():
    mqtt_client.loop_stop()
    mqtt_client.disconnect()


# ── Pydantic Models ──────────────────────────────────────────────────

class FatigueReading(BaseModel):
    fatigue: int
    blink: int
    distance: int
    lux: int
    blue_dose: int
    posture_risk: int
    dry_eye: int
    minutes_since_break: int


class LampOverride(BaseModel):
    cct: int
    brightness: int
    duration_min: int = 30


class UserRegister(BaseModel):
    email: str
    password: str
    name: str


class UserLogin(BaseModel):
    email: str
    password: str


# ── Endpoints ─────────────────────────────────────────────────────────

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "timestamp": datetime.utcnow().isoformat()}


@app.get("/api/v1/fatigue/current")
async def get_current_fatigue(authorization: Optional[str] = Header(None)):
    """Get current visual fatigue index."""
    # TODO: query latest from TimescaleDB
    return {
        "fatigue_score": 42,
        "alert_level": 1,
        "blink_rate": 12,
        "viewing_distance_mm": 450,
        "ambient_lux": 520,
        "minutes_since_break": 15,
        "timestamp": datetime.utcnow().isoformat(),
    }


@app.get("/api/v1/fatigue/history")
async def get_fatigue_history(days: int = 7, authorization: Optional[str] = Header(None)):
    """Get fatigue history."""
    # TODO: query TimescaleDB hypertable
    data = []
    for i in range(days * 24):
        ts = datetime.utcnow() - timedelta(hours=i)
        data.append({
            "timestamp": ts.isoformat(),
            "fatigue_score": 30 + (i % 40),
            "blink_rate": 8 + (i % 12),
            "viewing_distance_mm": 400 + (i % 200),
            "ambient_lux": 450 + (i % 200),
        })
    return {"data": data, "days": days}


@app.get("/api/v1/distance/history")
async def get_distance_history(days: int = 1, authorization: Optional[str] = Header(None)):
    """Get viewing-distance history."""
    data = []
    for i in range(days * 24 * 60):  # 1-minute resolution
        ts = datetime.utcnow() - timedelta(minutes=i)
        dist = 400 + (i % 300)
        data.append({
            "timestamp": ts.isoformat(),
            "distance_mm": dist,
            "near_work_flag": 1 if dist < 300 else 0,
        })
    return {
        "data": data[-1000:],
        "summary": {
            "avg_distance_mm": 450,
            "near_work_minutes": 95,
            "too_close_events": 3,
        },
    }


@app.get("/api/v1/blink/history")
async def get_blink_history(days: int = 1, authorization: Optional[str] = Header(None)):
    """Get blink-rate history."""
    data = []
    for i in range(days * 24 * 6):  # 10-minute resolution
        ts = datetime.utcnow() - timedelta(minutes=i * 10)
        data.append({
            "timestamp": ts.isoformat(),
            "bpm": 5 + (i % 15),
            "confidence": 70 + (i % 30),
        })
    return {
        "data": data[-500:],
        "summary": {
            "avg_bpm": 9.2,
            "min_bpm": 4,
            "low_blink_events": 7,
        },
    }


@app.get("/api/v1/light/history")
async def get_light_history(days: int = 1, authorization: Optional[str] = Header(None)):
    """Get ambient + blue light exposure history."""
    data = []
    for i in range(days * 24 * 2):  # 30-minute resolution
        ts = datetime.utcnow() - timedelta(minutes=i * 30)
        data.append({
            "timestamp": ts.isoformat(),
            "lux": 300 + (i % 400),
            "blue_mw": 200 + (i % 300),
            "cct": 3000 + (i % 3500),
        })
    return {
        "data": data[-500:],
        "summary": {
            "avg_lux": 480,
            "insufficient_light_minutes": 45,
            "blue_dose_mj_cm2": 8.2,
        },
    }


@app.get("/api/v1/myopia/forecast")
async def get_myopia_forecast(child_id: Optional[str] = None,
                               authorization: Optional[str] = Header(None)):
    """Get 90-day myopia progression forecast."""
    # TODO: call ONNX LSTM model for real prediction
    return {
        "risk_30day": 25,
        "risk_90day": 38,
        "refractive_delta_diopter": -0.12,
        "near_work_today_min": 95,
        "outdoor_today_min": 35,
        "avg_distance_mm": 420,
        "recommendation": "more_outdoor",
        "timestamp": datetime.utcnow().isoformat(),
    }


@app.get("/api/v1/report/optometrist")
async def get_optometrist_report(format: str = "json",
                                  authorization: Optional[str] = Header(None)):
    """Generate optometrist-ready clinical report."""
    if format == "json":
        return {
            "report_date": datetime.utcnow().isoformat(),
            "patient": {"name": "Demo User", "age": 32},
            "visual_hygiene_score": 72,
            "daily_fatigue_avg": 38,
            "blink_rate_avg": 9.1,
            "near_work_daily_avg_min": 105,
            "outdoor_light_daily_avg_min": 28,
            "viewing_distance_avg_mm": 440,
            "blue_light_dose_daily_avg": 7.8,
            "forward_head_posture_pct": 42,
            "20_20_20_compliance_pct": 65,
            "dry_eye_risk_avg": 28,
            "myopia_risk_90day": 38,
            "recommendations": [
                "Increase outdoor time to ≥2 hours/day",
                "Reduce continuous near-work to <45 minutes per session",
                "Improve ambient lighting (current avg: 480 lux, target: ≥500)",
                "Practice 20-20-20 rule more consistently (current: 65%)",
                "Consider lubricating eye drops for dry-eye symptoms",
            ],
        }
    elif format == "pdf":
        # TODO: generate PDF with reportlab
        content = b"%PDF-1.4\n%SightSync Optometrist Report\n%%EOF"
        return Response(content=content, media_type="application/pdf",
                       headers={"Content-Disposition": "attachment; filename=sightsync_report.pdf"})
    raise HTTPException(status_code=400, detail="Unsupported format")


@app.get("/api/v1/lamp/policy")
async def get_lamp_policy(authorization: Optional[str] = Header(None)):
    """Get current circadian DQN lamp policy."""
    return {
        "mode": "circadian",
        "schedule": [
            {"hour": 6, "cct": 3000, "brightness": 60},
            {"hour": 10, "cct": 5500, "brightness": 80},
            {"hour": 14, "cct": 5500, "brightness": 80},
            {"hour": 18, "cct": 3500, "brightness": 55},
            {"hour": 22, "cct": 1800, "brightness": 15},
        ],
    }


@app.post("/api/v1/lamp/override")
async def lamp_override(override: LampOverride,
                         authorization: Optional[str] = Header(None)):
    """Send manual lamp override command via MQTT."""
    payload = json.dumps({
        "cct": override.cct,
        "brightness": override.brightness,
        "duration_min": override.duration_min,
    })
    mqtt_client.publish("sightsync/cloud/lamp_cmd", payload)
    return {"status": "ok", "cct": override.cct, "brightness": override.brightness}


@app.post("/api/v1/auth/register")
async def register(user: UserRegister):
    """Register a new user."""
    user_id = str(uuid.uuid4())
    # TODO: hash password, store in DB
    token = f"jwt_{user_id}_{int(time.time())}"  # simplified
    return {"user_id": user_id, "token": token}


@app.post("/api/v1/auth/login")
async def login(user: UserLogin):
    """Authenticate user."""
    # TODO: verify password, generate JWT
    user_id = str(uuid.uuid4())
    token = f"jwt_{user_id}_{int(time.time())}"
    return {"user_id": user_id, "token": token}


@app.post("/api/v1/mqtt/inbound")
async def mqtt_inbound(topic: str, payload: dict):
    """MQTT webhook — hub data ingestion."""
    # TODO: insert into TimescaleDB
    return {"status": "ok", "topic": topic}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)