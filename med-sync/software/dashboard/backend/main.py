"""
MedSync - Cloud Dashboard Backend
FastAPI (Python) - REST API + WebSocket + MQTT bridge

Handles:
- Medication schedule management and distribution
- Dose verification and adherence tracking
- Vital signs monitoring and anomaly detection
- Fall detection alert routing
- Caregiver notification and alert management
- Mobile app data synchronization

Copyright (c) 2026 jayis1 - MIT License
"""

from fastapi import FastAPI, HTTPException, Depends, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import Optional, List
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
MQTT_TOPIC_VITALS = "medsync/vitals/+/+"
MQTT_TOPIC_DOSE = "medsync/dose/+/+"
MQTT_TOPIC_ALERTS = "medsync/alerts/+"
MQTT_TOPIC_SCHEDULE = "medsync/schedule/+"
MQTT_TOPIC_COMMANDS = "medsync/commands/+"

DATABASE_URL = "postgresql://medsync:***@localhost:5432/medsync"
JWT_SECRET = "change-me-in-production"
JWT_ALGORITHM = "HS256"

# ============================================================
# Enums
# ============================================================

class DoseStatus(IntEnum):
    PENDING = 0
    DISPENSED = 1
    PROBABLY_TAKEN = 2
    CONFIRMED = 3
    OVERDUE = 4
    MISSED = 5
    SKIPPED = 6

class AlertLevel(IntEnum):
    INFO = 0
    REMINDER = 1
    WARNING = 2
    URGENT = 3
    EMERGENCY = 4

class AlertType(IntEnum):
    DOSE_DUE = 0
    DOSE_OVERDUE = 1
    DOSE_MISSED = 2
    FALL = 3
    ADVERSE_EFFECT = 4
    BATTERY = 5
    MOTOR_FAULT = 6
    REFILL = 7
    SYSTEM = 8

class ConfirmMethod(IntEnum):
    WEIGHT = 0
    IR = 1
    NFC = 2
    BUTTON = 3
    APP = 4
    COVER = 5

class ActivityLevel(IntEnum):
    STILL = 0
    WALKING = 1
    RUNNING = 2
    SLEEPING = 3
    UNKNOWN = 255

# ============================================================
# Pydantic Models
# ============================================================

class MedicationBase(BaseModel):
    name: str = Field(..., max_length=64)
    generic_name: Optional[str] = None
    dose_count: int = Field(..., ge=1, le=10)
    pill_weight_mg: int = Field(..., gt=0)
    food_instruction: str = Field("anytime", pattern="^(anytime|before_food|with_food|after_food)$")
    color: Optional[str] = Field(None, pattern="^#[0-9a-fA-F]{6}$")
    notes: Optional[str] = None

class ScheduleEntryCreate(BaseModel):
    patient_id: int
    medication_id: int
    bin_id: int = Field(..., ge=0, le=7)
    hour: int = Field(..., ge=0, le=23)
    minute: int = Field(..., ge=0, le=59)
    frequency: str = Field("daily", pattern="^(daily|weekly|as_needed|custom)$")
    days_of_week: Optional[str] = None  # e.g., "1,3,5" for Mon/Wed/Fri

class DoseEventCreate(BaseModel):
    patient_id: int
    medication_id: int
    bin_id: int
    scheduled_time: datetime
    actual_time: Optional[datetime] = None
    status: DoseStatus
    confirm_method: Optional[ConfirmMethod] = None
    weight_change_mg: Optional[int] = None

class VitalsReport(BaseModel):
    node_id: int
    heart_rate_bpm: int = Field(..., ge=30, le=250)
    spo2_percent: int = Field(..., ge=70, le=100)
    activity_level: ActivityLevel
    fall_detected: bool
    steps_count: int = Field(..., ge=0)
    skin_temp_cx100: Optional[int] = None
    battery_mv: int
    timestamp: Optional[datetime] = None

class AlertCreate(BaseModel):
    level: AlertLevel
    alert_type: AlertType
    source_node_id: int
    bin_id: Optional[int] = None
    medication_id: Optional[int] = None
    message: str

class CaregiverCreate(BaseModel):
    name: str
    phone: str
    email: str
    relationship: str
    notify_dose_missed: bool = True
    notify_fall: bool = True
    notify_vitals_abnormal: bool = True
    notify_refill: bool = True

# ============================================================
# Database Setup
# ============================================================

async def init_db():
    """Initialize database tables."""
    conn = await asyncpg.connect(DATABASE_URL)
    await conn.execute("""
        CREATE TABLE IF NOT EXISTS patients (
            id SERIAL PRIMARY KEY,
            name VARCHAR(100) NOT NULL,
            birth_date DATE,
            created_at TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS medications (
            id SERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            name VARCHAR(64) NOT NULL,
            generic_name VARCHAR(64),
            dose_count INTEGER DEFAULT 1,
            pill_weight_mg INTEGER NOT NULL,
            food_instruction VARCHAR(20) DEFAULT 'anytime',
            color VARCHAR(7),
            notes TEXT,
            created_at TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS schedules (
            id SERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            medication_id INTEGER REFERENCES medications(id),
            bin_id INTEGER CHECK (bin_id BETWEEN 0 AND 7),
            hour INTEGER CHECK (hour BETWEEN 0 AND 23),
            minute INTEGER CHECK (minute BETWEEN 0 AND 59),
            frequency VARCHAR(20) DEFAULT 'daily',
            days_of_week VARCHAR(20),
            is_active BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS dose_events (
            id BIGSERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            medication_id INTEGER REFERENCES medications(id),
            bin_id INTEGER,
            scheduled_time TIMESTAMP NOT NULL,
            actual_time TIMESTAMP,
            status INTEGER DEFAULT 0,
            confirm_method INTEGER,
            weight_change_mg INTEGER,
            created_at TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS vitals_readings (
            id BIGSERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            node_id INTEGER,
            heart_rate_bpm INTEGER,
            spo2_percent INTEGER,
            activity_level INTEGER DEFAULT 255,
            fall_detected BOOLEAN DEFAULT FALSE,
            steps_count INTEGER DEFAULT 0,
            skin_temp_cx100 INTEGER,
            battery_mv INTEGER,
            timestamp TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS alerts (
            id BIGSERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            level INTEGER NOT NULL,
            alert_type INTEGER NOT NULL,
            source_node_id INTEGER,
            bin_id INTEGER,
            medication_id INTEGER,
            message TEXT,
            acknowledged BOOLEAN DEFAULT FALSE,
            timestamp TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS caregivers (
            id SERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            name VARCHAR(100) NOT NULL,
            phone VARCHAR(20) NOT NULL,
            email VARCHAR(100) NOT NULL,
            relationship VARCHAR(50),
            notify_dose_missed BOOLEAN DEFAULT TRUE,
            notify_fall BOOLEAN DEFAULT TRUE,
            notify_vitals_abnormal BOOLEAN DEFAULT TRUE,
            notify_refill BOOLEAN DEFAULT TRUE,
            created_at TIMESTAMP DEFAULT NOW()
        );

        CREATE TABLE IF NOT EXISTS adherence_daily (
            id SERIAL PRIMARY KEY,
            patient_id INTEGER REFERENCES patients(id),
            date DATE NOT NULL,
            doses_scheduled INTEGER DEFAULT 0,
            doses_taken INTEGER DEFAULT 0,
            doses_missed INTEGER DEFAULT 0,
            adherence_pct REAL DEFAULT 0,
            created_at TIMESTAMP DEFAULT NOW()
        );

        -- Create indexes for fast queries
        CREATE INDEX IF NOT EXISTS idx_dose_events_patient_time
            ON dose_events(patient_id, scheduled_time DESC);
        CREATE INDEX IF NOT EXISTS idx_vitals_patient_time
            ON vitals_readings(patient_id, timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_alerts_patient_time
            ON alerts(patient_id, timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_adherence_patient_date
            ON adherence_daily(patient_id, date DESC);
    """)
    await conn.close()

# ============================================================
# MQTT Bridge
# ============================================================

class MQTTBridge:
    """Bridges MQTT messages from hub to database and WebSocket clients."""

    def __init__(self):
        self.ws_clients: list[WebSocket] = []
        self.mqtt_client: aiomqtt.Client | None = None

    async def start(self):
        """Start MQTT listener and database bridge."""
        self.mqtt_client = aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT)
        await self.mqtt_client.connect()

        await self.mqtt_client.subscribe([
            (MQTT_TOPIC_VITALS, 1),
            (MQTT_TOPIC_DOSE, 1),
            (MQTT_TOPIC_ALERTS, 1),
            (MQTT_TOPIC_SCHEDULE, 1),
        ])

        asyncio.create_task(self._listen())

    async def _listen(self):
        """Listen for MQTT messages and process them."""
        async for message in self.mqtt_client.messages:
            try:
                topic = message.topic.value
                payload = json.loads(message.payload.decode())

                if "vitals" in topic:
                    await self._handle_vitals(payload)
                elif "dose" in topic:
                    await self._handle_dose(payload)
                elif "alerts" in topic:
                    await self._handle_alert(payload)

            except Exception as e:
                logging.error(f"MQTT message processing error: {e}")

    async def _handle_vitals(self, payload: dict):
        """Store vitals data and check for anomalies."""
        # Store in database
        conn = await asyncpg.connect(DATABASE_URL)
        await conn.execute("""
            INSERT INTO vitals_readings
                (patient_id, node_id, heart_rate_bpm, spo2_percent,
                 activity_level, fall_detected, steps_count,
                 skin_temp_cx100, battery_mv)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
        """, 1,  # patient_id
            payload.get("node_id"),
            payload.get("heart_rate_bpm"),
            payload.get("spo2_percent"),
            payload.get("activity_level", 255),
            payload.get("fall_detected", False),
            payload.get("steps_count", 0),
            payload.get("skin_temp_cx100"),
            payload.get("battery_mv"))
        await conn.close()

        # Check for abnormal vitals
        spo2 = payload.get("spo2_percent", 100)
        hr = payload.get("heart_rate_bpm", 70)

        if spo2 < 88:
            # Emergency: SpO2 critically low
            await self._send_alert(AlertLevel.EMERGENCY, AlertType.ADVERSE_EFFECT,
                                    "Blood oxygen critically low: {}%".format(spo2))
        elif spo2 < 92:
            # Warning: SpO2 below normal
            await self._send_alert(AlertLevel.WARNING, AlertType.ADVERSE_EFFECT,
                                    "Blood oxygen below normal: {}%".format(spo2))

        if hr > 130 or hr < 45:
            await self._send_alert(AlertLevel.WARNING, AlertType.ADVERSE_EFFECT,
                                    "Abnormal heart rate: {} BPM".format(hr))

        # Fall detection
        if payload.get("fall_detected", False):
            await self._send_alert(AlertLevel.EMERGENCY, AlertType.FALL,
                                    "Fall detected! Immediate assistance may be needed.")

        # Broadcast to WebSocket clients
        ws_message = json.dumps({
            "type": "vitals",
            "data": payload,
            "timestamp": datetime.utcnow().isoformat()
        })
        await self._broadcast(ws_message)

    async def _handle_dose(self, payload: dict):
        """Store dose event and update adherence tracking."""
        conn = await asyncpg.connect(DATABASE_URL)
        await conn.execute("""
            INSERT INTO dose_events
                (patient_id, medication_id, bin_id, scheduled_time,
                 actual_time, status, confirm_method, weight_change_mg)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
        """, 1,  # patient_id
            payload.get("medication_id"),
            payload.get("bin_id"),
            payload.get("scheduled_time"),
            payload.get("actual_time"),
            payload.get("status", 0),
            payload.get("confirm_method"),
            payload.get("weight_change_mg"))
        await conn.close()

        # Broadcast to WebSocket clients
        ws_message = json.dumps({
            "type": "dose_event",
            "data": payload,
            "timestamp": datetime.utcnow().isoformat()
        })
        await self._broadcast(ws_message)

    async def _handle_alert(self, payload: dict):
        """Store alert and send push notification."""
        conn = await asyncpg.connect(DATABASE_URL)
        await conn.execute("""
            INSERT INTO alerts
                (patient_id, level, alert_type, source_node_id,
                 bin_id, medication_id, message)
            VALUES ($1, $2, $3, $4, $5, $6, $7)
        """, 1,
            payload.get("level", 0),
            payload.get("alert_type", 0),
            payload.get("source_node_id"),
            payload.get("bin_id"),
            payload.get("medication_id"),
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
        # TODO: Send SMS to caregiver for URGENT and EMERGENCY

    async def _send_alert(self, level: AlertLevel, alert_type: AlertType, message: str):
        """Create and distribute an alert."""
        alert = {
            "level": level,
            "alert_type": alert_type,
            "source_node_id": 0,
            "message": message,
            "timestamp": datetime.utcnow().isoformat()
        }
        await self.mqtt_client.publish("medsync/alerts/hub", json.dumps(alert), qos=1)
        await self._handle_alert(alert)

    async def _broadcast(self, message: str):
        """Send message to all connected WebSocket clients."""
        for client in self.ws_clients:
            try:
                await client.send_text(message)
            except Exception:
                self.ws_clients.remove(client)

    async def publish_command(self, topic: str, payload: dict):
        """Publish a command to MQTT."""
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
    title="MedSync Dashboard API",
    description="AI-powered medication adherence and health monitoring system",
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

    # Today's adherence
    today = datetime.utcnow().date()
    adherence = await conn.fetchrow("""
        SELECT * FROM adherence_daily
        WHERE patient_id = $1 AND date = $2
    """, 1, today)

    # Active alerts
    alerts = await conn.fetch("""
        SELECT * FROM alerts
        WHERE patient_id = $1 AND acknowledged = FALSE
        ORDER BY timestamp DESC LIMIT 10
    """, 1)

    # Latest vitals
    vitals = await conn.fetchrow("""
        SELECT * FROM vitals_readings
        WHERE patient_id = $1
        ORDER BY timestamp DESC LIMIT 1
    """, 1)

    # Next scheduled dose
    next_dose = await conn.fetchrow("""
        SELECT s.*, m.name as medication_name, m.dose_count, m.food_instruction
        FROM schedules s
        JOIN medications m ON s.medication_id = m.id
        WHERE s.patient_id = $1 AND s.is_active = TRUE
        ORDER BY s.hour, s.minute
        LIMIT 1
    """, 1)

    await conn.close()

    return {
        "adherence_today": dict(adherence) if adherence else None,
        "active_alerts": [dict(a) for a in alerts],
        "latest_vitals": dict(vitals) if vitals else None,
        "next_dose": dict(next_dose) if next_dose else None,
        "system_healthy": len(alerts) == 0,
    }

@app.get("/api/v1/schedule")
async def get_schedule(patient_id: int = 1):
    """Get medication schedule for a patient."""
    conn = await asyncpg.connect(DATABASE_URL)

    schedules = await conn.fetch("""
        SELECT s.*, m.name as medication_name, m.generic_name,
               m.dose_count, m.pill_weight_mg, m.food_instruction, m.color
        FROM schedules s
        JOIN medications m ON s.medication_id = m.id
        WHERE s.patient_id = $1 AND s.is_active = TRUE
        ORDER BY s.hour, s.minute
    """, patient_id)

    await conn.close()
    return {"schedules": [dict(s) for s in schedules]}

@app.post("/api/v1/schedule")
async def create_schedule(entry: ScheduleEntryCreate):
    """Create a new medication schedule entry."""
    conn = await asyncpg.connect(DATABASE_URL)

    result = await conn.fetchrow("""
        INSERT INTO schedules
            (patient_id, medication_id, bin_id, hour, minute, frequency, days_of_week)
        VALUES ($1, $2, $3, $4, $5, $6, $7)
        RETURNING *
    """, entry.patient_id, entry.medication_id, entry.bin_id,
         entry.hour, entry.minute, entry.frequency, entry.days_of_week)

    await conn.close()

    # Push schedule update to hub via MQTT
    await mqtt_bridge.publish_command("medsync/commands/hub", {
        "command": "schedule_update",
        "schedule_id": result["id"],
        "timestamp": datetime.utcnow().isoformat()
    })

    return {"schedule": dict(result)}

@app.get("/api/v1/vitals")
async def get_vitals_history(patient_id: int = 1, hours: int = 24):
    """Get vitals history for a patient."""
    conn = await asyncpg.connect(DATABASE_URL)

    since = datetime.utcnow() - timedelta(hours=hours)
    vitals = await conn.fetch("""
        SELECT * FROM vitals_readings
        WHERE patient_id = $1 AND timestamp >= $2
        ORDER BY timestamp ASC
    """, patient_id, since)

    await conn.close()
    return {"vitals": [dict(v) for v in vitals]}

@app.get("/api/v1/adherence")
async def get_adherence(patient_id: int = 1, days: int = 30):
    """Get adherence history for a patient."""
    conn = await asyncpg.connect(DATABASE_URL)

    since = datetime.utcnow() - timedelta(days=days)
    adherence = await conn.fetch("""
        SELECT * FROM adherence_daily
        WHERE patient_id = $1 AND date >= $2
        ORDER BY date DESC
    """, patient_id, since.date())

    await conn.close()
    return {"adherence": [dict(a) for a in adherence]}

@app.get("/api/v1/doses")
async def get_dose_history(patient_id: int = 1, days: int = 7):
    """Get dose event history."""
    conn = await asyncpg.connect(DATABASE_URL)

    since = datetime.utcnow() - timedelta(days=days)
    doses = await conn.fetch("""
        SELECT d.*, m.name as medication_name
        FROM dose_events d
        JOIN medications m ON d.medication_id = m.id
        WHERE d.patient_id = $1 AND d.scheduled_time >= $2
        ORDER BY d.scheduled_time DESC
    """, patient_id, since)

    await conn.close()
    return {"doses": [dict(d) for d in doses]}

@app.post("/api/v1/dose/confirm")
async def confirm_dose(patient_id: int, medication_id: int,
                        bin_id: int, method: ConfirmMethod = ConfirmMethod.APP):
    """Manually confirm a dose was taken (from mobile app)."""
    conn = await asyncpg.connect(DATABASE_URL)

    # Find the most recent pending dose for this medication
    dose = await conn.fetchrow("""
        SELECT * FROM dose_events
        WHERE patient_id = $1 AND medication_id = $2 AND bin_id = $3
          AND status = 0
        ORDER BY scheduled_time DESC LIMIT 1
    """, patient_id, medication_id, bin_id)

    if not dose:
        await conn.close()
        raise HTTPException(status_code=404, detail="No pending dose found")

    # Update dose status
    await conn.execute("""
        UPDATE dose_events
        SET status = 3, confirm_method = $1, actual_time = $2
        WHERE id = $3
    """, method, datetime.utcnow(), dose["id"])

    await conn.close()

    # Send confirmation to hub
    await mqtt_bridge.publish_command("medsync/commands/hub", {
        "command": "dose_confirmed",
        "patient_id": patient_id,
        "medication_id": medication_id,
        "bin_id": bin_id,
        "method": method,
        "timestamp": datetime.utcnow().isoformat()
    })

    return {"status": "confirmed", "dose_id": dose["id"]}

@app.get("/api/v1/alerts")
async def get_alerts(patient_id: int = 1, level: Optional[int] = None, limit: int = 50):
    """Get alert history."""
    conn = await asyncpg.connect(DATABASE_URL)

    if level is not None:
        alerts = await conn.fetch("""
            SELECT * FROM alerts
            WHERE patient_id = $1 AND level >= $2
            ORDER BY timestamp DESC LIMIT $3
        """, patient_id, level, limit)
    else:
        alerts = await conn.fetch("""
            SELECT * FROM alerts
            WHERE patient_id = $1
            ORDER BY timestamp DESC LIMIT $2
        """, patient_id, limit)

    await conn.close()
    return {"alerts": [dict(a) for a in alerts]}

@app.post("/api/v1/alerts/{alert_id}/acknowledge")
async def acknowledge_alert(alert_id: int):
    """Acknowledge an alert."""
    conn = await asyncpg.connect(DATABASE_URL)

    result = await conn.execute("""
        UPDATE alerts SET acknowledged = TRUE WHERE id = $1
    """, alert_id)

    await conn.close()
    return {"status": "acknowledged", "alert_id": alert_id}

@app.get("/api/v1/caregivers")
async def get_caregivers(patient_id: int = 1):
    """Get caregivers for a patient."""
    conn = await asyncpg.connect(DATABASE_URL)

    caregivers = await conn.fetch("""
        SELECT * FROM caregivers WHERE patient_id = $1
    """, patient_id)

    await conn.close()
    return {"caregivers": [dict(c) for c in caregivers]}

@app.post("/api/v1/caregivers")
async def add_caregiver(caregiver: CaregiverCreate):
    """Add a caregiver for a patient."""
    conn = await asyncpg.connect(DATABASE_URL)

    result = await conn.fetchrow("""
        INSERT INTO caregivers
            (patient_id, name, phone, email, relationship,
             notify_dose_missed, notify_fall, notify_vitals_abnormal, notify_refill)
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
        RETURNING *
    """, 1,  # patient_id
         caregiver.name, caregiver.phone, caregiver.email, caregiver.relationship,
         caregiver.notify_dose_missed, caregiver.notify_fall,
         caregiver.notify_vitals_abnormal, caregiver.notify_refill)

    await conn.close()
    return {"caregiver": dict(result)}

# ============================================================
# WebSocket Endpoint
# ============================================================

@app.websocket("/ws/v1/live")
async def websocket_endpoint(websocket: WebSocket):
    """Real-time data stream for dashboard and mobile app."""
    await websocket.accept()
    mqtt_bridge.ws_clients.append(websocket)

    try:
        while True:
            # Keep connection alive, receive client commands
            data = await websocket.receive_text()
            # Could handle client commands here (e.g., dose confirmation)
    except WebSocketDisconnect:
        mqtt_bridge.ws_clients.remove(websocket)