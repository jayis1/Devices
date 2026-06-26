"""
HiveSync — Cloud Backend API
FastAPI application for beehive monitoring, ML inference, and alerting
"""

from fastapi import FastAPI, HTTPException, Depends, Query
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from typing import List, Optional
from datetime import datetime, timedelta
from enum import IntEnum
import asyncpg
import redis
import json

app = FastAPI(
    title="HiveSync API",
    description="AI-powered beehive health monitoring and management system",
    version="1.0.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# ---- Database ----
# In production, use asyncpg pool with proper migrations
# This is the schema definition for reference

DB_SCHEMA = """
CREATE TABLE IF NOT EXISTS apiaries (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    location_lat FLOAT,
    location_lon FLOAT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS hives (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    apiary_id UUID REFERENCES apiaries(id),
    node_id SMALLINT UNIQUE NOT NULL,
    name VARCHAR(255),
    hive_type VARCHAR(50) DEFAULT 'langstroth',
    installed_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS sensor_readings (
    id BIGSERIAL PRIMARY KEY,
    hive_id UUID REFERENCES hives(id),
    timestamp TIMESTAMPTZ DEFAULT NOW(),
    temp_brood_c REAL,
    temp_top_c REAL,
    temp_entrance_c REAL,
    humidity_pct REAL,
    weight_kg REAL,
    weight_delta_g REAL,
    accel_rms_mg REAL,
    battery_mv REAL,
    spectral_centroid_hz REAL,
    peak_freq_hz REAL,
    peak_amplitude_db REAL,
    spectral_bandwidth_hz REAL
);

CREATE TABLE IF NOT EXISTS entrance_readings (
    id BIGSERIAL PRIMARY KEY,
    hive_id UUID REFERENCES hives(id),
    timestamp TIMESTAMPTZ DEFAULT NOW(),
    bees_in INT,
    bees_out INT,
    mites_per_bee REAL,
    mite_class SMALLINT,
    entrance_temp_c REAL
);

CREATE TABLE IF NOT EXISTS feeder_status (
    id BIGSERIAL PRIMARY KEY,
    hive_id UUID REFERENCES hives(id),
    timestamp TIMESTAMPTZ DEFAULT NOW(),
    weight_kg REAL,
    temp_c REAL,
    humidity_pct REAL,
    state SMALLINT,
    valve_open BOOLEAN,
    clog_detected BOOLEAN,
    battery_mv REAL,
    dispense_count INT
);

CREATE TABLE IF NOT EXISTS alerts (
    id BIGSERIAL PRIMARY KEY,
    hive_id UUID REFERENCES hives(id),
    timestamp TIMESTAMPTZ DEFAULT NOW(),
    alert_type VARCHAR(50) NOT NULL,
    severity VARCHAR(20) NOT NULL,
    message TEXT,
    swarm_risk REAL,
    mite_class SMALLINT,
    queen_status VARCHAR(30),
    acknowledged BOOLEAN DEFAULT FALSE
);

CREATE INDEX ON sensor_readings (hive_id, timestamp DESC);
CREATE INDEX ON entrance_readings (hive_id, timestamp DESC);
CREATE INDEX ON alerts (hive_id, timestamp DESC);
"""


# ---- Pydantic Models ----

class NodeType(IntEnum):
    SENSOR = 1
    ENTRANCE_MONITOR = 2
    SMART_FEEDER = 3

class AlertSeverity(str):
    CRITICAL = "critical"
    HIGH = "high"
    MEDIUM = "medium"
    LOW = "low"
    INFO = "info"

class SensorReading(BaseModel):
    node_id: int = Field(..., description="Node address on Sub-GHz mesh")
    timestamp: datetime = Field(default_factory=datetime.utcnow)
    temp_brood_c: float
    temp_top_c: float
    temp_entrance_c: float
    humidity_pct: float
    weight_kg: float
    weight_delta_g: float
    accel_rms_mg: float
    battery_mv: float
    spectral_centroid_hz: float
    peak_freq_hz: float
    peak_amplitude_db: float
    spectral_bandwidth_hz: float

class EntranceReading(BaseModel):
    node_id: int
    timestamp: datetime = Field(default_factory=datetime.utcnow)
    bees_in: int
    bees_out: int
    mites_per_bee: float
    mite_class: int = Field(..., ge=0, le=3)
    entrance_temp_c: float

class FeederStatus(BaseModel):
    node_id: int
    timestamp: datetime = Field(default_factory=datetime.utcnow)
    weight_kg: float
    temp_c: float
    humidity_pct: float
    state: int
    valve_open: bool
    clog_detected: bool
    battery_mv: float
    dispense_count: int

class BatchReading(BaseModel):
    readings: List[SensorReading]

class SwarmRisk(BaseModel):
    probability_24h: float
    probability_72h: float
    probability_7d: float
    recommendation: str

class HiveHealth(BaseModel):
    score: float = Field(..., ge=0, le=100)
    queen_status: str
    swarm_risk: float
    mite_level: str
    weight_trend: str
    forager_traffic: str
    alerts: List[dict]

class FeedCommand(BaseModel):
    ml_syrup: Optional[float] = None
    mm_patty: Optional[float] = None
    valve_open: Optional[bool] = None


# ---- API Endpoints ----

@app.get("/api/v1/apiaries", tags=["apiaries"])
async def list_apiaries():
    """List all apiaries with hive counts and overall health."""
    # In production: query database
    return {"apiaries": []}

@app.post("/api/v1/apiaries", tags=["apiaries"])
async def create_apiary(name: str, lat: float, lon: float):
    """Register a new apiary."""
    return {"id": "uuid-placeholder", "name": name, "lat": lat, "lon": lon}

@app.get("/api/v1/hives", tags=["hives"])
async def list_hives(apiary_id: str = Query(...)):
    """List all hives in an apiary."""
    return {"hives": []}

@app.get("/api/v1/hives/{hive_id}/health", tags=["hives"], response_model=HiveHealth)
async def get_hive_health(hive_id: str):
    """Get composite health score for a hive (0-100)."""
    # In production: aggregate ML predictions + recent data
    return HiveHealth(
        score=78.5,
        queen_status="healthy",
        swarm_risk=0.12,
        mite_level="low",
        weight_trend="stable",
        forager_traffic="normal",
        alerts=[]
    )

@app.get("/api/v1/hives/{hive_id}/swarm-risk", tags=["hives"], response_model=SwarmRisk)
async def get_swarm_risk(hive_id: str):
    """Get 7-day swarm probability forecast."""
    return SwarmRisk(
        probability_24h=0.05,
        probability_72h=0.18,
        probability_7d=0.32,
        recommendation="Monitor closely. Consider adding a super if space is limited."
    )

@app.post("/api/v1/readings/batch", tags=["readings"])
async def ingest_readings(batch: BatchReading):
    """Ingest a batch of sensor readings from gateway."""
    # In production: insert into TimescaleDB, trigger ML pipeline
    count = len(batch.readings)
    return {"ingested": count, "timestamp": datetime.utcnow().isoformat()}

@app.post("/api/v1/readings/entrance", tags=["readings"])
async def ingest_entrance_reading(reading: EntranceReading):
    """Ingest entrance monitor reading (bee counts, mite detection)."""
    return {"ingested": True, "node_id": reading.node_id}

@app.post("/api/v1/readings/feeder", tags=["readings"])
async def ingest_feeder_status(status: FeederStatus):
    """Ingest feeder status update."""
    return {"ingested": True, "node_id": status.node_id}

@app.post("/api/v1/hives/{hive_id}/feed", tags=["hives"])
async def command_feeder(hive_id: str, cmd: FeedCommand):
    """Command feeder to dispense syrup or advance patty."""
    commands = []
    if cmd.ml_syrup:
        commands.append(f"DISPENSE_SYRUP:{cmd.ml_syrup:.0f}mL")
    if cmd.mm_patty:
        commands.append(f"ADVANCE_PATTY:{cmd.mm_patty:.1f}mm")
    if cmd.valve_open is not None:
        commands.append(f"VALVE_{'OPEN' if cmd.valve_open else 'CLOSE'}")
    return {"hive_id": hive_id, "commands_queued": commands}

@app.get("/api/v1/hives/{hive_id}/readings", tags=["readings"])
async def get_readings(
    hive_id: str,
    metric: str = Query("temp_brood_c"),
    hours: int = Query(24, le=720),
):
    """Get time-series readings for a specific metric."""
    return {"hive_id": hive_id, "metric": metric, "hours": hours, "data": []}

@app.get("/api/v1/alerts", tags=["alerts"])
async def list_alerts(
    apiary_id: Optional[str] = None,
    severity: Optional[str] = None,
    acknowledged: bool = False,
):
    """List alerts, filterable by apiary, severity, acknowledged status."""
    return {"alerts": []}

@app.post("/api/v1/alerts/{alert_id}/acknowledge", tags=["alerts"])
async def acknowledge_alert(alert_id: int):
    """Mark an alert as acknowledged."""
    return {"acknowledged": True, "alert_id": alert_id}

@app.post("/api/v1/alerts/subscribe", tags=["alerts"])
async def subscribe_push_notifications(
    device_token: str,
    platform: str = Query(..., regex="^(ios|android)$"),
    apiary_id: Optional[str] = None,
    min_severity: str = Query("medium"),
):
    """Subscribe to push notifications for alerts."""
    return {"subscribed": True, "platform": platform}

@app.get("/api/v1/apiaries/{apiary_id}/dashboard", tags=["dashboard"])
async def get_apiary_dashboard(apiary_id: str):
    """Get multi-hive overview for an apiary."""
    return {
        "apiary_id": apiary_id,
        "hives": [],
        "weather": {"temp_c": 22, "humidity": 45, "wind_kph": 8},
        "summary": {
            "total_hives": 0,
            "healthy": 0,
            "attention_needed": 0,
            "critical": 0,
        }
    }


# ---- ML Inference Endpoints ----

@app.post("/api/v1/ml/swarm-predict", tags=["ml"])
async def predict_swarm(readings: List[SensorReading]):
    """Run swarm prediction model on provided readings."""
    # In production: call SwarmPredictor LSTM model
    return {"swarm_probability": 0.0, "confidence": 0.0}

@app.post("/api/v1/ml/varroa-detect", tags=["ml"])
async def detect_varroa(image_data: bytes):
    """Run Varroa mite detection on entrance camera frame."""
    # In production: call VarroaDetector EfficientNet model
    return {"mites_per_bee": 0.0, "mite_class": 0}

@app.post("/api/v1/ml/queen-health", tags=["ml"])
async def assess_queen(readings: List[SensorReading]):
    """Assess queen health from temperature + acoustic features."""
    # In production: call QueenHealth model
    return {"queen_status": "healthy", "confidence": 0.0}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)