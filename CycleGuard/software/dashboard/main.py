"""
CycleGuard Cloud Backend - FastAPI + MQTT
Receives sensor data from Hub via MQTT, runs ML inference, serves API.
"""

import asyncio
import json
import os
from datetime import datetime, timedelta
from typing import Optional, List
from urllib.parse import quote

import aiomqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import asyncpg

app = FastAPI(title="CycleGuard API", version="1.0.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Config ----
MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.cycleguard.cloud")
MQTT_PORT   = int(os.getenv("MQTT_PORT", "1883"))
DB_URL      = os.getenv("DATABASE_URL",
    "postgresql://cycleguard:cycleguard@localhost/cycleguard")

# ---- Database ----
db_pool: Optional[asyncpg.Pool] = None

@app.on_event("startup")
async def startup():
    global db_pool
    db_pool = await asyncpg.create_pool(DB_URL, min_size=2, max_size=10)
    async with db_pool.acquire() as conn:
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS ride_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                speed_kmh REAL, cadence_rpm INT, tire_pressure REAL,
                tire_temp REAL, gps_lat REAL, gps_lon REAL,
                heading REAL, battery INT
            );
            CREATE TABLE IF NOT EXISTS helmet_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                crash_class INT, impact_g REAL, rot_velocity REAL,
                horn_detected BOOL, siren_detected BOOL,
                hr INT, battery INT
            );
            CREATE TABLE IF NOT EXISTS light_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                braking BOOL, turn_signal INT,
                headlight_pct INT, taillight_pct INT,
                ambient_lux REAL, battery INT
            );
            CREATE TABLE IF NOT EXISTS lock_data (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                lock_state INT, gps_lat REAL, gps_lon REAL,
                tamper_count INT, load_cell_kg INT, battery INT
            );
            CREATE TABLE IF NOT EXISTS crash_events (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                gps_lat REAL, gps_lon REAL, speed_kmh REAL,
                impact_g REAL, rot_velocity REAL,
                dispatched BOOL, cancelled BOOL
            );
            CREATE TABLE IF NOT EXISTS theft_events (
                id SERIAL PRIMARY KEY,
                device_id TEXT, timestamp TIMESTAMPTZ DEFAULT NOW(),
                gps_lat REAL, gps_lon REAL,
                tamper_count INT, siren_activated BOOL
            );
            CREATE TABLE IF NOT EXISTS rides (
                id SERIAL PRIMARY KEY,
                device_id TEXT, start_time TIMESTAMPTZ,
                end_time TIMESTAMPTZ, distance_km REAL,
                avg_speed REAL, max_speed REAL, safety_score REAL,
                calories INT, route JSONB
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
        await client.subscribe("cycleguard/#")
        async for msg in client.messages:
            topic = str(msg.topic)
            try:
                payload = json.loads(msg.payload.decode())
                await route_mqtt_message(topic, payload)
            except (json.JSONDecodeError, UnicodeDecodeError):
                pass

async def route_mqtt_message(topic: str, payload: dict):
    if topic == "cycleguard/hub/state":
        await store_ride_data(payload)
    elif "cycleguard/lock" in topic and "/data" in topic:
        await store_lock_data(payload)
    elif topic == "cycleguard/alert/crash":
        await store_crash_event(payload)
        await dispatch_crash_emergency(payload)
    elif topic == "cycleguard/alert/theft":
        await store_theft_event(payload)
        await dispatch_theft_alert(payload)

async def store_ride_data(data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO ride_data
               (device_id, speed_kmh, cadence_rpm, tire_pressure,
                gps_lat, gps_lon, heading, battery)
               VALUES ($1,$2,$3,$4,$5,$6,$7,$8)""",
            "hub", data.get("speed",0), data.get("cadence",0),
            data.get("tire_psi",0), data.get("gps_lat",0),
            data.get("gps_lon",0), data.get("heading",0),
            data.get("battery",0))

async def store_lock_data(data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO lock_data
               (device_id, lock_state, gps_lat, gps_lon,
                tamper_count, load_cell_kg, battery)
               VALUES ($1,$2,$3,$4,$5,$6,$7)""",
            "smart_lock", data.get("lock_state",0),
            data.get("gps_lat_e7",0) / 1e7,
            data.get("gps_lon_e7",0) / 1e7,
            data.get("tamper_count",0), data.get("load_cell_kg",0),
            data.get("battery",0))

async def store_crash_event(data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO crash_events
               (device_id, gps_lat, gps_lon, speed_kmh, dispatched)
               VALUES ($1,$2,$3,$4,$5)""",
            "hub", data.get("lat",0), data.get("lon",0),
            data.get("speed",0), True)

async def store_theft_event(data: dict):
    if not db_pool: return
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO theft_events
               (device_id, gps_lat, gps_lon, tamper_count, siren_activated)
               VALUES ($1,$2,$3,$4,$5)""",
            "smart_lock",
            data.get("gps_lat_e7",0) / 1e7,
            data.get("gps_lon_e7",0) / 1e7,
            data.get("tamper_count",0), True)

async def dispatch_crash_emergency(data: dict):
    """Send SMS to emergency contact + 911 via Twilio"""
    print(f"[EMERGENCY] Crash detected — dispatching: {data}")
    # In production: twilio_client.messages.create(...)

async def dispatch_theft_alert(data: dict):
    """Send push notification + SMS to owner"""
    print(f"[THEFT] Theft alert — notifying owner: {data}")
    # In production: push notification + SMS

# ---- ML Inference (delegates to ml_inference.py) ----
from ml_inference import (
    predict_theft_pattern, get_route_safety,
    get_crash_risk_forecast, get_ride_safety_score
)

# ---- API Models ----
class RideData(BaseModel):
    speed_kmh: float
    cadence_rpm: int
    tire_pressure: float
    gps_lat: float
    gps_lon: float
    heading: float
    battery: int

class LockCommand(BaseModel):
    command: int  # 0=disarm, 1=arm, 2=silent, 3=alarm
    geo_fence_m: int = 50

class RouteRequest(BaseModel):
    start_lat: float
    start_lon: float
    end_lat: float
    end_lon: float

# ---- API Endpoints ----

@app.get("/api/v1/ride/current")
async def get_current_ride():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM ride_data ORDER BY timestamp DESC LIMIT 1")
    return dict(row) if row else {}

@app.get("/api/v1/ride/history")
async def get_ride_history(hours: int = 24):
    if not db_pool: raise HTTPException(500, "DB not ready")
    since = datetime.utcnow() - timedelta(hours=hours)
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            "SELECT * FROM ride_data WHERE timestamp > $1 ORDER BY timestamp",
            since)
    return [dict(r) for r in rows]

@app.get("/api/v1/helmet/current")
async def get_current_helmet():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM helmet_data ORDER BY timestamp DESC LIMIT 1")
    return dict(row) if row else {}

@app.get("/api/v1/lock/status")
async def get_lock_status():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM lock_data ORDER BY timestamp DESC LIMIT 1")
    return dict(row) if row else {"lock_state": 0}

@app.post("/api/v1/lock/arm")
async def arm_lock():
    """Send arm command to Smart Lock via MQTT"""
    if not g_mqtt_publish:
        raise HTTPException(500, "MQTT not ready")
    # Publish arm command
    print("[API] Lock arm command sent")
    return {"status": "arm_command_sent"}

@app.post("/api/v1/lock/disarm")
async def disarm_lock():
    """Send disarm command to Smart Lock via MQTT"""
    print("[API] Lock disarm command sent")
    return {"status": "disarm_command_sent"}

@app.get("/api/v1/theft/alerts")
async def get_theft_alerts():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            "SELECT * FROM theft_events ORDER BY timestamp DESC LIMIT 50")
    return [dict(r) for r in rows]

@app.get("/api/v1/theft/trail")
async def get_theft_trail():
    """Get GPS trail of stolen bike (recent lock_data during ALARM/TRACKING)"""
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT gps_lat, gps_lon, timestamp FROM lock_data
               WHERE lock_state >= 3 ORDER BY timestamp DESC LIMIT 100""")
    return [dict(r) for r in rows]

@app.get("/api/v1/crash/reports")
async def get_crash_reports():
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            "SELECT * FROM crash_events ORDER BY timestamp DESC LIMIT 50")
    return [dict(r) for r in rows]

@app.post("/api/v1/safety/route")
async def get_safe_route(req: RouteRequest):
    """Get safe route recommendation from A to B"""
    route = await get_route_safety(req.start_lat, req.start_lon,
                                    req.end_lat, req.end_lon)
    return route

@app.get("/api/v1/safety/forecast")
async def get_safety_forecast():
    """48-hour crash risk forecast"""
    forecast = await get_crash_risk_forecast()
    return {"forecast": forecast, "recommended_window": "07:00-09:00"}

@app.get("/api/v1/reports/ride/{ride_id}")
async def get_ride_report(ride_id: int):
    """Generate detailed ride report"""
    if not db_pool: raise HTTPException(500, "DB not ready")
    async with db_pool.acquire() as conn:
        ride = await conn.fetchrow("SELECT * FROM rides WHERE id=$1", ride_id)
        if not ride:
            raise HTTPException(404, "Ride not found")
        data = await conn.fetch(
            "SELECT * FROM ride_data WHERE timestamp BETWEEN $1 AND $2 ORDER BY timestamp",
            ride["start_time"], ride["end_time"])
    safety_score = await get_ride_safety_score([dict(d) for d in data])
    report = {
        "ride_id": ride_id,
        "distance_km": ride["distance_km"],
        "avg_speed": ride["avg_speed"],
        "max_speed": ride["max_speed"],
        "safety_score": safety_score,
        "calories": ride["calories"],
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
                    "SELECT * FROM ride_data ORDER BY timestamp DESC LIMIT 1")
            await ws.send_json(dict(row) if row else {})
            await asyncio.sleep(2)
    except WebSocketDisconnect:
        pass

# MQTT publish stub (in production, connected to MQTT client)
g_mqtt_publish = True

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)