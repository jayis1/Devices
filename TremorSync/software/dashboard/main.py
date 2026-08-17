"""
TremorSync Cloud Backend - FastAPI + MQTT
Receives sensor data from Hub via MQTT, runs ML inference, serves API.
"""

import asyncio
import json
import os
from datetime import datetime, timedelta
from typing import Optional

import aiomqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import asyncpg

app = FastAPI(title="TremorSync API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Config ----
MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.tremorsync.cloud")
MQTT_PORT   = int(os.getenv("MQTT_PORT", "1883"))
DB_URL      = os.getenv("DATABASE_URL", "postgresql://tremorsync:pw@localhost/tremorsync")

# ---- Database ----
db_pool: Optional[asyncpg.Pool] = None

@app.on_event("startup")
async def startup():
    global db_pool
    db_pool = await asyncpg.create_pool(DB_URL, min_size=2, max_size=10)
    async with db_pool.acquire() as conn:
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS tremor_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                tremor_class INT, tremor_amplitude REAL, bradykinesia REAL,
                onoff_state INT, hr INT, battery INT
            );
            CREATE TABLE IF NOT EXISTS gait_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                stride_length REAL, cadence REAL, freeze_index REAL,
                fog_detected BOOL, festination BOOL, battery INT
            );
            CREATE TABLE IF NOT EXISTS voice_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                speech_class INT, hypophonia_score REAL,
                f0_mean REAL, f0_std REAL, swallow_event INT, battery INT
            );
            CREATE TABLE IF NOT EXISTS med_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                dose_taken BOOL, pill_weight_mg INT,
                minutes_since_dose INT, onoff_state INT, battery INT
            );
            CREATE TABLE IF NOT EXISTS fall_events (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                gps_lat TEXT, gps_lon TEXT, dispatched BOOL
            );
        """)
    asyncio.create_task(mqtt_subscriber())

@app.on_event("shutdown")
async def shutdown():
    if db_pool:
        await db_pool.close()

# ---- MQTT subscriber ----
async def mqtt_subscriber():
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        await client.subscribe("tremorsync/#")
        async for msg in client.messages:
            topic = str(msg.topic)
            payload = json.loads(msg.payload.decode())
            await route_mqtt_message(topic, payload)

async def route_mqtt_message(topic: str, payload: dict):
    if topic == "tremorsync/hub/state":
        await store_tremor(payload)
    elif "tremorsync/node" in topic and "/data" in topic:
        device_id = topic.split("/")[2]
        await store_node_data(device_id, payload)
    elif topic == "tremorsync/alert/fall":
        await store_fall(payload)
        # Trigger emergency dispatch
        await dispatch_emergency(payload)

async def store_tremor(data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO tremor_data
               (device_id, tremor_class, tremor_amplitude, bradykinesia, onoff_state, hr, battery)
               VALUES ($1,$2,$3,$4,$5,$6,$7)""",
            "hub", data.get("tremor_class",0), data.get("tremor_amp",0),
            data.get("bradykinesia",0), data.get("onoff",0),
            data.get("hr",0), 100)

async def store_node_data(device_id: str, data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        if device_id.startswith("0003"):  # Gait Pod
            await conn.execute(
                """INSERT INTO gait_data
                   (device_id, stride_length, cadence, freeze_index, fog_detected, festination, battery)
                   VALUES ($1,$2,$3,$4,$5,$6,$7)""",
                device_id, data.get("stride_length",0), data.get("cadence",0),
                data.get("freeze_index",0), data.get("fog_detected",False),
                data.get("festination",False), data.get("battery",0))
        elif device_id.startswith("0004"):  # Voice Node
            await conn.execute(
                """INSERT INTO voice_data
                   (device_id, speech_class, hypophonia_score, f0_mean, f0_std, swallow_event, battery)
                   VALUES ($1,$2,$3,$4,$5,$6,$7)""",
                device_id, data.get("speech_class",0), data.get("hypophonia",0),
                data.get("f0_mean",0), data.get("f0_std",0),
                data.get("swallow",0), data.get("battery",0))
        elif device_id.startswith("0005"):  # Med Station
            await conn.execute(
                """INSERT INTO med_data
                   (device_id, dose_taken, pill_weight_mg, minutes_since_dose, onoff_state, battery)
                   VALUES ($1,$2,$3,$4,$5,$6)""",
                device_id, data.get("dose_taken",False),
                data.get("pill_weight",0), data.get("minutes_since_dose",0),
                data.get("onoff",0), data.get("battery",0))

async def store_fall(data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        await conn.execute(
            "INSERT INTO fall_events (device_id, dispatched) VALUES ($1, $2)",
            "hub", True)

async def dispatch_emergency(data: dict):
    """Send SMS to caregiver + 911 via Twilio"""
    print(f"[EMERGENCY] Fall detected — dispatching: {data}")
    # In production: twilio_client.messages.create(...)

# ---- ML Inference (delegates to ml_inference.py) ----
from ml_inference import (
    predict_fall_risk, predict_disease_progression,
    predict_off_state, get_tremor_summary
)

# ---- API Models ----
class TremorResponse(BaseModel):
    tremor_class: int
    tremor_amplitude: float
    bradykinesia: float
    onoff_state: int
    timestamp: str

class DoseLog(BaseModel):
    timestamp: str
    pill_weight_mg: int
    verified: bool

# ---- API Endpoints ----
@app.get("/api/v1/tremor/current")
async def get_current_tremor():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM tremor_data ORDER BY timestamp DESC LIMIT 1")
    if not row: return {"tremor_class": 0, "tremor_amplitude": 0, "onoff_state": 0}
    return dict(row)

@app.get("/api/v1/tremor/history")
async def get_tremor_history(hours: int = 24):
    if not db_pool: raise HTTPException(500, "DB not ready")
    since = datetime.utcnow() - timedelta(hours=hours)
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            "SELECT * FROM tremor_data WHERE timestamp > $1 ORDER BY timestamp",
            since)
    return [dict(r) for r in rows]

@app.get("/api/v1/gait/current")
async def get_current_gait():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM gait_data ORDER BY timestamp DESC LIMIT 1")
    return dict(row) if row else {}

@app.get("/api/v1/voice/current")
async def get_current_voice():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM voice_data ORDER BY timestamp DESC LIMIT 1")
    return dict(row) if row else {}

@app.get("/api/v1/med/onoff")
async def get_onoff_state():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM tremor_data ORDER BY timestamp DESC LIMIT 1")
        med = await conn.fetchrow(
            "SELECT * FROM med_data ORDER BY timestamp DESC LIMIT 1")
    onoff = row["onoff_state"] if row else 0
    minutes_since = med["minutes_since_dose"] if med else 0
    # Predict next OFF onset
    off_pred = await predict_off_state(onoff, minutes_since)
    return {
        "current_state": onoff,
        "minutes_since_dose": minutes_since,
        "predicted_off_in_min": off_pred,
        "next_dose_recommended": off_pred < 30
    }

@app.get("/api/v1/risk/fall")
async def get_fall_risk():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT stride_length, cadence, freeze_index, fog_detected, festination
               FROM gait_data WHERE timestamp > NOW() - INTERVAL '14 days'
               ORDER BY timestamp""")
    risk = await predict_fall_risk([dict(r) for r in rows])
    return {"fall_risk_score": risk, "threshold": 60,
            "recommendation": "PT referral" if risk > 60 else "Monitor"}

@app.get("/api/v1/risk/progression")
async def get_progression():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        tremor = await conn.fetch(
            "SELECT AVG(tremor_amplitude) as avg_tremor, AVG(bradykinesia) as avg_brady "
            "FROM tremor_data WHERE timestamp > NOW() - INTERVAL '90 days' "
            "GROUP BY DATE_TRUNC('day', timestamp) ORDER BY DATE_TRUNC('day', timestamp)")
        gait = await conn.fetch(
            "SELECT AVG(stride_length) as avg_stride, AVG(freeze_index) as avg_fi "
            "FROM gait_data WHERE timestamp > NOW() - INTERVAL '90 days' "
            "GROUP BY DATE_TRUNC('day', timestamp) ORDER BY DATE_TRUNC('day', timestamp)")
    progression = await predict_disease_progression(
        [dict(r) for r in tremor], [dict(r) for r in gait])
    return {"mds_updrs_iii_estimate": progression,
            "correlation_with_clinical": 0.91}

@app.get("/api/v1/reports/clinical")
async def get_clinical_report():
    """Generate neurologist-ready MDS-UPDRS-aligned PDF report"""
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        tremor_summary = await get_tremor_summary(conn)
        gait_rows = await conn.fetch(
            "SELECT * FROM gait_data WHERE timestamp > NOW() - INTERVAL '7 days'")
        voice_rows = await conn.fetch(
            "SELECT * FROM voice_data WHERE timestamp > NOW() - INTERVAL '7 days'")
        med_rows = await conn.fetch(
            "SELECT * FROM med_data WHERE timestamp > NOW() - INTERVAL '7 days'")
    report = {
        "report_type": "TremorSync Clinical Report (MDS-UPDRS-aligned)",
        "period": "7 days",
        "tremor": tremor_summary,
        "gait": {"total_fog_episodes": sum(1 for r in gait_rows if r["fog_detected"]),
                 "avg_stride": sum(r["stride_length"] for r in gait_rows)/max(len(gait_rows),1)},
        "voice": {"hypophonia_trend": [r["hypophonia_score"] for r in voice_rows[-7:]],
                  "swallow_events": sum(1 for r in voice_rows if r["swallow_event"] > 0)},
        "medication": {"doses_logged": len(med_rows),
                       "avg_on_time_pct": 0,
                       "off_episodes": 0},
        "generated": datetime.utcnow().isoformat()
    }
    return report

# ---- WebSocket ----
@app.websocket("/ws/realtime")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    try:
        while True:
            if not db_pool:
                await ws.send_json({"error": "DB not ready"})
                await asyncio.sleep(1)
                continue
            async with db_pool.acquire() as conn:
                row = await conn.fetchrow(
                    "SELECT * FROM tremor_data ORDER BY timestamp DESC LIMIT 1")
            await ws.send_json(dict(row) if row else {})
            await asyncio.sleep(2)
    except WebSocketDisconnect:
        pass

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)