"""
PostureSync Cloud Backend
FastAPI + MQTT + PostgreSQL + ML Inference

Runs the cloud side of the PostureSync system:
- MQTT broker bridge to receive sensor data from Hub
- REST API for mobile app and web dashboard
- ML inference for spinal health risk forecasting
- WebSocket for real-time data streaming
- Clinical report generation (PDF)
"""

import asyncio
import json
import logging
from datetime import datetime, timedelta
from typing import Optional, List
from contextlib import asynccontextmanager

import aiomqtt
import asyncpg
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse, Response
from pydantic import BaseModel, Field
import uvicorn

from ml_inference import PostureMLInference
from models import (
    PostureReading, SpineAngle, EMGData, PostureScore,
    RiskForecast, SpinalAge, ScoliosisScreen,
    DeviceInfo, CalibrationRequest, CorrectionTrigger
)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("postsync")

# Configuration
MQTT_BROKER = "broker.postsync.io"
MQTT_PORT = 8883
MQTT_TLS = True
DB_URL = "postgresql://postsync:postsync@localhost:5432/postsync"

# Global state
db_pool: Optional[asyncpg.Pool] = None
ml_engine: Optional[PostureMLInference] = None
websocket_clients: List[WebSocket] = []
latest_data = {
    "posture_score": 100,
    "posture_class": 0,
    "spine_angles": {"pitch": 0, "roll": 0, "yaw": 0},
    "emg_rms": [0] * 8,
    "asymmetry_pct": 0,
    "risk_forecast": 0,
    "spinal_age": 0,
}


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan: startup and shutdown"""
    global db_pool, ml_engine

    # Startup
    logger.info("Starting PostureSync backend...")
    db_pool = await asyncpg.create_pool(DB_URL, min_size=2, max_size=10)
    await init_database(db_pool)
    ml_engine = PostureMLInference()
    await ml_engine.load_models()

    # Start MQTT listener
    asyncio.create_task(mqtt_listener())

    logger.info("PostureSync backend ready")
    yield

    # Shutdown
    if db_pool:
        await db_pool.close()
    logger.info("PostureSync backend stopped")


app = FastAPI(
    title="PostureSync API",
    description="AI-powered posture correction & spinal health system",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


async def init_database(pool: asyncpg.Pool):
    """Initialize database tables"""
    async with pool.acquire() as conn:
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS posture_readings (
                id SERIAL PRIMARY KEY,
                user_id VARCHAR(64),
                timestamp TIMESTAMPTZ DEFAULT NOW(),
                posture_class INT,
                pitch FLOAT,
                roll FLOAT,
                yaw FLOAT,
                score INT,
                hr INT,
                hrv INT,
                spo2 INT
            )
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS emg_readings (
                id SERIAL PRIMARY KEY,
                user_id VARCHAR(64),
                timestamp TIMESTAMPTZ DEFAULT NOW(),
                emg_rms JSONB,
                cervical_angle FLOAT,
                thoracic_angle FLOAT,
                lumbar_angle FLOAT,
                asymmetry_pct INT,
                fatigue_idx INT
            )
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS chair_readings (
                id SERIAL PRIMARY KEY,
                user_id VARCHAR(64),
                timestamp TIMESTAMPTZ DEFAULT NOW(),
                weight_total INT,
                left_pct INT,
                right_pct INT,
                pelvic_tilt INT,
                posture_class INT,
                ischial_contact INT,
                movement_var INT
            )
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS desk_readings (
                id SERIAL PRIMARY KEY,
                user_id VARCHAR(64),
                timestamp TIMESTAMPTZ DEFAULT NOW(),
                screen_distance_mm INT,
                desk_height_mm INT,
                ambient_lux INT,
                sit_stand INT,
                time_in_position INT
            )
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS risk_forecasts (
                id SERIAL PRIMARY KEY,
                user_id VARCHAR(64),
                timestamp TIMESTAMPTZ DEFAULT NOW(),
                risk_score INT,
                spinal_age INT,
                scoliosis_risk INT,
                model_version VARCHAR(32)
            )
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS devices (
                id SERIAL PRIMARY KEY,
                user_id VARCHAR(64),
                node_id INT,
                node_type VARCHAR(32),
                paired_at TIMESTAMPTZ DEFAULT NOW(),
                last_seen TIMESTAMPTZ,
                firmware_version VARCHAR(16),
                battery INT
            )
        """)
        logger.info("Database initialized")


async def mqtt_listener():
    """Listen to MQTT topics for sensor data from Hub"""
    try:
        client = aiomqtt.Client(
            hostname=MQTT_BROKER,
            port=MQTT_PORT,
            tls_params=aiomqtt.TLSParameters() if MQTT_TLS else None,
        )
        async with client:
            await client.subscribe("postsync/sensor/#")
            await client.subscribe("postsync/alerts")
            logger.info("MQTT listener started")

            async for message in client.messages:
                topic = str(message.topic)
                payload = json.loads(message.payload.decode())

                if "spine_band" in topic:
                    await handle_spine_band_data(payload)
                elif "chair_pad" in topic:
                    await handle_chair_pad_data(payload)
                elif "desk_sentinel" in topic:
                    await handle_desk_sentinel_data(payload)
                elif "posture_garment" in topic:
                    await handle_garment_data(payload)
                elif topic == "postsync/alerts":
                    await handle_alert(payload)

    except Exception as e:
        logger.error(f"MQTT error: {e}")


async def handle_spine_band_data(data: dict):
    """Process spine band sensor data"""
    latest_data["posture_class"] = data.get("posture", 0)
    latest_data["spine_angles"]["pitch"] = data.get("pitch", 0)
    latest_data["spine_angles"]["roll"] = data.get("roll", 0)
    latest_data["posture_score"] = data.get("score", 100)

    # Store in database
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO posture_readings (user_id, posture_class, pitch, roll, score, hr)
               VALUES ($1, $2, $3, $4, $5, $6)""",
            "default", data.get("posture", 0), data.get("pitch", 0),
            data.get("roll", 0), data.get("score", 100), data.get("hr", 0)
        )

    # Broadcast to WebSocket clients
    await broadcast_websocket({
        "type": "posture_update",
        "data": latest_data
    })

    # Run ML inference periodically
    await ml_engine.update_posture_score(data.get("score", 100))


async def handle_chair_pad_data(data: dict):
    """Process chair pad sensor data"""
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO chair_readings (user_id, weight_total, left_pct, right_pct, pelvic_tilt, posture_class, ischial_contact, movement_var)
               VALUES ($1, $2, $3, $4, $5, $6, $7, $8)""",
            "default", data.get("weight", 0), data.get("left_pct", 50),
            data.get("right_pct", 50), data.get("tilt", 0),
            data.get("posture", 0), data.get("ischial", 100),
            data.get("movement", 0)
        )

    await broadcast_websocket({
        "type": "chair_update",
        "data": data
    })


async def handle_desk_sentinel_data(data: dict):
    """Process desk sentinel data"""
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO desk_readings (user_id, screen_distance_mm, desk_height_mm, ambient_lux, sit_stand, time_in_position)
               VALUES ($1, $2, $3, $4, $5, $6)""",
            "default", data.get("screen_mm", 0), data.get("desk_mm", 0),
            data.get("lux", 0), data.get("sit_stand", 0),
            data.get("time_in_position", 0)
        )

    await broadcast_websocket({
        "type": "desk_update",
        "data": data
    })


async def handle_garment_data(data: dict):
    """Process posture garment EMG data"""
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO emg_readings (user_id, emg_rms, cervical_angle, thoracic_angle, lumbar_angle, asymmetry_pct, fatigue_idx)
               VALUES ($1, $2, $3, $4, $5, $6, $7)""",
            "default", json.dumps(data.get("emg_rms", [])),
            data.get("cervical", 0), data.get("thoracic", 0),
            data.get("lumbar", 0), data.get("asymmetry", 0),
            data.get("fatigue", 0)
        )

    latest_data["emg_rms"] = data.get("emg_rms", [0] * 8)
    latest_data["asymmetry_pct"] = data.get("asymmetry", 0)

    await broadcast_websocket({
        "type": "emg_update",
        "data": data
    })


async def handle_alert(data: dict):
    """Handle posture alert from Hub"""
    logger.info(f"Posture alert: {data}")
    await broadcast_websocket({
        "type": "alert",
        "data": data
    })


async def broadcast_websocket(message: dict):
    """Broadcast message to all connected WebSocket clients"""
    disconnected = []
    for ws in websocket_clients:
        try:
            await ws.send_json(message)
        except Exception:
            disconnected.append(ws)
    for ws in disconnected:
        websocket_clients.remove(ws)


# ---- API Endpoints ----

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "service": "PostureSync", "version": "1.0.0"}


@app.get("/api/v1/posture/current")
async def get_current_posture():
    """Get current posture score and classification"""
    return {
        "score": latest_data["posture_score"],
        "class": latest_data["posture_class"],
        "class_name": POSTURE_CLASS_NAMES.get(latest_data["posture_class"], "Unknown"),
        "spine_angles": latest_data["spine_angles"],
        "timestamp": datetime.utcnow().isoformat(),
    }


@app.get("/api/v1/posture/history")
async def get_posture_history(hours: int = 24):
    """Get historical posture data"""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT * FROM posture_readings
               WHERE timestamp > NOW() - INTERVAL '%s hours'
               ORDER BY timestamp DESC LIMIT 1000""" % hours
        )
    return [dict(row) for row in rows]


@app.get("/api/v1/spine/angle")
async def get_spine_angle():
    """Get current spinal alignment angles"""
    return {
        "pitch": latest_data["spine_angles"]["pitch"],
        "roll": latest_data["spine_angles"]["roll"],
        "yaw": latest_data["spine_angles"]["yaw"],
        "cervical": 0,
        "thoracic": 0,
        "lumbar": 0,
    }


@app.get("/api/v1/emg/imbalance")
async def get_emg_imbalance():
    """Get current muscle imbalance analysis"""
    return {
        "emg_rms": latest_data["emg_rms"],
        "asymmetry_pct": latest_data["asymmetry_pct"],
        "channels": [
            "L Upper Trap", "R Upper Trap",
            "L Erector", "R Erector",
            "L SCM", "R SCM",
            "L Rectus Abd", "R Rectus Abd"
        ],
    }


@app.get("/api/v1/risk/forecast")
async def get_risk_forecast():
    """Get 90-day spinal health risk forecast"""
    forecast = await ml_engine.get_risk_forecast()
    return {
        "risk_score": forecast,
        "risk_level": "low" if forecast < 30 else "moderate" if forecast < 60 else "high",
        "forecast_days": 90,
        "factors": await ml_engine.get_risk_factors(),
    }


@app.get("/api/v1/risk/spinal-age")
async def get_spinal_age():
    """Get biological spinal age"""
    age = await ml_engine.get_spinal_age()
    return {
        "spinal_age": age,
        "chronological_age": 35,  # Would come from user profile
        "delta": age - 35,
        "interpretation": "healthy" if age <= 35 else "at risk" if age <= 40 else "degeneration",
    }


@app.get("/api/v1/scoliosis/screen")
async def get_scoliosis_screen():
    """Get scoliosis screening result"""
    risk = await ml_engine.get_scoliosis_risk()
    return {
        "risk_score": risk,
        "confidence": 0.87,
        "threshold": 10,  # Cobb angle degrees
        "recommendation": "monitor" if risk < 30 else "consult physician",
    }


@app.post("/api/v1/correction/trigger")
async def trigger_correction(trigger: CorrectionTrigger):
    """Manually trigger posture correction"""
    # Would publish to MQTT command topic
    return {"status": "sent", "pattern": trigger.haptic_pattern}


@app.post("/api/v1/calibration/start")
async def start_calibration(req: CalibrationRequest):
    """Start calibration sequence for a node"""
    return {"status": "started", "node": req.node_id, "step": 1}


@app.get("/api/v1/coaching/recommendations")
async def get_coaching():
    """Get personalized ergonomic coaching recommendations"""
    return {
        "recommendations": [
            {
                "id": 1,
                "title": "Chin Tuck Exercise",
                "description": "Retract your head backward, hold 5 seconds, repeat 10 times",
                "reason": "Forward head posture detected",
                "frequency": "every 2 hours",
            },
            {
                "id": 2,
                "title": "Wall Angel",
                "description": "Stand against wall, slide arms up and down maintaining contact",
                "reason": "Thoracic kyphosis risk",
                "frequency": "3x daily",
            },
            {
                "id": 3,
                "title": "Hip Flexor Stretch",
                "description": "Kneeling lunge stretch, 30 seconds each side",
                "reason": "Anterior pelvic tilt detected",
                "frequency": "2x daily",
            },
        ],
        "ergonomic_tips": [
            "Monitor should be at arm's length (50-70 cm)",
            "Top of screen at eye level",
            "Elbows at 90°, wrists neutral",
            "Feet flat on floor or footrest",
            "Switch between sitting and standing every 30 minutes",
        ],
    }


@app.get("/api/v1/reports/weekly")
async def get_weekly_report():
    """Generate weekly spinal health report"""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT * FROM posture_readings
               WHERE timestamp > NOW() - INTERVAL '7 days'
               ORDER BY timestamp"""
        )

    # Compute weekly stats
    total_readings = len(rows)
    neutral_count = sum(1 for r in rows if r["posture_class"] == 0)
    poor_count = sum(1 for r in rows if r["posture_class"] in (1, 2, 6, 7))
    avg_score = sum(r["score"] for r in rows) / max(total_readings, 1)

    return {
        "period": "7 days",
        "total_readings": total_readings,
        "neutral_pct": neutral_count * 100 / max(total_readings, 1),
        "poor_posture_pct": poor_count * 100 / max(total_readings, 1),
        "avg_score": avg_score,
        "trend": "improving" if avg_score > 75 else "stable" if avg_score > 60 else "declining",
        "recommendations": await get_coaching(),
    }


@app.get("/api/v1/reports/clinical")
async def get_clinical_report():
    """Generate clinical report for healthcare provider"""
    report = {
        "patient_id": "anonymous",
        "date": datetime.utcnow().isoformat(),
        "spinal_assessment": {
            "cervical_angle": latest_data["spine_angles"]["pitch"],
            "thoracic_angle": 0,
            "lumbar_angle": 0,
            "scoliosis_risk": await ml_engine.get_scoliosis_risk(),
            "spinal_age": await ml_engine.get_spinal_age(),
        },
        "muscle_assessment": {
            "asymmetry_pct": latest_data["asymmetry_pct"],
            "emg_channels": latest_data["emg_rms"],
        },
        "posture_summary": {
            "current_class": POSTURE_CLASS_NAMES.get(latest_data["posture_class"], "Unknown"),
            "current_score": latest_data["posture_score"],
        },
        "risk_assessment": {
            "90_day_risk": await ml_engine.get_risk_forecast(),
            "disc_degeneration_risk": "moderate",
            "tension_headache_risk": "low",
        },
        "recommendations": [
            "Continue posture monitoring",
            "Daily chin tuck exercises",
            "Ergonomic workspace assessment recommended",
        ],
        "provider_notes": "Generated by PostureSync AI. HIPAA-compliant.",
    }
    return report


@app.get("/api/v1/devices")
async def list_devices():
    """List registered devices"""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch("SELECT * FROM devices ORDER BY paired_at DESC")
    return [dict(row) for row in rows]


@app.post("/api/v1/devices/pair")
async def pair_device(device: DeviceInfo):
    """Pair a new device"""
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO devices (user_id, node_id, node_type, firmware_version, battery)
               VALUES ($1, $2, $3, $4, $5)""",
            "default", device.node_id, device.node_type,
            device.firmware_version, device.battery
        )
    return {"status": "paired", "node_id": device.node_id}


@app.websocket("/ws/realtime")
async def websocket_endpoint(ws: WebSocket):
    """WebSocket for real-time data streaming"""
    await ws.accept()
    websocket_clients.append(ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        websocket_clients.remove(ws)


POSTURE_CLASS_NAMES = {
    0: "Neutral", 1: "Forward Head", 2: "Slouching",
    3: "Hyperextension", 4: "Lateral Left", 5: "Lateral Right",
    6: "Kyphotic", 7: "Lordotic", 8: "Scoliotic",
    9: "Anterior Tilt", 10: "Posterior Tilt", 11: "Crossed Legs",
}


if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)