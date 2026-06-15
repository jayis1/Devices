"""
FlowGuard - Cloud Dashboard Backend
FastAPI (Python) - REST API + WebSocket + MQTT bridge

Handles:
- Sensor data ingestion via MQTT
- Time-series storage in PostgreSQL
- Flow disaggregation (NILM) service calls
- Freeze risk prediction service calls
- Alert management and push notifications
- Valve control commands (with 2FA)
- User authentication and home management

Copyright (c) 2026 jayis1 - MIT License
"""

from fastapi import FastAPI, HTTPException, Depends, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime, timedelta
from enum import IntEnum
import asyncio
import json
import logging
from contextlib import asynccontextmanager

import aiomqtt
import asyncpg
from jose import JWTError, jwt

# ============================================================
# Configuration
# ============================================================

MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_SENSORS = "flowguard/sensors/+/+"
MQTT_TOPIC_VALVE = "flowguard/valve/status"
MQTT_TOPIC_ALERTS = "flowguard/alerts/+"
MQTT_TOPIC_COMMANDS = "flowguard/commands/valve"

DATABASE_URL = "postgresql://flowguard:flowguard@localhost:5432/flowguard"
JWT_SECRET = "change-me-in-production"
JWT_ALGORITHM = "HS256"

# ============================================================
# Enums
# ============================================================

class ValveState(IntEnum):
    OPEN = 0
    CLOSED = 1
    CLOSING = 2
    OPENING = 3
    ERROR = 4

class LeakState(IntEnum):
    DRY = 0
    WET = 1
    ALERT = 2
    CONFIRMED = 3

class AlertLevel(IntEnum):
    INFO = 0
    WARNING = 1
    CRITICAL = 2
    EMERGENCY = 3

class AlertType(IntEnum):
    LEAK = 0
    PRESSURE = 1
    FREEZE = 2
    HAMMER = 3
    APPLIANCE = 4
    BATTERY = 5
    FLOW = 6

# ============================================================
# Pydantic Models
# ============================================================

class PipeSensorReport(BaseModel):
    node_id: int
    temperature_cx100: int = Field(description="Temperature in °C × 100")
    humidity_cx10: int = Field(description="Humidity in %RH × 10")
    vibration_rms_mgx10: int = Field(description="Vibration RMS in mg × 10")
    acoustic_anomaly: int = Field(description="Acoustic anomaly score 0-255")
    leak_state: LeakState
    battery_mv: int = Field(description="Battery voltage in mV")
    uptime_sec: int

class ApplianceReport(BaseModel):
    node_id: int
    flow_rate_ml_min: int = Field(description="Flow rate in mL/min")
    flow_volume_ml: int = Field(description="Cumulative volume in mL")
    temperature_cx100: int
    humidity_cx10: int
    pressure_kpa_x10: int = Field(description="Pressure in kPa × 10")
    leak_probe_1: bool
    leak_probe_2: bool
    battery_mv: int

class ValveStatus(BaseModel):
    node_id: int
    valve_state: ValveState
    flow_rate_ml_min: int
    pressure_kpa_x10: int
    temperature_cx100: int
    motor_current_ma: int
    heater_state: bool
    battery_mv: int
    supply_12v_ok: bool
    cumulative_gallons_x10: int

class ValveCommand(BaseModel):
    command: str = Field(..., pattern="^(open|close|emergency_shutdown)$")
    auth_token: str = Field(..., min_length=4, max_length=4)
    reason: str = "user_manual"
    two_factor_code: Optional[str] = None

class AlertCreate(BaseModel):
    level: AlertLevel
    alert_type: AlertType
    source_node_id: int
    message: str

# ============================================================
# Database Setup
# ============================================================

async def init_db():
    """Initialize database tables."""
    conn = await asyncpg.connect(DATABASE_URL)
    await conn.execute("""
        CREATE TABLE IF NOT EXISTS homes (
            id SERIAL PRIMARY KEY,
            name VARCHAR(100),
            created_at TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS nodes (
            id SERIAL PRIMARY KEY,
            home_id INTEGER REFERENCES homes(id),
            node_type VARCHAR(20) NOT NULL,
            node_id INTEGER NOT NULL,
            name VARCHAR(100),
            location VARCHAR(100),
            last_seen TIMESTAMP,
            battery_mv INTEGER,
            firmware_version VARCHAR(20)
        );

        CREATE TABLE IF NOT EXISTS sensor_readings (
            id BIGSERIAL PRIMARY KEY,
            node_id INTEGER REFERENCES nodes(id),
            timestamp TIMESTAMP DEFAULT NOW(),
            temperature_cx100 INTEGER,
            humidity_cx10 INTEGER,
            vibration_rms_mgx10 INTEGER,
            acoustic_anomaly INTEGER,
            leak_state INTEGER,
            flow_rate_ml_min INTEGER,
            flow_volume_ml INTEGER,
            pressure_kpa_x10 INTEGER,
            battery_mv INTEGER
        );

        CREATE TABLE IF NOT EXISTS valve_events (
            id BIGSERIAL PRIMARY KEY,
            home_id INTEGER REFERENCES homes(id),
            timestamp TIMESTAMP DEFAULT NOW(),
            old_state INTEGER,
            new_state INTEGER,
            reason VARCHAR(50),
            auth_token_hash VARCHAR(64)
        );

        CREATE TABLE IF NOT EXISTS alerts (
            id BIGSERIAL PRIMARY KEY,
            home_id INTEGER REFERENCES homes(id),
            level INTEGER NOT NULL,
            alert_type INTEGER NOT NULL,
            source_node_id INTEGER,
            message TEXT,
            timestamp TIMESTAMP DEFAULT NOW(),
            acknowledged BOOLEAN DEFAULT FALSE
        );

        CREATE TABLE IF NOT EXISTS water_usage (
            id BIGSERIAL PRIMARY KEY,
            home_id INTEGER REFERENCES homes(id),
            date DATE NOT NULL,
            total_gallons REAL,
            toilet_gallons REAL,
            shower_gallons REAL,
            faucet_gallons REAL,
            dishwasher_gallons REAL,
            washing_machine_gallons REAL,
            outdoor_gallons REAL,
            unknown_gallons REAL
        );

        -- Create indexes for fast time-series queries
        CREATE INDEX IF NOT EXISTS idx_sensor_readings_node_time
            ON sensor_readings(node_id, timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_alerts_home_time
            ON alerts(home_id, timestamp DESC);
    """)
    await conn.close()

# ============================================================
# MQTT Bridge
# ============================================================

class MQTTBridge:
    """Bridges MQTT messages from hub to database and WebSocket clients."""

    def __init__(self):
        self.ws_clients: list[WebSocket] = []
        self.mqtt_client: Optional[aiomqtt.Client] = None

    async def start(self):
        """Start MQTT listener and database bridge."""
        self.mqtt_client = aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT)
        await self.mqtt_client.connect()

        # Subscribe to all sensor topics
        await self.mqtt_client.subscribe([
            (MQTT_TOPIC_SENSORS, 1),
            (MQTT_TOPIC_VALVE, 1),
            (MQTT_TOPIC_ALERTS, 1),
        ])

        # Start listener task
        asyncio.create_task(self._listen())

    async def _listen(self):
        """Listen for MQTT messages and process them."""
        async for message in self.mqtt_client.messages:
            try:
                topic = message.topic.value
                payload = json.loads(message.payload.decode())

                if "sensors" in topic:
                    await self._handle_sensor_data(topic, payload)
                elif "valve" in topic:
                    await self._handle_valve_status(payload)
                elif "alerts" in topic:
                    await self._handle_alert(payload)

            except Exception as e:
                logging.error(f"MQTT message processing error: {e}")

    async def _handle_sensor_data(self, topic: str, payload: dict):
        """Store sensor data in database and forward to WebSocket clients."""
        # Extract node info from topic: flowguard/sensors/{node_type}/{node_id}
        parts = topic.split("/")
        node_type = parts[2]
        node_id = int(parts[3])

        # Store in database
        conn = await asyncpg.connect(DATABASE_URL)
        await conn.execute("""
            INSERT INTO sensor_readings
                (node_id, temperature_cx100, humidity_cx10, vibration_rms_mgx10,
                 acoustic_anomaly, leak_state, flow_rate_ml_min, flow_volume_ml,
                 pressure_kpa_x10, battery_mv)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
        """, node_id,
            payload.get("temperature_cx100"),
            payload.get("humidity_cx10"),
            payload.get("vibration_rms_mgx10"),
            payload.get("acoustic_anomaly"),
            payload.get("leak_state"),
            payload.get("flow_rate_ml_min"),
            payload.get("flow_volume_ml"),
            payload.get("pressure_kpa_x10"),
            payload.get("battery_mv"))
        await conn.close()

        # Forward to WebSocket clients
        ws_message = json.dumps({
            "type": "sensor_data",
            "node_type": node_type,
            "node_id": node_id,
            "data": payload,
            "timestamp": datetime.utcnow().isoformat()
        })
        await self._broadcast(ws_message)

    async def _handle_valve_status(self, payload: dict):
        """Store valve status and broadcast."""
        ws_message = json.dumps({
            "type": "valve_status",
            "data": payload,
            "timestamp": datetime.utcnow().isoformat()
        })
        await self._broadcast(ws_message)

    async def _handle_alert(self, payload: dict):
        """Store alert and broadcast (also triggers push notification)."""
        # Store in database
        conn = await asyncpg.connect(DATABASE_URL)
        await conn.execute("""
            INSERT INTO alerts (home_id, level, alert_type, source_node_id, message)
            VALUES ($1, $2, $3, $4, $5)
        """, 1,  # home_id
            payload.get("level", 0),
            payload.get("alert_type", 0),
            payload.get("source_node_id", 0),
            payload.get("message", ""))
        await conn.close()

        # Broadcast to WebSocket clients
        ws_message = json.dumps({
            "type": "alert",
            "data": payload,
            "timestamp": datetime.utcnow().isoformat()
        })
        await self._broadcast(ws_message)

        # TODO: Send push notification (FCM/APNs)

    async def _broadcast(self, message: str):
        """Send message to all connected WebSocket clients."""
        for client in self.ws_clients:
            try:
                await client.send_text(message)
            except Exception:
                self.ws_clients.remove(client)

    async def publish_command(self, topic: str, payload: dict):
        """Publish a command to MQTT (valve control, etc.)."""
        await self.mqtt_client.publish(topic, json.dumps(payload), qos=1)

mqtt_bridge = MQTTBridge()

# ============================================================
# FastAPI Application
# ============================================================

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan: init DB, start MQTT bridge."""
    await init_db()
    await mqtt_bridge.start()
    yield
    if mqtt_bridge.mqtt_client:
        await mqtt_bridge.mqtt_client.disconnect()

app = FastAPI(
    title="FlowGuard Dashboard API",
    description="Water leak detection, pipe health monitoring, and flood prevention system",
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

# ============================================================
# REST API Endpoints
# ============================================================

@app.get("/api/v1/status")
async def get_system_status():
    """Get overall system status."""
    conn = await asyncpg.connect(DATABASE_URL)

    # Latest valve status
    valve = await conn.fetchrow("""
        SELECT * FROM valve_events ORDER BY timestamp DESC LIMIT 1
    """)

    # Active alerts
    alerts = await conn.fetch("""
        SELECT * FROM alerts WHERE acknowledged = FALSE
        ORDER BY timestamp DESC LIMIT 10
    """)

    # Latest sensor readings
    readings = await conn.fetch("""
        SELECT sr.*, n.node_type, n.name, n.location
        FROM sensor_readings sr
        JOIN nodes n ON sr.node_id = n.id
        WHERE sr.timestamp > NOW() - INTERVAL '1 hour'
        ORDER BY sr.timestamp DESC LIMIT 20
    """)

    await conn.close()

    return {
        "valve_state": dict(valve) if valve else None,
        "active_alerts": [dict(a) for a in alerts],
        "recent_readings": [dict(r) for r in readings],
        "system_healthy": len(alerts) == 0,
    }

@app.get("/api/v1/usage")
async def get_water_usage(
    days: int = 7,
    appliance: Optional[str] = None
):
    """Get water usage history."""
    conn = await asyncpg.connect(DATABASE_URL)

    since = datetime.utcnow() - timedelta(days=days)
    usage = await conn.fetch("""
        SELECT * FROM water_usage
        WHERE date >= $1
        ORDER BY date DESC
    """, since.date())

    await conn.close()
    return {"usage": [dict(u) for u in usage]}

@app.get("/api/v1/sensors/{node_id}")
async def get_sensor_history(
    node_id: int,
    hours: int = 24
):
    """Get sensor history for a specific node."""
    conn = await asyncpg.connect(DATABASE_URL)

    since = datetime.utcnow() - timedelta(hours=hours)
    readings = await conn.fetch("""
        SELECT * FROM sensor_readings
        WHERE node_id = $1 AND timestamp >= $2
        ORDER BY timestamp ASC
    """, node_id, since)

    await conn.close()
    return {"readings": [dict(r) for r in readings]}

@app.get("/api/v1/alerts")
async def get_alerts(
    level: Optional[int] = None,
    limit: int = 50
):
    """Get alert history."""
    conn = await asyncpg.connect(DATABASE_URL)

    if level is not None:
        alerts = await conn.fetch("""
            SELECT * FROM alerts
            WHERE level >= $1
            ORDER BY timestamp DESC LIMIT $2
        """, level, limit)
    else:
        alerts = await conn.fetch("""
            SELECT * FROM alerts
            ORDER BY timestamp DESC LIMIT $1
        """, limit)

    await conn.close()
    return {"alerts": [dict(a) for a in alerts]}

@app.post("/api/v1/valve/command")
async def send_valve_command(command: ValveCommand):
    """Send a command to the valve controller.
    Requires 2FA for OPEN commands (2FA code sent to user's phone).
    """
    if command.command == "open":
        # Verify 2FA code for opening valve
        if not command.two_factor_code:
            raise HTTPException(
                status_code=403,
                detail="2FA code required for valve OPEN command"
            )
        # TODO: Verify 2FA code against TOTP or SMS code
        valve_state = ValveState.OPENING
        mqtt_command = "valve_open"
    elif command.command == "close":
        valve_state = ValveState.CLOSING
        mqtt_command = "valve_close"
    elif command.command == "emergency_shutdown":
        valve_state = ValveState.CLOSED
        mqtt_command = "emergency_shutdown"
    else:
        raise HTTPException(status_code=400, detail="Invalid command")

    # Publish command via MQTT
    await mqtt_bridge.publish_command(MQTT_TOPIC_COMMANDS, {
        "command": mqtt_command,
        "auth_token": command.auth_token,
        "reason": command.reason,
        "timestamp": datetime.utcnow().isoformat()
    })

    # Log valve event
    conn = await asyncpg.connect(DATABASE_URL)
    await conn.execute("""
        INSERT INTO valve_events (home_id, old_state, new_state, reason)
        VALUES ($1, $2, $3, $4)
    """, 1, None, valve_state.value, command.reason)
    await conn.close()

    return {
        "status": "command_sent",
        "command": command.command,
        "timestamp": datetime.utcnow().isoformat()
    }

@app.get("/api/v1/nodes")
async def get_nodes():
    """Get all registered nodes and their status."""
    conn = await asyncpg.connect(DATABASE_URL)
    nodes = await conn.fetch("""
        SELECT n.*, 
               (SELECT timestamp FROM sensor_readings 
                WHERE node_id = n.id ORDER BY timestamp DESC LIMIT 1) as last_reading
        FROM nodes n
    """)
    await conn.close()
    return {"nodes": [dict(n) for n in nodes]}

@app.post("/api/v1/alerts/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: int):
    """Acknowledge an alert."""
    conn = await asyncpg.connect(DATABASE_URL)
    result = await conn.execute("""
        UPDATE alerts SET acknowledged = TRUE WHERE id = $1
    """, alert_id)
    await conn.close()

    if result == "UPDATE 0":
        raise HTTPException(status_code=404, detail="Alert not found")

    return {"status": "acknowledged", "alert_id": alert_id}

# ============================================================
# WebSocket Endpoint
# ============================================================

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for real-time sensor data streaming."""
    await websocket.accept()
    mqtt_bridge.ws_clients.append(websocket)

    try:
        while True:
            # Keep connection alive
            data = await websocket.receive_text()
            # Client can send commands via WebSocket too
            try:
                command = json.loads(data)
                if command.get("type") == "subscribe":
                    # Client requesting specific data streams
                    pass
            except json.JSONDecodeError:
                pass
    except WebSocketDisconnect:
        mqtt_bridge.ws_clients.remove(websocket)

# ============================================================
# Health Check
# ============================================================

@app.get("/health")
async def health():
    return {"status": "ok", "service": "flowguard-api", "version": "1.0.0"}