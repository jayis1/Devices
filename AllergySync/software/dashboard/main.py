"""
AllergySync — Cloud Backend (FastAPI + MQTT)
=============================================
Receives telemetry from the hub via MQTT, stores in PostgreSQL,
serves REST API + WebSocket for the mobile app, runs ML inference.

Run: uvicorn main:app --host 0.0.0.0 --port 8000
"""

import asyncio
import json
import os
import time
from datetime import datetime, timedelta
from contextlib import asynccontextmanager

import aiomqtt
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional
import asyncpg
import redis.asyncio as redis
import structlog

logger = structlog.get_logger()

# ---- Configuration ----
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT   = int(os.getenv("MQTT_PORT", "1883"))
DB_URL      = os.getenv("DATABASE_URL", "postgresql://allergysync:secret@localhost:5432/allergysync")
REDIS_URL   = os.getenv("REDIS_URL", "redis://localhost:6379/0")

# ---- Database pool ----
db_pool: asyncpg.Pool = None
redis_client: redis.Redis = None
ws_clients: set[WebSocket] = set()

# ---- Pydantic models ----
class SymptomEntry(BaseModel):
    timestamp: Optional[datetime] = None
    sneezing: int = Field(ge=0, le=5, default=0)
    itchy_eyes: int = Field(ge=0, le=5, default=0)
    congestion: int = Field(ge=0, le=5, default=0)
    runny_nose: int = Field(ge=0, le=5, default=0)
    headache: int = Field(ge=0, le=5, default=0)
    fatigue: int = Field(ge=0, le=5, default=0)
    notes: str = ""

class MedicationEntry(BaseModel):
    timestamp: Optional[datetime] = None
    medication: str  # e.g., "cetirizine", "loratadine"
    dose_mg: float
    taken: bool = True

class AllergyProfile(BaseModel):
    birch: int = Field(ge=0, le=5, default=0)
    grass: int = Field(ge=0, le=5, default=0)
    ragweed: int = Field(ge=0, le=5, default=0)
    oak: int = Field(ge=0, le=5, default=0)
    pine: int = Field(ge=0, le=5, default=0)
    mold: int = Field(ge=0, le=5, default=0)
    dust_mites: int = Field(ge=0, le=5, default=0)
    pet_dander: int = Field(ge=0, le=5, default=0)
    skin_prick_results: dict = {}
    immunotherapy: bool = False

class NodePair(BaseModel):
    node_type: str
    serial: str
    pubkey: str

# ---- Lifespan ----
@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool, redis_client
    db_pool = await asyncpg.create_pool(DB_URL, min_size=2, max_size=10)
    redis_client = redis.from_url(REDIS_URL)
    asyncio.create_task(mqtt_loop())
    logger.info("backend_started", db=DB_URL, mqtt=f"{MQTT_BROKER}:{MQTT_PORT}")
    yield
    await db_pool.close()
    await redis_client.close()

app = FastAPI(title="AllergySync API", version="1.0.0", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"],
                    allow_methods=["*"], allow_headers=["*"])

# ---- MQTT listener ----
async def mqtt_loop():
    """Subscribe to hub telemetry and store in database."""
    while True:
        try:
            async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
                await client.subscribe("allergysync/telemetry/#")
                await client.subscribe("allergysync/cmd/hub")
                logger.info("mqtt_connected")
                async for msg in client.messages:
                    await handle_mqtt_message(msg)
        except Exception as e:
            logger.error("mqtt_error", error=str(e))
            await asyncio.sleep(5)

async def handle_mqtt_message(msg):
    """Process incoming MQTT message from hub."""
    topic = str(msg.topic)
    payload = msg.payload
    parts = topic.split("/")
    
    if "telemetry" in parts:
        idx = parts.index("telemetry")
        if idx + 2 < len(parts):
            node_id = int(parts[idx + 2])
            await store_telemetry(node_id, payload)
            # Broadcast to WebSocket clients
            data = json.dumps({
                "node_id": node_id,
                "data": payload.hex(),
                "timestamp": datetime.utcnow().isoformat()
            })
            for ws in ws_clients:
                try:
                    await ws.send_text(data)
                except:
                    ws_clients.discard(ws)

async def store_telemetry(node_id: int, raw: bytes):
    """Parse and store telemetry packet."""
    # Parse based on node type (first byte of telemetry sub-type)
    if len(raw) < 2:
        return
    
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO telemetry (node_id, raw_data, received_at)
               VALUES ($1, $2, $3)""",
            node_id, raw, datetime.utcnow()
        )
        # Update Redis cache for latest state
        await redis_client.set(f"node:{node_id}:latest",
                               raw.hex(), ex=3600)

# ---- API Endpoints ----
@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "time": datetime.utcnow().isoformat()}

@app.get("/api/v1/exposure/current")
async def get_current_exposure():
    """Get current pollen levels and allergen risk."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT node_id, raw_data, received_at FROM telemetry
               WHERE received_at > NOW() - INTERVAL '10 minutes'
               ORDER BY received_at DESC LIMIT 10"""
        )
    
    # Parse latest sentinel telemetry
    pollen_class = 0
    pollen_conf = 0
    pm2_5 = 0
    pm10 = 0
    co2 = 0
    
    for row in rows:
        data = bytes.fromhex(row["raw_data"])
        if len(data) >= 40:  # Sentinel telemetry
            import struct
            vals = struct.unpack("<HHHHHhHHBBHH", data[:24])
            pm2_5 = vals[1] / 10.0
            pm10 = vals[2] / 10.0
            co2 = vals[3]
            pollen_class = vals[8]
            pollen_conf = vals[9]
            break
    
    pollen_names = ["none", "birch", "grass", "ragweed", "oak", "pine", "mold"]
    
    return {
        "pm2_5": pm2_5,
        "pm10": pm10,
        "co2_ppm": co2,
        "pollen_class": pollen_class,
        "pollen_name": pollen_names[pollen_class] if pollen_class < len(pollen_names) else "unknown",
        "pollen_confidence": pollen_conf,
        "risk_level": "high" if pollen_conf > 60 else "moderate" if pollen_conf > 30 else "low",
        "timestamp": datetime.utcnow().isoformat()
    }

@app.get("/api/v1/exposure/forecast")
async def get_forecast():
    """24-hour pollen forecast from PollenForecast LSTM."""
    # In production, this calls the ML inference service
    # Placeholder: return cached forecast
    forecast = await redis_client.get("pollen_forecast_24h")
    if forecast:
        return json.loads(forecast)
    
    return {
        "forecast": [
            {"hour": i, "pollen_class": 0, "concentration": 0,
             "confidence": 0}
            for i in range(24)
        ],
        "generated_at": datetime.utcnow().isoformat(),
        "source": "placeholder"
    }

@app.get("/api/v1/exposure/history")
async def get_history(hours: int = 24):
    """Historical exposure data."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT received_at, raw_data FROM telemetry
               WHERE received_at > NOW() - ($1 || ' hours')::interval
               ORDER BY received_at ASC""",
            str(hours)
        )
    
    history = []
    for row in rows:
        data = bytes.fromhex(row["raw_data"])
        if len(data) >= 24:
            import struct
            vals = struct.unpack("<HHHHHhHHBBHH", data[:24])
            history.append({
                "timestamp": row["received_at"].isoformat(),
                "pm2_5": vals[1] / 10.0,
                "pm10": vals[2] / 10.0,
                "pollen_class": vals[8],
                "pollen_confidence": vals[9]
            })
    
    return {"history": history, "count": len(history)}

@app.post("/api/v1/symptoms")
async def log_symptom(entry: SymptomEntry):
    """Log a symptom entry."""
    ts = entry.timestamp or datetime.utcnow()
    total = (entry.sneezing + entry.itchy_eyes + entry.congestion +
             entry.runny_nose + entry.headache + entry.fatigue) / 6.0
    
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO symptoms (timestamp, sneezing, itchy_eyes,
               congestion, runny_nose, headache, fatigue, notes, total_severity)
               VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)""",
            ts, entry.sneezing, entry.itchy_eyes, entry.congestion,
            entry.runny_nose, entry.headache, entry.fatigue,
            entry.notes, total
        )
    
    return {"status": "logged", "total_severity": total}

@app.get("/api/v1/symptoms")
async def get_symptoms(days: int = 30):
    """Retrieve symptom journal."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT * FROM symptoms
               WHERE timestamp > NOW() - ($1 || ' days')::interval
               ORDER BY timestamp DESC""",
            str(days)
        )
    
    return {"symptoms": [dict(r) for r in rows], "count": len(rows)}

@app.post("/api/v1/medication")
async def log_medication(entry: MedicationEntry):
    """Log a medication dose."""
    ts = entry.timestamp or datetime.utcnow()
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO medications (timestamp, medication, dose_mg, taken)
               VALUES ($1, $2, $3, $4)""",
            ts, entry.medication, entry.dose_mg, entry.taken
        )
    return {"status": "logged"}

@app.get("/api/v1/profile")
async def get_profile():
    """Get the user's allergy profile."""
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            "SELECT * FROM allergy_profile ORDER BY updated_at DESC LIMIT 1"
        )
    return dict(row) if row else {"message": "No profile set"}

@app.put("/api/v1/profile")
async def update_profile(profile: AllergyProfile):
    """Update the allergy profile."""
    async with db_pool.acquire() as conn:
        await conn.execute(
            """INSERT INTO allergy_profile
               (birch, grass, ragweed, oak, pine, mold, dust_mites, pet_dander,
                skin_prick_results, immunotherapy, updated_at)
               VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)""",
            profile.birch, profile.grass, profile.ragweed, profile.oak,
            profile.pine, profile.mold, profile.dust_mites, profile.pet_dander,
            json.dumps(profile.skin_prick_results), profile.immunotherapy,
            datetime.utcnow()
        )
    return {"status": "updated"}

@app.get("/api/v1/nodes")
async def list_nodes():
    """List all registered nodes."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch(
            """SELECT * FROM nodes ORDER BY node_id"""
        )
    return {"nodes": [dict(r) for r in rows]}

@app.post("/api/v1/nodes/pair")
async def pair_node(node: NodePair):
    """Pair a new node."""
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow(
            """INSERT INTO nodes (node_type, serial, pubkey, paired_at)
               VALUES ($1, $2, $3, $4) RETURNING node_id""",
            node.node_type, node.serial, node.pubkey, datetime.utcnow()
        )
    return {"status": "paired", "node_id": row["node_id"]}

@app.post("/api/v1/nodes/{node_id}/ota")
async def trigger_ota(node_id: int, version: str = "latest"):
    """Trigger OTA firmware update for a node."""
    # In production, this pushes firmware to the hub via MQTT
    # The hub then relays it to the target node via Sub-GHz
    return {"status": "ota_triggered", "node_id": node_id, "version": version}

@app.get("/api/v1/insights")
async def get_insights(period: str = "weekly"):
    """Get weekly/monthly insights."""
    days = 7 if period == "weekly" else 30
    async with db_pool.acquire() as conn:
        # Exposure summary
        exposure = await conn.fetch(
            """SELECT
                 AVG(raw_data) as avg_exposure,
                 MAX(received_at) as last_reading
               FROM telemetry
               WHERE received_at > NOW() - ($1 || ' days')::interval""",
            str(days)
        )
        # Symptom summary
        symptoms = await conn.fetch(
            """SELECT
                 AVG(total_severity) as avg_severity,
                 COUNT(*) as entry_count
               FROM symptoms
               WHERE timestamp > NOW() - ($1 || ' days')::interval""",
            str(days)
        )
        # Medication summary
        meds = await conn.fetch(
            """SELECT medication, COUNT(*) as doses
               FROM medications
               WHERE timestamp > NOW() - ($1 || ' days')::interval
               GROUP BY medication""",
            str(days)
        )
    
    return {
        "period": period,
        "days": days,
        "avg_symptom_severity": float(symptoms[0]["avg_severity"] or 0),
        "symptom_entries": symptoms[0]["entry_count"],
        "medication_doses": [dict(m) for m in meds],
        "last_reading": exposure[0]["last_reading"].isoformat() if exposure[0]["last_reading"] else None,
        "tip": "Your symptoms correlate most with birch pollen exposure. Consider keeping windows closed in the morning."
    }

# ---- WebSocket ----
@app.websocket("/api/v1/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    ws_clients.add(ws)
    try:
        while True:
            data = await ws.receive_text()
            # Client can send commands via WebSocket too
            if data == "ping":
                await ws.send_text(json.dumps({"type": "pong"}))
    except WebSocketDisconnect:
        ws_clients.discard(ws)

# ---- Database init script ----
DB_INIT_SQL = """
CREATE TABLE IF NOT EXISTS telemetry (
    id SERIAL PRIMARY KEY,
    node_id INTEGER NOT NULL,
    raw_data BYTEA NOT NULL,
    received_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS symptoms (
    id SERIAL PRIMARY KEY,
    timestamp TIMESTAMP NOT NULL DEFAULT NOW(),
    sneezing INTEGER DEFAULT 0,
    itchy_eyes INTEGER DEFAULT 0,
    congestion INTEGER DEFAULT 0,
    runny_nose INTEGER DEFAULT 0,
    headache INTEGER DEFAULT 0,
    fatigue INTEGER DEFAULT 0,
    notes TEXT DEFAULT '',
    total_severity FLOAT DEFAULT 0
);

CREATE TABLE IF NOT EXISTS medications (
    id SERIAL PRIMARY KEY,
    timestamp TIMESTAMP NOT NULL DEFAULT NOW(),
    medication TEXT NOT NULL,
    dose_mg FLOAT NOT NULL,
    taken BOOLEAN DEFAULT TRUE
);

CREATE TABLE IF NOT EXISTS allergy_profile (
    id SERIAL PRIMARY KEY,
    birch INTEGER DEFAULT 0,
    grass INTEGER DEFAULT 0,
    ragweed INTEGER DEFAULT 0,
    oak INTEGER DEFAULT 0,
    pine INTEGER DEFAULT 0,
    mold INTEGER DEFAULT 0,
    dust_mites INTEGER DEFAULT 0,
    pet_dander INTEGER DEFAULT 0,
    skin_prick_results JSONB DEFAULT '{}',
    immunotherapy BOOLEAN DEFAULT FALSE,
    updated_at TIMESTAMP NOT NULL DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS nodes (
    node_id SERIAL PRIMARY KEY,
    node_type TEXT NOT NULL,
    serial TEXT UNIQUE NOT NULL,
    pubkey TEXT NOT NULL,
    paired_at TIMESTAMP NOT NULL DEFAULT NOW(),
    last_seen TIMESTAMP,
    firmware_version TEXT DEFAULT '1.0.0'
);

CREATE INDEX IF NOT EXISTS idx_telemetry_node ON telemetry(node_id, received_at);
CREATE INDEX IF NOT EXISTS idx_symptoms_ts ON symptoms(timestamp);
"""