"""
CradleKeep — Cloud Dashboard Backend (FastAPI + MQTT)

Real-time infant monitoring: breathing tracking, cry classification,
feeding logs, sleep analysis, and environmental monitoring.

MQTT topics:
  cradle-keep/crib/data          — Crib pad sensor data (from hub)
  cradle-keep/nursery/data      — Nursery monitor data
  cradle-keep/feeding/data      — Feeding station data
  cradle-keep/cry/event         — Cry classification events
  cradle-keep/breathing/alert   — Breathing safety alerts
  cradle-keep/sleep/stage       — Sleep stage updates
  cradle-keep/commands/{node}   — Commands to nodes (via hub)
"""

from fastapi import FastAPI, WebSocket, HTTPException, Depends, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime, timedelta
import asyncio
import json
import logging
from contextlib import asynccontextmanager

import sqlalchemy as sa
from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession, async_sessionmaker
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column
import aiomqtt

# ── Configuration ──────────────────────────────────────────────────────────
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_PREFIX = "cradle-keep"
DATABASE_URL = "postgresql+asyncpg://cradlekeep:cradlekeep@localhost/cradlekeep"

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("cradlekeep")

# ── Database Models ───────────────────────────────────────────────────────
class Base(DeclarativeBase):
    pass

class CribReading(Base):
    __tablename__ = "crib_readings"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    breath_rate: Mapped[int] = mapped_column(sa.SmallInteger)        # BPM
    breath_regularity: Mapped[int] = mapped_column(sa.SmallInteger)  # 0-100
    movement_score: Mapped[int] = mapped_column(sa.SmallInteger)      # 0-255
    position: Mapped[int] = mapped_column(sa.SmallInteger)            # POS_*
    temp_c_x10: Mapped[int] = mapped_column(sa.Integer)              # °C ×10
    wetness_flag: Mapped[int] = mapped_column(sa.SmallInteger)        # 0/1
    wetness_level: Mapped[int] = mapped_column(sa.SmallInteger)      # 0-255
    breath_apnea_count: Mapped[int] = mapped_column(sa.SmallInteger)
    alert_level: Mapped[int] = mapped_column(sa.SmallInteger)        # 0-4
    battery_pct: Mapped[int] = mapped_column(sa.SmallInteger)

class NurseryReading(Base):
    __tablename__ = "nursery_readings"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    cry_type: Mapped[int] = mapped_column(sa.SmallInteger)           # CRY_*
    cry_confidence: Mapped[int] = mapped_column(sa.SmallInteger)     # 0-255
    cry_intensity: Mapped[int] = mapped_column(sa.SmallInteger)      # 0-255
    room_temp_c_x10: Mapped[int] = mapped_column(sa.Integer)
    room_humidity_x10: Mapped[int] = mapped_column(sa.Integer)
    co2_ppm: Mapped[int] = mapped_column(sa.Integer)
    voc_index: Mapped[int] = mapped_column(sa.Integer)
    light_lux: Mapped[int] = mapped_column(sa.Integer)
    noise_level_db: Mapped[int] = mapped_column(sa.SmallInteger)
    ir_active: Mapped[int] = mapped_column(sa.SmallInteger)
    baby_present: Mapped[int] = mapped_column(sa.SmallInteger)
    alert_level: Mapped[int] = mapped_column(sa.SmallInteger)
    battery_pct: Mapped[int] = mapped_column(sa.SmallInteger)

class FeedingReading(Base):
    __tablename__ = "feeding_readings"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    feeding_state: Mapped[int] = mapped_column(sa.SmallInteger)      # FEED_*
    bottle_temp_c_x10: Mapped[int] = mapped_column(sa.Integer)
    target_temp_c_x10: Mapped[int] = mapped_column(sa.Integer)
    weight_mg: Mapped[int] = mapped_column(sa.Integer)
    start_weight_mg: Mapped[int] = mapped_column(sa.Integer)
    volume_consumed_ml: Mapped[int] = mapped_column(sa.Integer)
    feeding_duration_s: Mapped[int] = mapped_column(sa.Integer)
    heater_pct: Mapped[int] = mapped_column(sa.SmallInteger)
    uv_turbidity: Mapped[int] = mapped_column(sa.SmallInteger)
    battery_pct: Mapped[int] = mapped_column(sa.SmallInteger)

class CryEvent(Base):
    __tablename__ = "cry_events"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    cry_type: Mapped[int] = mapped_column(sa.SmallInteger)            # CRY_*
    cry_confidence: Mapped[int] = mapped_column(sa.SmallInteger)      # 0-255
    cry_intensity: Mapped[int] = mapped_column(sa.SmallInteger)        # 0-255
    duration_s: Mapped[int] = mapped_column(sa.SmallInteger)
    preceding_sleep_stage: Mapped[int] = mapped_column(sa.SmallInteger)
    time_since_feed_min: Mapped[int] = mapped_column(sa.Integer)
    time_since_sleep_min: Mapped[int] = mapped_column(sa.Integer)

class BreathingAlert(Base):
    __tablename__ = "breathing_alerts"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    alert_level: Mapped[int] = mapped_column(sa.SmallInteger)          # 0-4
    breath_rate: Mapped[int] = mapped_column(sa.SmallInteger)
    apnea_duration_ms: Mapped[int] = mapped_column(sa.Integer)
    time_since_breath_ms: Mapped[int] = mapped_column(sa.Integer)
    position: Mapped[int] = mapped_column(sa.SmallInteger)
    movement_score: Mapped[int] = mapped_column(sa.SmallInteger)
    source_node: Mapped[int] = mapped_column(sa.SmallInteger)

class SleepSession(Base):
    __tablename__ = "sleep_sessions"
    id: Mapped[int] = mapped_column(primary_key=True)
    timestamp: Mapped[datetime] = mapped_column(sa.DateTime, default=datetime.utcnow)
    sleep_stage: Mapped[int] = mapped_column(sa.SmallInteger)          # SLEEP_*
    confidence: Mapped[int] = mapped_column(sa.SmallInteger)            # 0-255
    breath_rate: Mapped[int] = mapped_column(sa.SmallInteger)
    movement_score: Mapped[int] = mapped_column(sa.SmallInteger)
    duration_s: Mapped[int] = mapped_column(sa.Integer)

# ── Pydantic Models ────────────────────────────────────────────────────────
class CribDataInput(BaseModel):
    breath_rate: int = Field(ge=0, le=120)
    breath_regularity: int = Field(ge=0, le=100)
    movement_score: int = Field(ge=0, le=255)
    position: int = Field(ge=0, le=5)
    temp_c_x10: int
    wetness_flag: int = Field(ge=0, le=1)
    wetness_level: int = Field(ge=0, le=255)
    breath_apnea_count: int = Field(ge=0)
    alert_level: int = Field(ge=0, le=4)
    battery_pct: int = Field(ge=0, le=100)

class CryEventInput(BaseModel):
    cry_type: int = Field(ge=0, le=5)
    cry_confidence: int = Field(ge=0, le=255)
    cry_intensity: int = Field(ge=0, le=255)
    duration_s: int = Field(ge=0)
    preceding_sleep_stage: int = Field(ge=0, le=3)
    time_since_feed_min: int = Field(ge=0)
    time_since_sleep_min: int = Field(ge=0)

class BreathingAlertInput(BaseModel):
    alert_level: int = Field(ge=0, le=4)
    breath_rate: int = Field(ge=0, le=120)
    apnea_duration_ms: int = Field(ge=0)
    time_since_breath_ms: int = Field(ge=0)
    position: int = Field(ge=0, le=5)
    movement_score: int = Field(ge=0, le=255)

class FeedingStartInput(BaseModel):
    target_temp_c_x10: int = 370  # 37.0°C default
    auto_start: bool = False

class SoundCommandInput(BaseModel):
    sound_type: int = Field(ge=1, le=7)  # SOUND_* constants
    duration_s: int = Field(ge=0, default=1800)  # 30 min default

# ── Application ────────────────────────────────────────────────────────────
engine = create_async_engine(DATABASE_URL)
async_session = async_sessionmaker(engine, expire_on_commit=False)

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Create database tables and start MQTT listener."""
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    
    # Start MQTT listener in background
    mqtt_task = asyncio.create_task(mqtt_listener())
    
    yield
    
    mqtt_task.cancel()

app = FastAPI(
    title="CradleKeep API",
    description="AI-powered infant monitoring and care system",
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

# ── WebSocket Connections ──────────────────────────────────────────────────
websocket_connections: list[WebSocket] = []

async def broadcast_to_websockets(data: dict):
    """Broadcast data to all connected WebSocket clients."""
    for ws in websocket_connections[:]:
        try:
            await ws.send_json(data)
        except Exception:
            websocket_connections.remove(ws)

# ── MQTT Listener ──────────────────────────────────────────────────────────
async def mqtt_listener():
    """Listen for MQTT messages from hub and store in database."""
    while True:
        try:
            async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
                await client.subscribe(f"{MQTT_PREFIX}/#")
                async for message in client.messages:
                    topic = message.topic.value
                    payload = json.loads(message.payload.decode())
                    
                    async with async_session() as session:
                        if topic == f"{MQTT_PREFIX}/crib/data":
                            reading = CribReading(**payload)
                            session.add(reading)
                        elif topic == f"{MQTT_PREFIX}/nursery/data":
                            reading = NurseryReading(**payload)
                            session.add(reading)
                        elif topic == f"{MQTT_PREFIX}/feeding/data":
                            reading = FeedingReading(**payload)
                            session.add(reading)
                        elif topic == f"{MQTT_PREFIX}/cry/event":
                            event = CryEvent(**payload)
                            session.add(event)
                        elif topic == f"{MQTT_PREFIX}/breathing/alert":
                            alert = BreathingAlert(**payload)
                            session.add(alert)
                        elif topic == f"{MQTT_PREFIX}/sleep/stage":
                            stage = SleepSession(**payload)
                            session.add(stage)
                        
                        await session.commit()
                    
                    # Broadcast to WebSocket clients
                    await broadcast_to_websockets({
                        "topic": topic,
                        "data": payload,
                        "timestamp": datetime.utcnow().isoformat(),
                    })
                    
        except Exception as e:
            logger.error(f"MQTT error: {e}")
            await asyncio.sleep(5)

# ── REST API Endpoints ─────────────────────────────────────────────────────

@app.get("/")
async def root():
    return {"name": "CradleKeep API", "version": "1.0.0"}

@app.get("/health")
async def health():
    return {"status": "ok", "timestamp": datetime.utcnow()}

# ── Crib Data ───────────────────────────────────────────────────────────────

@app.post("/api/crib/data")
async def post_crib_data(data: CribDataInput):
    """Receive crib pad sensor data from hub."""
    async with async_session() as session:
        reading = CribReading(**data.model_dump())
        session.add(reading)
        await session.commit()
    
    # Check for breathing alerts
    if data.breath_rate < 15 or data.breath_rate > 70:
        # Send push notification
        pass
    if data.wetness_flag:
        # Wetness notification
        pass
    
    return {"status": "ok"}

@app.get("/api/crib/data")
async def get_crib_data(
    hours: int = Query(default=24, ge=1, le=168),
    limit: int = Query(default=1000, ge=1, le=10000),
):
    """Get recent crib data."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        result = await session.execute(
            sa.select(CribReading)
            .where(CribReading.timestamp >= since)
            .order_by(CribReading.timestamp.desc())
            .limit(limit)
        )
        readings = result.scalars().all()
        return [r.__dict__ for r in readings]

@app.get("/api/crib/breathing")
async def get_breathing_summary(hours: int = Query(default=24, ge=1, le=168)):
    """Get breathing rate summary statistics."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        result = await session.execute(
            sa.select(
                sa.func.avg(CribReading.breath_rate).label("avg_rate"),
                sa.func.min(CribReading.breath_rate).label("min_rate"),
                sa.func.max(CribReading.breath_rate).label("max_rate"),
                sa.func.avg(CribReading.breath_regularity).label("avg_regularity"),
                sa.func.count().label("total_readings"),
                sa.func.sum(sa.case((CribReading.alert_level > 0, 1), else_=0)).label("alert_count"),
            ).where(CribReading.timestamp >= since)
        )
        row = result.one()
        return {
            "avg_breath_rate": float(row.avg_rate) if row.avg_rate else 0,
            "min_breath_rate": int(row.min_rate) if row.min_rate else 0,
            "max_breath_rate": int(row.max_rate) if row.max_rate else 0,
            "avg_regularity": float(row.avg_regularity) if row.avg_regularity else 0,
            "total_readings": int(row.total_readings) if row.total_readings else 0,
            "alert_count": int(row.alert_count) if row.alert_count else 0,
        }

# ── Nursery Data ────────────────────────────────────────────────────────────

@app.post("/api/nursery/data")
async def post_nursery_data(data: dict):
    """Receive nursery monitor data from hub."""
    async with async_session() as session:
        reading = NurseryReading(**data)
        session.add(reading)
        await session.commit()
    return {"status": "ok"}

@app.get("/api/nursery/environment")
async def get_environment(hours: int = Query(default=24, ge=1, le=168)):
    """Get room environment summary."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        result = await session.execute(
            sa.select(
                sa.func.avg(NurseryReading.room_temp_c_x10).label("avg_temp"),
                sa.func.avg(NurseryReading.room_humidity_x10).label("avg_humidity"),
                sa.func.avg(NurseryReading.co2_ppm).label("avg_co2"),
                sa.func.avg(NurseryReading.voc_index).label("avg_voc"),
                sa.func.avg(NurseryReading.light_lux).label("avg_light"),
                sa.func.avg(NurseryReading.noise_level_db).label("avg_noise"),
            ).where(NurseryReading.timestamp >= since)
        )
        row = result.one()
        return {
            "avg_temp_c": float(row.avg_temp) / 10 if row.avg_temp else None,
            "avg_humidity_pct": float(row.avg_humidity) / 10 if row.avg_humidity else None,
            "avg_co2_ppm": float(row.avg_co2) if row.avg_co2 else None,
            "avg_voc_index": float(row.avg_voc) if row.avg_voc else None,
            "avg_light_lux": float(row.avg_light) if row.avg_light else None,
            "avg_noise_db": float(row.avg_noise) if row.avg_noise else None,
        }

# ── Feeding Data ────────────────────────────────────────────────────────────

@app.post("/api/feeding/data")
async def post_feeding_data(data: dict):
    """Receive feeding station data from hub."""
    async with async_session() as session:
        reading = FeedingReading(**data)
        session.add(reading)
        await session.commit()
    return {"status": "ok"}

@app.get("/api/feeding/history")
async def get_feeding_history(days: int = Query(default=7, ge=1, le=30)):
    """Get feeding history for analysis."""
    since = datetime.utcnow() - timedelta(days=days)
    async with async_session() as session:
        result = await session.execute(
            sa.select(FeedingReading)
            .where(FeedingReading.timestamp >= since)
            .where(FeedingReading.feeding_state == 4)  # FEED_DONE
            .order_by(FeedingReading.timestamp.desc())
        )
        readings = result.scalars().all()
        return [{
            "timestamp": r.timestamp.isoformat(),
            "volume_ml": r.volume_consumed_ml,
            "duration_s": r.feeding_duration_s,
            "bottle_temp_c": r.bottle_temp_c_x10 / 10 if r.bottle_temp_c_x10 else None,
        } for r in readings]

@app.post("/api/feeding/start")
async def start_warming(params: FeedingStartInput):
    """Start bottle warming via hub."""
    # Send MQTT command to hub
    command = {
        "cmd_id": 0x01,  # CMD_START_WARMING
        "target_node": 0x03,  # ADDR_FEEDING_STATION
        "param1": params.target_temp_c_x10,
    }
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        await client.publish(f"{MQTT_PREFIX}/commands/feeding", json.dumps(command))
    return {"status": "ok", "message": "Warming command sent"}

@app.post("/api/feeding/stop")
async def stop_warming():
    """Stop bottle warming."""
    command = {"cmd_id": 0x02, "target_node": 0x03}
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        await client.publish(f"{MQTT_PREFIX}/commands/feeding", json.dumps(command))
    return {"status": "ok"}

# ── Cry Events ───────────────────────────────────────────────────────────────

@app.post("/api/cry/event")
async def post_cry_event(event: CryEventInput):
    """Receive cry classification event from nursery monitor."""
    async with async_session() as session:
        cry_event = CryEvent(**event.model_dump())
        session.add(cry_event)
        await session.commit()
    
    # Push notification to mobile app
    cry_types = {0: "none", 1: "hungry", 2: "tired", 3: "pain", 4: "colic", 5: "discomfort"}
    if event.cry_type > 0 and event.cry_confidence > 128:
        # Send push notification
        pass
    
    return {"status": "ok"}

@app.get("/api/cry/patterns")
async def get_cry_patterns(days: int = Query(default=7, ge=1, le=30)):
    """Get cry pattern analysis."""
    since = datetime.utcnow() - timedelta(days=days)
    async with async_session() as session:
        result = await session.execute(
            sa.select(
                CryEvent.cry_type,
                sa.func.count().label("count"),
                sa.func.avg(CryEvent.cry_confidence).label("avg_confidence"),
                sa.func.avg(CryEvent.duration_s).label("avg_duration"),
                sa.extract("hour", CryEvent.timestamp).label("hour"),
            )
            .where(CryEvent.timestamp >= since)
            .where(CryEvent.cry_type > 0)
            .group_by(CryEvent.cry_type, sa.extract("hour", CryEvent.timestamp))
            .order_by(sa.extract("hour", CryEvent.timestamp))
        )
        rows = result.all()
        return [{
            "cry_type": row.cry_type,
            "count": int(row.count),
            "avg_confidence": float(row.avg_confidence) if row.avg_confidence else 0,
            "avg_duration_s": float(row.avg_duration) if row.avg_duration else 0,
            "hour": int(row.hour),
        } for row in rows]

# ── Breathing Alerts ────────────────────────────────────────────────────────

@app.post("/api/breathing/alert")
async def post_breathing_alert(alert: BreathingAlertInput):
    """Receive breathing safety alert."""
    async with async_session() as session:
        breathing_alert = BreathingAlert(**alert.model_dump())
        session.add(breathing_alert)
        await session.commit()
    
    # Escalate alerts
    if alert.alert_level >= 3:  # CRITICAL or EMERGENCY
        # Send SMS and push notification
        pass
    elif alert.alert_level >= 2:  # URGENT
        # Send push notification
        pass
    
    return {"status": "ok", "alert_level": alert.alert_level}

@app.get("/api/breathing/alerts")
async def get_breathing_alerts(hours: int = Query(default=24, ge=1, le=168)):
    """Get breathing alert history."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        result = await session.execute(
            sa.select(BreathingAlert)
            .where(BreathingAlert.timestamp >= since)
            .order_by(BreathingAlert.timestamp.desc())
        )
        alerts = result.scalars().all()
        return [a.__dict__ for a in alerts]

# ── Sleep Analysis ──────────────────────────────────────────────────────────

@app.get("/api/sleep/analysis")
async def get_sleep_analysis(hours: int = Query(default=24, ge=1, le=168)):
    """Get sleep stage analysis."""
    since = datetime.utcnow() - timedelta(hours=hours)
    async with async_session() as session:
        result = await session.execute(
            sa.select(
                SleepSession.sleep_stage,
                sa.func.count().label("count"),
                sa.func.sum(SleepSession.duration_s).label("total_duration"),
                sa.func.avg(SleepSession.confidence).label("avg_confidence"),
            )
            .where(SleepSession.timestamp >= since)
            .group_by(SleepSession.sleep_stage)
        )
        rows = result.all()
        stages = {0: "awake", 1: "light", 2: "deep", 3: "rem"}
        return [{
            "stage": stages.get(row.sleep_stage, "unknown"),
            "episodes": int(row.count),
            "total_duration_s": int(row.total_duration) if row.total_duration else 0,
            "avg_confidence": float(row.avg_confidence) if row.avg_confidence else 0,
        } for row in rows]

# ── Sound Control ────────────────────────────────────────────────────────────

@app.post("/api/sound/play")
async def play_sound(params: SoundCommandInput):
    """Play a soothing sound on the hub speaker."""
    command = {
        "cmd_id": 0x04,  # CMD_PLAY_SOUND
        "param1": params.sound_type,
        "param2": params.duration_s,
    }
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        await client.publish(f"{MQTT_PREFIX}/commands/hub", json.dumps(command))
    return {"status": "ok", "sound_type": params.sound_type}

@app.post("/api/sound/stop")
async def stop_sound():
    """Stop playing sound."""
    command = {"cmd_id": 0x05}  # CMD_STOP_SOUND
    async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
        await client.publish(f"{MQTT_PREFIX}/commands/hub", json.dumps(command))
    return {"status": "ok"}

# ── WebSocket ────────────────────────────────────────────────────────────────

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    websocket_connections.append(websocket)
    try:
        while True:
            # Keep connection alive; client can send commands
            data = await websocket.receive_text()
            # Process client commands if needed
    except Exception:
        websocket_connections.remove(websocket)

# ── Dashboard Data ──────────────────────────────────────────────────────────

@app.get("/api/dashboard")
async def get_dashboard():
    """Get current dashboard state (latest readings from all nodes)."""
    async with async_session() as session:
        # Latest crib reading
        crib = await session.execute(
            sa.select(CribReading).order_by(CribReading.timestamp.desc()).limit(1)
        )
        crib_reading = crib.scalar_one_or_none()
        
        # Latest nursery reading
        nursery = await session.execute(
            sa.select(NurseryReading).order_by(NurseryReading.timestamp.desc()).limit(1)
        )
        nursery_reading = nursery.scalar_one_or_none()
        
        # Latest feeding reading
        feeding = await session.execute(
            sa.select(FeedingReading).order_by(FeedingReading.timestamp.desc()).limit(1)
        )
        feeding_reading = feeding.scalar_one_or_none()
        
        # Latest cry event
        cry = await session.execute(
            sa.select(CryEvent).order_by(CryEvent.timestamp.desc()).limit(1)
        )
        cry_event = cry.scalar_one_or_none()
        
        return {
            "crib": {
                "breath_rate": crib_reading.breath_rate if crib_reading else None,
                "breath_regularity": crib_reading.breath_regularity if crib_reading else None,
                "movement_score": crib_reading.movement_score if crib_reading else None,
                "position": crib_reading.position if crib_reading else None,
                "temp_c": crib_reading.temp_c_x10 / 10 if crib_reading else None,
                "wetness": crib_reading.wetness_flag if crib_reading else None,
                "alert_level": crib_reading.alert_level if crib_reading else None,
            } if crib_reading else None,
            "nursery": {
                "cry_type": nursery_reading.cry_type if nursery_reading else None,
                "cry_confidence": nursery_reading.cry_confidence if nursery_reading else None,
                "room_temp_c": nursery_reading.room_temp_c_x10 / 10 if nursery_reading else None,
                "humidity_pct": nursery_reading.room_humidity_x10 / 10 if nursery_reading else None,
                "co2_ppm": nursery_reading.co2_ppm if nursery_reading else None,
                "light_lux": nursery_reading.light_lux if nursery_reading else None,
                "noise_db": nursery_reading.noise_level_db if nursery_reading else None,
            } if nursery_reading else None,
            "feeding": {
                "state": feeding_reading.feeding_state if feeding_reading else None,
                "bottle_temp_c": feeding_reading.bottle_temp_c_x10 / 10 if feeding_reading else None,
                "volume_consumed_ml": feeding_reading.volume_consumed_ml if feeding_reading else None,
                "duration_s": feeding_reading.feeding_duration_s if feeding_reading else None,
            } if feeding_reading else None,
            "cry": {
                "type": cry_event.cry_type if cry_event else None,
                "confidence": cry_event.cry_confidence if cry_event else None,
                "timestamp": cry_event.timestamp.isoformat() if cry_event else None,
            } if cry_event else None,
        }