"""
BreathHome - Cloud Dashboard Backend
FastAPI + MQTT + PostgreSQL + WebSocket

Receives sensor data from hubs via MQTT, stores in PostgreSQL,
serves REST API and WebSocket for real-time dashboard updates.
"""

import os
import json
import asyncio
from datetime import datetime, timedelta
from typing import List, Optional, Dict, Any
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
import asyncpg
import paho.mqtt.client as mqtt
import numpy as np

# ========== CONFIGURATION ==========

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://breathhome:breathhome@localhost/breathhome")
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "breathhome")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "breathhome")

# ========== PYDANTIC MODELS ==========

class SensorReading(BaseModel):
    """Air quality reading from a room sensor node."""
    room_id: int
    pm1_0: Optional[float] = None
    pm2_5: Optional[float] = None
    pm4_0: Optional[float] = None
    pm10: Optional[float] = None
    co2: Optional[float] = None
    voc_index: Optional[float] = None
    nox_index: Optional[float] = None
    hcho: Optional[float] = None
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    pressure: Optional[float] = None
    light_lux: Optional[int] = None
    radon_bq_m3: Optional[float] = None
    mold_risk_pct: Optional[float] = None
    aqi_score: Optional[int] = None
    aqi_category: Optional[int] = None
    timestamp: Optional[datetime] = None

class HVACState(BaseModel):
    """HVAC controller state."""
    vent_positions: List[int] = Field(default_factory=lambda: [50] * 8)
    purifier_speed: int = 0
    filter_health_pct: float = 100.0
    duct_pressure_pa: float = 101325.0
    supply_air_temp_c: float = 22.0
    blower_current_ma: float = 0.0
    relay_states: int = 0

class ExposureData(BaseModel):
    """Personal exposure data from wearable tag."""
    tag_id: int
    eco2: float
    tvoc: float
    temperature: float
    humidity: float
    activity: int
    personal_aqi: float
    battery_pct: int
    symptom_flag: int = 0

class AlertRule(BaseModel):
    """Alert rule configuration."""
    room_id: int
    parameter: str  # pm25, co2, voc, hcho, radon, mold, aqi
    threshold: float
    operator: str  # gt, lt, gte, lte
    action: str  # notify, hvac_ventilate, hvac_purify, hvac_exhaust, alarm
    enabled: bool = True

class AlertEvent(BaseModel):
    """An alert event that has been triggered."""
    id: Optional[int] = None
    room_id: int
    parameter: str
    value: float
    threshold: float
    category: str  # info, warning, danger, critical
    message: str
    timestamp: datetime
    acknowledged: bool = False

class UserConfig(BaseModel):
    """User configuration including health profile."""
    user_id: int
    name: str
    has_asthma: bool = False
    has_copd: bool = False
    has_allergies: bool = False
    alert_phone: Optional[str] = None
    alert_email: Optional[str] = None
    alert_push: bool = True


# ========== DATABASE SETUP ==========

async def init_db():
    """Initialize PostgreSQL database tables."""
    pool = await asyncpg.create_pool(DATABASE_URL)
    async with pool.acquire() as conn:
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS sensor_readings (
                id SERIAL PRIMARY KEY,
                hub_id INTEGER NOT NULL,
                room_id INTEGER NOT NULL,
                timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                pm1_0 REAL,
                pm2_5 REAL,
                pm4_0 REAL,
                pm10 REAL,
                co2 REAL,
                voc_index REAL,
                nox_index REAL,
                hcho REAL,
                temperature REAL,
                humidity REAL,
                pressure REAL,
                light_lux INTEGER,
                radon_bq_m3 REAL,
                mold_risk_pct REAL,
                aqi_score INTEGER,
                aqi_category INTEGER
            );
            
            CREATE TABLE IF NOT EXISTS hvac_states (
                id SERIAL PRIMARY KEY,
                hub_id INTEGER NOT NULL,
                timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                vent_positions INTEGER[] NOT NULL,
                purifier_speed INTEGER NOT NULL,
                filter_health_pct REAL NOT NULL,
                duct_pressure_pa REAL,
                supply_air_temp_c REAL,
                blower_current_ma REAL,
                relay_states INTEGER NOT NULL
            );
            
            CREATE TABLE IF NOT EXISTS exposure_data (
                id SERIAL PRIMARY KEY,
                hub_id INTEGER NOT NULL,
                tag_id INTEGER NOT NULL,
                timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                eco2 REAL,
                tvoc REAL,
                temperature REAL,
                humidity REAL,
                activity INTEGER,
                personal_aqi REAL,
                battery_pct INTEGER,
                symptom_flag INTEGER DEFAULT 0
            );
            
            CREATE TABLE IF NOT EXISTS alert_events (
                id SERIAL PRIMARY KEY,
                hub_id INTEGER NOT NULL,
                room_id INTEGER,
                parameter TEXT NOT NULL,
                value REAL NOT NULL,
                threshold REAL NOT NULL,
                category TEXT NOT NULL,
                message TEXT NOT NULL,
                timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                acknowledged BOOLEAN DEFAULT FALSE
            );
            
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                name TEXT NOT NULL,
                has_asthma BOOLEAN DEFAULT FALSE,
                has_copd BOOLEAN DEFAULT FALSE,
                has_allergies BOOLEAN DEFAULT FALSE,
                alert_phone TEXT,
                alert_email TEXT,
                alert_push BOOLEAN DEFAULT TRUE
            );
            
            -- Create hypertables for time-series data (TimescaleDB)
            -- CREATE EXTENSION IF NOT EXISTS timescaledb;
            -- SELECT create_hypertable('sensor_readings', 'timestamp');
            -- SELECT create_hypertable('exposure_data', 'timestamp');
            
            -- Indexes for common queries
            CREATE INDEX IF NOT EXISTS idx_sensor_room_time ON sensor_readings (room_id, timestamp DESC);
            CREATE INDEX IF NOT EXISTS idx_sensor_hub_time ON sensor_readings (hub_id, timestamp DESC);
            CREATE INDEX IF NOT EXISTS idx_exposure_tag_time ON exposure_data (tag_id, timestamp DESC);
            CREATE INDEX IF NOT EXISTS idx_alerts_time ON alert_events (timestamp DESC);
        """)
    return pool


# ========== ALERT ENGINE ==========

class AlertEngine:
    """Evaluate alert rules against incoming sensor data."""
    
    ALERT_THRESHOLDS = {
        "pm25": {"warning": 35.4, "danger": 55.4, "critical": 150.4},
        "co2": {"warning": 1200, "danger": 1800, "critical": 2500},
        "voc": {"warning": 200, "danger": 300, "critical": 500},
        "hcho": {"warning": 0.05, "danger": 0.08, "critical": 0.1},
        "radon": {"warning": 148, "danger": 370, "critical": 740},  # Bq/m3
        "mold": {"warning": 60, "danger": 80, "critical": 95},
        "aqi": {"warning": 101, "danger": 151, "critical": 301},
    }
    
    def __init__(self):
        self.websocket_connections: List[WebSocket] = []
    
    def evaluate(self, reading: SensorReading) -> List[AlertEvent]:
        """Evaluate a sensor reading against all alert thresholds."""
        alerts = []
        
        # PM2.5
        if reading.pm2_5 is not None:
            alerts.extend(self._check_threshold(
                reading.room_id, "pm25", reading.pm2_5, reading.timestamp
            ))
        
        # CO2
        if reading.co2 is not None:
            alerts.extend(self._check_threshold(
                reading.room_id, "co2", reading.co2, reading.timestamp
            ))
        
        # VOC
        if reading.voc_index is not None:
            alerts.extend(self._check_threshold(
                reading.room_id, "voc", reading.voc_index, reading.timestamp
            ))
        
        # Formaldehyde
        if reading.hcho is not None:
            alerts.extend(self._check_threshold(
                reading.room_id, "hcho", reading.hcho, reading.timestamp
            ))
        
        # Radon
        if reading.radon_bq_m3 is not None and reading.radon_bq_m3 > 0:
            alerts.extend(self._check_threshold(
                reading.room_id, "radon", reading.radon_bq_m3, reading.timestamp
            ))
        
        # Mold risk
        if reading.mold_risk_pct is not None:
            alerts.extend(self._check_threshold(
                reading.room_id, "mold", reading.mold_risk_pct, reading.timestamp
            ))
        
        # AQI
        if reading.aqi_score is not None:
            alerts.extend(self._check_threshold(
                reading.room_id, "aqi", float(reading.aqi_score), reading.timestamp
            ))
        
        return alerts
    
    def _check_threshold(self, room_id: int, parameter: str, 
                          value: float, timestamp: Optional[datetime]) -> List[AlertEvent]:
        """Check a single parameter against thresholds."""
        alerts = []
        thresholds = self.ALERT_THRESHOLDS.get(parameter, {})
        
        if value >= thresholds.get("critical", float('inf')):
            alerts.append(AlertEvent(
                room_id=room_id,
                parameter=parameter,
                value=value,
                threshold=thresholds["critical"],
                category="critical",
                message=f"CRITICAL: {parameter} = {value:.1f} in room {room_id}",
                timestamp=timestamp or datetime.utcnow()
            ))
        elif value >= thresholds.get("danger", float('inf')):
            alerts.append(AlertEvent(
                room_id=room_id,
                parameter=parameter,
                value=value,
                threshold=thresholds["danger"],
                category="danger",
                message=f"DANGER: {parameter} = {value:.1f} in room {room_id}",
                timestamp=timestamp or datetime.utcnow()
            ))
        elif value >= thresholds.get("warning", float('inf')):
            alerts.append(AlertEvent(
                room_id=room_id,
                parameter=parameter,
                value=value,
                threshold=thresholds["warning"],
                category="warning",
                message=f"Warning: {parameter} = {value:.1f} in room {room_id}",
                timestamp=timestamp or datetime.utcnow()
            ))
        
        return alerts
    
    async def broadcast_alert(self, alert: AlertEvent):
        """Broadcast an alert to all connected WebSocket clients."""
        for ws in self.websocket_connections:
            try:
                await ws.send_json(alert.dict())
            except Exception:
                self.websocket_connections.remove(ws)


# ========== FASTAPI APPLICATION ==========

db_pool = None
alert_engine = AlertEngine()

@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool
    db_pool = await init_db()
    yield
    await db_pool.close()

app = FastAPI(
    title="BreathHome Dashboard API",
    description="Cloud backend for BreathHome indoor air quality monitoring system",
    version="1.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ========== MQTT HANDLER ==========

mqtt_client = mqtt.Client(client_id="breathhome-api")

def on_connect(client, userdata, flags, rc):
    """Subscribe to all sensor topics on connect."""
    client.subscribe("breathhome/sensors/+/+")
    client.subscribe("breathhome/hvac/+/status")
    client.subscribe("breathhome/exposure/+/+")
    client.subscribe("breathhome/alerts/+/+")

def on_message(client, userdata, msg):
    """Process incoming MQTT messages."""
    global db_pool
    if db_pool is None:
        return
    
    topic = msg.topic
    payload = json.loads(msg.payload.decode())
    
    asyncio.create_task(_process_mqtt_message(topic, payload))

async def _process_mqtt_message(topic: str, payload: dict):
    """Process MQTT message asynchronously."""
    async with db_pool.acquire() as conn:
        if topic.startswith("breathhome/sensors/"):
            # Store sensor reading
            await conn.execute("""
                INSERT INTO sensor_readings (hub_id, room_id, pm2_5, co2, voc_index, 
                    hcho, temperature, humidity, pressure, radon_bq_m3, mold_risk_pct, 
                    aqi_score, aqi_category)
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)
            """, 
                payload.get("hub_id", 1),
                payload.get("room_id", 0),
                payload.get("pm2_5"),
                payload.get("co2"),
                payload.get("voc_index"),
                payload.get("hcho"),
                payload.get("temperature"),
                payload.get("humidity"),
                payload.get("pressure"),
                payload.get("radon_bq_m3"),
                payload.get("mold_risk_pct"),
                payload.get("aqi_score"),
                payload.get("aqi_category")
            )
            
            # Evaluate alerts
            reading = SensorReading(
                room_id=payload.get("room_id", 0),
                pm2_5=payload.get("pm2_5"),
                co2=payload.get("co2"),
                voc_index=payload.get("voc_index"),
                hcho=payload.get("hcho"),
                radon_bq_m3=payload.get("radon_bq_m3"),
                mold_risk_pct=payload.get("mold_risk_pct"),
                aqi_score=payload.get("aqi_score"),
                aqi_category=payload.get("aqi_category"),
            )
            alerts = alert_engine.evaluate(reading)
            for alert in alerts:
                await alert_engine.broadcast_alert(alert)
        
        elif topic.startswith("breathhome/exposure/"):
            # Store exposure data
            await conn.execute("""
                INSERT INTO exposure_data (hub_id, tag_id, eco2, tvoc, temperature, 
                    humidity, activity, personal_aqi, battery_pct, symptom_flag)
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
            """,
                payload.get("hub_id", 1),
                payload.get("tag_id", 0),
                payload.get("eco2"),
                payload.get("tvoc"),
                payload.get("temperature"),
                payload.get("humidity"),
                payload.get("activity"),
                payload.get("personal_aqi"),
                payload.get("battery_pct"),
                payload.get("symptom_flag", 0)
            )

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

# ========== REST API ENDPOINTS ==========

@app.get("/")
async def root():
    return {"name": "BreathHome Dashboard API", "version": "1.0.0"}

@app.get("/api/rooms")
async def list_rooms():
    """List all rooms and their current AQI."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch("""
            SELECT DISTINCT ON (room_id) 
                room_id, timestamp, aqi_score, aqi_category,
                pm2_5, co2, voc_index, hcho, temperature, humidity,
                mold_risk_pct, radon_bq_m3
            FROM sensor_readings
            ORDER BY room_id, timestamp DESC
        """)
        return [dict(row) for row in rows]

@app.get("/api/rooms/{room_id}")
async def get_room_detail(room_id: int, hours: int = Query(24, ge=1, le=168)):
    """Get detailed air quality history for a room."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch("""
            SELECT * FROM sensor_readings
            WHERE room_id = $1 AND timestamp > NOW() - INTERVAL '$2 hours'
            ORDER BY timestamp ASC
        """, room_id, hours)
        return [dict(row) for row in rows]

@app.get("/api/rooms/{room_id}/latest")
async def get_room_latest(room_id: int):
    """Get the latest reading for a room."""
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow("""
            SELECT * FROM sensor_readings
            WHERE room_id = $1
            ORDER BY timestamp DESC LIMIT 1
        """, room_id)
        return dict(row) if row else {}

@app.get("/api/rooms/{room_id}/trends")
async def get_room_trends(room_id: int, hours: int = Query(168, ge=1, le=720)):
    """Get aggregated trends for a room (hourly averages)."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch("""
            SELECT 
                date_trunc('hour', timestamp) as hour,
                AVG(pm2_5) as avg_pm25,
                AVG(co2) as avg_co2,
                AVG(voc_index) as avg_voc,
                AVG(temperature) as avg_temp,
                AVG(humidity) as avg_humidity,
                AVG(aqi_score) as avg_aqi,
                MAX(aqi_score) as max_aqi,
                AVG(mold_risk_pct) as avg_mold_risk
            FROM sensor_readings
            WHERE room_id = $1 AND timestamp > NOW() - INTERVAL '$2 hours'
            GROUP BY hour
            ORDER BY hour ASC
        """, room_id, hours)
        return [dict(row) for row in rows]

@app.get("/api/exposure/{tag_id}")
async def get_exposure(tag_id: int, hours: int = Query(24, ge=1, le=168)):
    """Get personal exposure history for a wearable tag."""
    async with db_pool.acquire() as conn:
        rows = await conn.fetch("""
            SELECT * FROM exposure_data
            WHERE tag_id = $1 AND timestamp > NOW() - INTERVAL '$2 hours'
            ORDER BY timestamp ASC
        """, tag_id, hours)
        return [dict(row) for row in rows]

@app.get("/api/alerts")
async def get_alerts(limit: int = Query(50, ge=1, le=500), 
                     category: Optional[str] = None,
                     acknowledged: Optional[bool] = None):
    """Get recent alert events."""
    async with db_pool.acquire() as conn:
        query = "SELECT * FROM alert_events WHERE 1=1"
        params = []
        if category:
            query += " AND category = $1"
            params.append(category)
        if acknowledged is not None:
            query += f" AND acknowledged = ${len(params)+1}"
            params.append(acknowledged)
        query += " ORDER BY timestamp DESC LIMIT $" + str(len(params)+1)
        params.append(limit)
        
        rows = await conn.fetch(query, *params)
        return [dict(row) for row in rows]

@app.put("/api/alerts/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: int):
    """Acknowledge an alert."""
    async with db_pool.acquire() as conn:
        await conn.execute("""
            UPDATE alert_events SET acknowledged = TRUE WHERE id = $1
        """, alert_id)
    return {"status": "acknowledged"}

@app.get("/api/hvac/status")
async def get_hvac_status():
    """Get current HVAC controller state."""
    async with db_pool.acquire() as conn:
        row = await conn.fetchrow("""
            SELECT * FROM hvac_states
            ORDER BY timestamp DESC LIMIT 1
        """)
        return dict(row) if row else {}

@app.post("/api/hvac/command")
async def send_hvac_command(vent_positions: Optional[List[int]] = None,
                             purifier_speed: Optional[int] = None,
                             fan_override: Optional[bool] = None,
                             range_hood: Optional[bool] = None,
                             bathroom_exhaust: Optional[bool] = None):
    """Send a command to the HVAC controller via MQTT."""
    command = {}
    if vent_positions is not None:
        command["vent_positions"] = vent_positions
    if purifier_speed is not None:
        command["purifier_speed"] = purifier_speed
    if fan_override is not None:
        command["fan_override"] = fan_override
    if range_hood is not None:
        command["range_hood"] = range_hood
    if bathroom_exhaust is not None:
        command["bathroom_exhaust"] = bathroom_exhaust
    
    mqtt_client.publish("breathhome/hvac/1/cmd", json.dumps(command))
    return {"status": "sent", "command": command}

@app.get("/api/analytics/summary")
async def get_analytics_summary():
    """Get overall system analytics summary."""
    async with db_pool.acquire() as conn:
        # Current AQI across all rooms
        current_aqi = await conn.fetch("""
            SELECT DISTINCT ON (room_id) room_id, aqi_score, aqi_category,
                pm2_5, co2, voc_index, timestamp
            FROM sensor_readings
            ORDER BY room_id, timestamp DESC
        """)
        
        # Average AQI over last 24 hours
        avg_aqi = await conn.fetch("""
            SELECT room_id, AVG(aqi_score) as avg_aqi, MAX(aqi_score) as max_aqi,
                   MIN(aqi_score) as min_aqi
            FROM sensor_readings
            WHERE timestamp > NOW() - INTERVAL '24 hours'
            GROUP BY room_id
        """)
        
        # Alert count by category
        alert_counts = await conn.fetch("""
            SELECT category, COUNT(*) as count
            FROM alert_events
            WHERE timestamp > NOW() - INTERVAL '24 hours'
            GROUP BY category
        """)
        
        # Filter health
        filter_health = await conn.fetchrow("""
            SELECT filter_health_pct FROM hvac_states
            ORDER BY timestamp DESC LIMIT 1
        """)
        
        return {
            "current_aqi": [dict(r) for r in current_aqi],
            "avg_aqi_24h": [dict(r) for r in avg_aqi],
            "alert_counts_24h": [dict(r) for r in alert_counts],
            "filter_health": dict(filter_health) if filter_health else {}
        }

# ========== WEBSOCKET ==========

@app.websocket("/ws/realtime")
async def websocket_realtime(websocket: WebSocket):
    """WebSocket for real-time sensor data streaming."""
    await websocket.accept()
    alert_engine.websocket_connections.append(websocket)
    try:
        while True:
            data = await websocket.receive_text()
            # Client can request specific rooms or data types
            request = json.loads(data)
            # For now, just keep connection alive
    except WebSocketDisconnect:
        alert_engine.websocket_connections.remove(websocket)

# ========== STARTUP ==========

def start_mqtt():
    """Start MQTT client connection."""
    mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()

if __name__ == "__main__":
    import uvicorn
    start_mqtt()
    uvicorn.run(app, host="0.0.0.0", port=8000)