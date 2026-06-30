"""
AsthmaSync — Cloud Backend (FastAPI)
=====================================
Receives telemetry from hubs via MQTT, stores in TimescaleDB,
serves REST API for mobile app, runs ML inference.

Endpoints:
  GET  /api/v1/risk          — 7-day exacerbation risk forecast
  GET  /api/v1/triggers       — personal trigger attribution
  GET  /api/v1/adherence      — medication adherence summary
  GET  /api/v1/events          — recent events (wheeze, actuation, alerts)
  GET  /api/v1/trends          — time-series trends (PM2.5, HRV, etc.)
  GET  /api/v1/action-plan     — GINA-aligned action plan
  POST /api/v1/event           — manual event log (symptom, peak flow)
  GET  /api/v1/report           — PDF clinical report

License: MIT
"""

import os
import json
import logging
from datetime import datetime, timedelta, timezone
from contextlib import asynccontextmanager
from typing import Optional

from fastapi import FastAPI, HTTPException, Depends, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import Response
from pydantic import BaseModel, Field
import asyncpg
import aiomqtt

# ── Configuration ────────────────────────────────────────
DB_URL = os.getenv("DATABASE_URL", "postgresql://asthmasync:asthmasync@localhost/asthmasync")
MQTT_BROKER = os.getenv("MQTT_BROKER", "broker.asthmasync.io")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TELEMETRY_TOPIC = "asthmasync/telemetry"
MQTT_EVENTS_TOPIC = "asthmasync/events"

logger = logging.getLogger("asthmasync.backend")
logging.basicConfig(level=logging.INFO)

# ── Database Pool ────────────────────────────────────────
db_pool: Optional[asyncpg.Pool] = None


async def get_db():
    async with db_pool.acquire() as conn:
        yield conn


# ── MQTT Subscriber (background task) ────────────────────
async def mqtt_subscriber():
    """Subscribe to hub telemetry and insert into TimescaleDB."""
    while True:
        try:
            async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
                await client.subscribe(MQTT_TELEMETRY_TOPIC)
                await client.subscribe(MQTT_EVENTS_TOPIC)
                logger.info("MQTT subscriber connected")

                async for message in client.messages:
                    try:
                        payload = json.loads(message.payload.decode())
                        await process_mqtt_message(payload)
                    except Exception as e:
                        logger.error(f"MQTT message error: {e}")
        except Exception as e:
            logger.error(f"MQTT connection failed: {e}, retrying in 5s...")
            import asyncio
            await asyncio.sleep(5)


async def process_mqtt_message(payload: dict):
    """Route MQTT message to appropriate DB insert."""
    async with db_pool.acquire() as conn:
        msg_type = payload.get("type")
        node_id = payload.get("node_id", 0)
        ts = payload.get("ts", datetime.now(timezone.utc))

        if msg_type == "air_quality":
            await conn.execute("""
                INSERT INTO air_quality (timestamp, node_id, pm25, pm10, voc_index, co2_ppm, temp_c, humidity_pct)
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
            """, ts, node_id, payload.get("pm25", 0), payload.get("pm10", 0),
                payload.get("voc", 0), payload.get("co2", 0),
                payload.get("temp", 0), payload.get("rh", 0))

        elif msg_type == "vitals":
            await conn.execute("""
                INSERT INTO vitals (timestamp, node_id, hr, spo2, hrv_rmssd, skin_temp, activity)
                VALUES ($1, $2, $3, $4, $5, $6, $7)
            """, ts, node_id, payload.get("hr", 0), payload.get("spo2", 0),
                payload.get("hrv", 0), payload.get("skin_temp", 0),
                payload.get("activity", 0))

        elif msg_type == "audio":
            await conn.execute("""
                INSERT INTO audio_events (timestamp, node_id, wheeze_prob, snr_db)
                VALUES ($1, $2, $3, $4)
            """, ts, node_id, payload.get("wheeze_prob", 0),
                payload.get("snr", 0))

        elif msg_type == "actuation":
            await conn.execute("""
                INSERT INTO actuations (timestamp, node_id, confidence, peak_accel, duration_ms, battery_pct)
                VALUES ($1, $2, $3, $4, $5, $6)
            """, ts, node_id, payload.get("confidence", 0),
                payload.get("peak_accel", 0), payload.get("duration_ms", 0),
                payload.get("battery", 0))

        elif msg_type == "alert":
            await conn.execute("""
                INSERT INTO alerts (timestamp, zone, message)
                VALUES ($1, $2, $3)
            """, ts, payload.get("zone", 0), payload.get("message", ""))


# ── Pydantic Models ──────────────────────────────────────
class RiskForecast(BaseModel):
    risk_score: float = Field(..., description="0-100 risk score")
    risk_level: str = Field(..., description="low/moderate/high")
    confidence: float = Field(..., description="0-1 model confidence")
    forecast_days: int = 7
    contributing_factors: list[dict]
    trend: str = Field(..., description="improving/stable/declining")


class AdherenceSummary(BaseModel):
    rescue_count_7d: int
    rescue_count_30d: int
    controller_adherence_pct: float
    last_rescue: Optional[str]
    gina_controlled: bool


class TriggerAttribution(BaseModel):
    trigger: str
    contribution_pct: float
    exposure_level: str
    recommendation: str


class EventLog(BaseModel):
    timestamp: datetime
    event_type: str
    severity: int
    message: str


class ManualEvent(BaseModel):
    event_type: str = Field(..., description="symptom|peak_flow|exercise|medication")
    value: Optional[float] = None
    note: Optional[str] = None


# ── App Lifecycle ────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    global db_pool
    db_pool = await asyncpg.create_pool(
        DB_URL, min_size=2, max_size=10, command_timeout=30
    )
    await init_db(db_pool)

    import asyncio
    mqtt_task = asyncio.create_task(mqtt_subscriber())

    logger.info("AsthmaSync backend started")
    yield

    mqtt_task.cancel()
    await db_pool.close()
    logger.info("AsthmaSync backend stopped")


app = FastAPI(
    title="AsthmaSync API",
    version="1.0.0",
    description="AI-powered asthma management backend",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


async def init_db(pool: asyncpg.Pool):
    """Create tables and hypertables."""
    async with pool.acquire() as conn:
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS air_quality (
                timestamp TIMESTAMPTZ NOT NULL,
                node_id INT,
                pm25 FLOAT, pm10 FLOAT,
                voc_index INT, co2_ppm INT,
                temp_c FLOAT, humidity_pct FLOAT
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS vitals (
                timestamp TIMESTAMPTZ NOT NULL,
                node_id INT,
                hr INT, spo2 INT, hrv_rmssd FLOAT,
                skin_temp FLOAT, activity INT
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS audio_events (
                timestamp TIMESTAMPTZ NOT NULL,
                node_id INT,
                wheeze_prob INT, snr_db INT
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS actuations (
                timestamp TIMESTAMPTZ NOT NULL,
                node_id INT,
                confidence INT, peak_accel FLOAT,
                duration_ms INT, battery_pct INT
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS alerts (
                timestamp TIMESTAMPTZ NOT NULL,
                zone INT, message TEXT
            );
        """)
        await conn.execute("""
            CREATE TABLE IF NOT EXISTS manual_events (
                timestamp TIMESTAMPTZ DEFAULT NOW(),
                event_type TEXT, value FLOAT, note TEXT
            );
        """)
        # Convert to TimescaleDB hypertables
        for table in ["air_quality", "vitals", "audio_events", "actuations", "alerts"]:
            try:
                await conn.execute(f"SELECT create_hypertable('{table}', 'timestamp', if_not_exists => TRUE);")
            except Exception:
                pass  # already a hypertable or TimescaleDB not installed
        logger.info("Database initialized")


# ── API Endpoints ────────────────────────────────────────

@app.get("/api/v1/health")
async def health():
    return {"status": "ok", "service": "asthmasync", "version": "1.0.0"}


@app.get("/api/v1/risk", response_model=RiskForecast)
async def get_risk(db=Depends(get_db)):
    """7-day exacerbation risk forecast (LSTM)."""
    from .ml_service import compute_risk_forecast
    forecast = await compute_risk_forecast(db)
    return forecast


@app.get("/api/v1/triggers", response_model=list[TriggerAttribution])
async def get_triggers(db=Depends(get_db)):
    """Personal trigger attribution (XGBoost SHAP)."""
    from .ml_service import compute_trigger_attribution
    triggers = await compute_trigger_attribution(db)
    return triggers


@app.get("/api/v1/adherence", response_model=AdherenceSummary)
async def get_adherence(db=Depends(get_db)):
    """Medication adherence summary."""
    now = datetime.now(timezone.utc)
    seven_days_ago = now - timedelta(days=7)
    thirty_days_ago = now - timedelta(days=30)

    count_7d = await db.fetchval(
        "SELECT COUNT(*) FROM actuations WHERE timestamp > $1", seven_days_ago)
    count_30d = await db.fetchval(
        "SELECT COUNT(*) FROM actuations WHERE timestamp > $1", thirty_days_ago)
    last = await db.fetchrow(
        "SELECT timestamp FROM actuations ORDER BY timestamp DESC LIMIT 1")

    last_str = last["timestamp"].isoformat() if last else None
    gina_controlled = count_7d <= 2

    return AdherenceSummary(
        rescue_count_7d=count_7d or 0,
        rescue_count_30d=count_30d or 0,
        controller_adherence_pct=85.0,  # TODO: from controller med tracking
        last_rescue=last_str,
        gina_controlled=gina_controlled,
    )


@app.get("/api/v1/events", response_model=list[EventLog])
async def get_events(
    limit: int = Query(50, ge=1, le=500),
    db=Depends(get_db)
):
    """Recent events (wheeze, actuation, alerts)."""
    rows = await db.fetch("""
        SELECT timestamp, 'wheeze' AS event_type, 1 AS severity,
               'Wheeze detected' AS message
        FROM audio_events WHERE wheeze_prob > 65
        UNION ALL
        SELECT timestamp, 'actuation' AS event_type, 1 AS severity,
               'Rescue inhaler used' AS message
        FROM actuations
        UNION ALL
        SELECT timestamp, 'alert' AS event_type, 2 AS severity,
               message FROM alerts
        ORDER BY timestamp DESC LIMIT $1
    """, limit)
    return [EventLog(**dict(r)) for r in rows]


@app.get("/api/v1/trends")
async def get_trends(
    metric: str = Query("pm25", regex="^(pm25|co2|voc|hr|spo2|hrv|wheeze_prob)$"),
    hours: int = Query(24, ge=1, le=168),
    db=Depends(get_db)
):
    """Time-series trend data."""
    since = datetime.now(timezone.utc) - timedelta(hours=hours)

    table_map = {
        "pm25": ("air_quality", "pm25"),
        "co2": ("air_quality", "co2_ppm"),
        "voc": ("air_quality", "voc_index"),
        "hr": ("vitals", "hr"),
        "spo2": ("vitals", "spo2"),
        "hrv": ("vitals", "hrv_rmssd"),
        "wheeze_prob": ("audio_events", "wheeze_prob"),
    }

    table, column = table_map.get(metric, ("air_quality", "pm25"))
    rows = await db.fetch(f"""
        SELECT date_trunc('hour', timestamp) AS hour, AVG({column}) AS avg_val, MAX({column}) AS max_val
        FROM {table} WHERE timestamp > $1
        GROUP BY hour ORDER BY hour
    """, since)

    return {
        "metric": metric,
        "hours": hours,
        "data": [
            {"timestamp": r["hour"].isoformat(), "avg": float(r["avg_val"] or 0),
             "max": float(r["max_val"] or 0)}
            for r in rows
        ],
    }


@app.get("/api/v1/action-plan")
async def get_action_plan(db=Depends(get_db)):
    """GINA-aligned action plan based on current data."""
    seven_days_ago = datetime.now(timezone.utc) - timedelta(days=7)
    rescue_count = await db.fetchval(
        "SELECT COUNT(*) FROM actuations WHERE timestamp > $1", seven_days_ago)
    last_spo2 = await db.fetchval(
        "SELECT spo2 FROM vitals ORDER BY timestamp DESC LIMIT 1")

    if last_spo2 and last_spo2 < 92:
        zone = "red"
        steps = [
            "Use rescue inhaler immediately",
            "Sit upright, remain calm",
            "Call emergency services if no improvement in 15 minutes",
            "Contact your pulmonologist today",
        ]
    elif rescue_count and rescue_count > 4:
        zone = "red"
        steps = [
            "Step up controller medication (follow your action plan)",
            "Monitor symptoms closely",
            "Contact your doctor within 24 hours",
            "Avoid known triggers",
        ]
    elif rescue_count and rescue_count > 2:
        zone = "yellow"
        steps = [
            "Review trigger exposure (check app for air quality alerts)",
            "Ensure controller medication adherence",
            "Consider doubling controller dose per your action plan",
            "Monitor for worsening symptoms",
        ]
    else:
        zone = "green"
        steps = [
            "Continue current medication regimen",
            "Maintain good inhaler technique",
            "Stay active and exercise regularly",
        ]

    return {
        "zone": zone,
        "rescue_use_7d": rescue_count or 0,
        "last_spo2": last_spo2 or 0,
        "steps": steps,
        "last_updated": datetime.now(timezone.utc).isoformat(),
    }


@app.post("/api/v1/event")
async def log_event(event: ManualEvent, db=Depends(get_db)):
    """Log a manual event (symptom, peak flow reading, etc.)."""
    await db.execute("""
        INSERT INTO manual_events (event_type, value, note)
        VALUES ($1, $2, $3)
    """, event.event_type, event.value, event.note)
    return {"status": "logged", "event_type": event.event_type}


@app.get("/api/v1/report")
async def get_report(db=Depends(get_db)):
    """Generate PDF clinical report (returns JSON placeholder for now)."""
    thirty_days_ago = datetime.now(timezone.utc) - timedelta(days=30)

    rescue_count = await db.fetchval(
        "SELECT COUNT(*) FROM actuations WHERE timestamp > $1", thirty_days_ago)
    wheeze_count = await db.fetchval(
        "SELECT COUNT(*) FROM audio_events WHERE wheeze_prob > 65 AND timestamp > $1",
        thirty_days_ago)
    avg_pm25 = await db.fetchval(
        "SELECT AVG(pm25) FROM air_quality WHERE timestamp > $1", thirty_days_ago)
    avg_spo2 = await db.fetchval(
        "SELECT AVG(spo2) FROM vitals WHERE timestamp > $1", thirty_days_ago)
    alert_count = await db.fetchval(
        "SELECT COUNT(*) FROM alerts WHERE timestamp > $1", thirty_days_ago)

    return {
        "report_type": "30-day asthma summary",
        "generated": datetime.now(timezone.utc).isoformat(),
        "period": "30 days",
        "summary": {
            "rescue_inhaler_uses": rescue_count or 0,
            "wheeze_events": wheeze_count or 0,
            "avg_pm25_exposure": float(avg_pm25 or 0),
            "avg_spo2": float(avg_spo2 or 0),
            "alert_count": alert_count or 0,
            "gina_controlled": (rescue_count or 0) <= 8,
        },
        "note": "Full PDF report generation requires reportlab. See /scripts/generate_report.py",
    }


# ── Run ──────────────────────────────────────────────────
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)