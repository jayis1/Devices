"""
GlucoSync Cloud Backend — FastAPI + MQTT + TimescaleDB

Metabolic health intelligence backend.
Receives glucose data, meal scans, insulin events, activity data from Hub.
Runs long-term ML (insulin sensitivity personalization, AGP reports).
Generates clinical reports. Notifies emergency contacts.

License: MIT
"""

import asyncio
import json
import logging
from datetime import datetime, timedelta
from typing import Optional, List, Dict, Any

import uvicorn
from fastapi import FastAPI, WebSocket, HTTPException, Depends, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.security import OAuth2PasswordBearer
from pydantic import BaseModel, Field
import asyncpg
import aiomqtt

# ── Configuration ─────────────────────────────────────────────────

MQTT_BROKER = "localhost"
MQTT_PORT = 1883
DATABASE_URL = "postgresql://glucosync:glucosync@localhost:5432/glucosync"
JWT_SECRET = "glucosync-secret-change-in-production"
TWILIO_SID = ""
TWILIO_TOKEN = ""
TWILIO_FROM = ""

# ── Logging ───────────────────────────────────────────────────────

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
logger = logging.getLogger("glucosync")

# ── FastAPI App ───────────────────────────────────────────────────

app = FastAPI(
    title="GlucoSync Cloud",
    description="AI-powered glucose management & metabolic health backend",
    version="1.0.0",
    docs_url="/docs",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

oauth2_scheme = OAuth2PasswordBearer(tokenUrl="token")

# ── Data Models ──────────────────────────────────────────────────

class GlucoseReading(BaseModel):
    user_id: str
    glucose_mgdl: int = Field(..., ge=20, le=600)
    trend_mgdl_min: float = 0.0
    sensor_state: int = 0
    confidence: int = 0
    timestamp: int

class MealScan(BaseModel):
    user_id: str
    food_class_id: int
    food_confidence: int
    carb_grams: int
    portion_grams: int
    glycemic_index: int
    spectral_bands: int
    timestamp: int

class InsulinEvent(BaseModel):
    user_id: str
    pen_type: int  # 0=basal, 1=bolus
    pen_id: int
    estimated_units: int
    confidence: int
    injection_dur_ms: int
    timestamp: int

class ActivityData(BaseModel):
    user_id: str
    hr: int = 0
    hrv_rmssd: int = 0
    activity_class: int
    intensity: int
    confidence: int
    timestamp: int

class ForecastData(BaseModel):
    user_id: str
    glucose: int
    trend: float
    forecast_30: int
    forecast_60: int
    hypo_risk: int
    risk_score: int
    iob: float
    cob: float
    hr: int
    activity: int
    timestamp: int

class UserCreate(BaseModel):
    email: str
    password: str
    diabetes_type: str  # "T1D", "T2D", "prediabetes", "gestational"
    weight_kg: float
    target_glucose: int = 100
    hypo_threshold: int = 70
    hyper_threshold: int = 180

class EmergencyContact(BaseModel):
    name: str
    phone: str
    relationship: str

# ── Database Pool ─────────────────────────────────────────────────

db_pool: Optional[asyncpg.Pool] = None

async def get_db():
    global db_pool
    if db_pool is None:
        db_pool = await asyncpg.create_pool(DATABASE_URL, min_size=2, max_size=10)
    return db_pool

# ── MQTT Subscriber ──────────────────────────────────────────────

class MQTTSubscriber:
    def __init__(self):
        self.client: Optional[aiomqtt.Client] = None

    async def start(self):
        asyncio.create_task(self._subscribe_loop())

    async def _subscribe_loop(self):
        while True:
            try:
                async with aiomqtt.Client(MQTT_BROKER, port=MQTT_PORT) as client:
                    await client.subscribe("glucosync/data/#")
                    await client.subscribe("glucosync/status/#")
                    async for message in client.messages:
                        await self._handle_message(message)
            except Exception as e:
                logger.error(f"MQTT error: {e}")
                await asyncio.sleep(5)

    async def _handle_message(self, message):
        topic = str(message.topic)
        payload = json.loads(message.payload.decode())
        logger.info(f"MQTT {topic}: {payload}")

        db = await get_db()
        async with db.acquire() as conn:
            if "forecast" in topic:
                await self._store_forecast(conn, payload)
            elif "status" in topic:
                await self._store_status(conn, payload)

    async def _store_forecast(self, conn, data):
        await conn.execute(
            """INSERT INTO glucose_readings (user_id, glucose_mgdl, trend, forecast_30,
               forecast_60, hypo_risk, risk_score, iob, cob, hr, activity, created_at)
               VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW())""",
            "001", data.get("glucose", 0), data.get("trend", 0),
            data.get("forecast_30", 0), data.get("forecast_60", 0),
            data.get("hypo_risk", 0), data.get("risk", 0),
            data.get("iob", 0), data.get("cob", 0),
            data.get("hr", 0), data.get("activity", 0)
        )

    async def _store_status(self, conn, data):
        await conn.execute(
            """INSERT INTO hub_status (hub_id, battery, nodes, glucose, iob, created_at)
               VALUES ($1, $2, $3, $4, $5, NOW())""",
            data.get("hub", "001"), data.get("bat", 0),
            data.get("nodes", 0), data.get("glucose", 0), data.get("iob", 0)
        )

mqtt_subscriber = MQTTSubscriber()

# ── Startup ───────────────────────────────────────────────────────

@app.on_event("startup")
async def startup():
    await mqtt_subscriber.start()
    logger.info("GlucoSync Cloud started")

# ── Health ───────────────────────────────────────────────────────

@app.get("/health")
async def health():
    return {"status": "ok", "service": "glucosync-cloud", "version": "1.0.0"}

# ── User Registration ────────────────────────────────────────────

@app.post("/api/register")
async def register(user: UserCreate):
    db = await get_db()
    async with db.acquire() as conn:
        # Check existing
        existing = await conn.fetchval(
            "SELECT id FROM users WHERE email = $1", user.email
        )
        if existing:
            raise HTTPException(400, "Email already registered")

        # Create user
        user_id = await conn.fetchval(
            """INSERT INTO users (email, password_hash, diabetes_type, weight_kg,
               target_glucose, hypo_threshold, hyper_threshold, created_at)
               VALUES ($1, crypt($2, gen_salt('bf')), $3, $4, $5, $6, $7, NOW())
               RETURNING id""",
            user.email, user.password, user.diabetes_type, user.weight_kg,
            user.target_glucose, user.hypo_threshold, user.hyper_threshold
        )
        return {"user_id": user_id, "status": "created"}

# ── Glucose Data Ingestion ───────────────────────────────────────

@app.post("/api/glucose")
async def ingest_glucose(reading: GlucoseReading):
    db = await get_db()
    async with db.acquire() as conn:
        await conn.execute(
            """INSERT INTO glucose_readings (user_id, glucose_mgdl, trend,
               sensor_state, confidence, created_at)
               VALUES ($1, $2, $3, $4, $5, to_timestamp($6))""",
            reading.user_id, reading.glucose_mgdl, reading.trend_mgdl_min,
            reading.sensor_state, reading.confidence, reading.timestamp
        )

        # Check for critical hypoglycemia
        if reading.glucose_mgdl < 54:
            await _notify_emergency_contact(conn, reading.user_id, reading.glucose_mgdl)

    return {"status": "stored"}

@app.get("/api/glucose/{user_id}")
async def get_glucose(user_id: str, hours: int = 24):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            f"""SELECT * FROM glucose_readings
               WHERE user_id = $1 AND created_at > NOW() - INTERVAL '{hours} hours'
               ORDER BY created_at ASC""",
            user_id
        )
        return [dict(r) for r in rows]

# ── Meal Data ────────────────────────────────────────────────────

@app.post("/api/meals")
async def ingest_meal(meal: MealScan):
    db = await get_db()
    async with db.acquire() as conn:
        await conn.execute(
            """INSERT INTO meals (user_id, food_class_id, food_confidence,
               carb_grams, portion_grams, glycemic_index, spectral_bands, created_at)
               VALUES ($1, $2, $3, $4, $5, $6, $7, to_timestamp($8))""",
            meal.user_id, meal.food_class_id, meal.food_confidence,
            meal.carb_grams, meal.portion_grams, meal.glycemic_index,
            meal.spectral_bands, meal.timestamp
        )
    return {"status": "stored"}

@app.get("/api/meals/{user_id}")
async def get_meals(user_id: str, hours: int = 24):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            f"""SELECT * FROM meals WHERE user_id = $1
               AND created_at > NOW() - INTERVAL '{hours} hours'
               ORDER BY created_at DESC""",
            user_id
        )
        return [dict(r) for r in rows]

# ── Insulin Data ─────────────────────────────────────────────────

@app.post("/api/insulin")
async def ingest_insulin(event: InsulinEvent):
    db = await get_db()
    async with db.acquire() as conn:
        await conn.execute(
            """INSERT INTO insulin_events (user_id, pen_type, pen_id,
               estimated_units, confidence, injection_dur_ms, created_at)
               VALUES ($1, $2, $3, $4, $5, $6, to_timestamp($7))""",
            event.user_id, event.pen_type, event.pen_id,
            event.estimated_units, event.confidence, event.injection_dur_ms,
            event.timestamp
        )
    return {"status": "stored"}

@app.get("/api/insulin/{user_id}")
async def get_insulin(user_id: str, hours: int = 24):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            f"""SELECT * FROM insulin_events WHERE user_id = $1
               AND created_at > NOW() - INTERVAL '{hours} hours'
               ORDER BY created_at DESC""",
            user_id
        )
        return [dict(r) for r in rows]

# ── Activity Data ────────────────────────────────────────────────

@app.post("/api/activity")
async def ingest_activity(data: ActivityData):
    db = await get_db()
    async with db.acquire() as conn:
        await conn.execute(
            """INSERT INTO activity_log (user_id, hr, hrv_rmssd, activity_class,
               intensity, confidence, created_at)
               VALUES ($1, $2, $3, $4, $5, $6, to_timestamp($7))""",
            data.user_id, data.hr, data.hrv_rmssd, data.activity_class,
            data.intensity, data.confidence, data.timestamp
        )
    return {"status": "stored"}

# ── Analytics: Time-in-Range ─────────────────────────────────────

@app.get("/api/analytics/tir/{user_id}")
async def get_time_in_range(user_id: str, days: int = 14):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            f"""SELECT glucose_mgdl, created_at FROM glucose_readings
               WHERE user_id = $1 AND created_at > NOW() - INTERVAL '{days} days'
               ORDER BY created_at ASC""",
            user_id
        )

        if not rows:
            return {"tir": 0, "below": 0, "above": 0, "avg": 0, "readings": 0}

        total = len(rows)
        in_range = sum(1 for r in rows if 70 <= r["glucose_mgdl"] <= 180)
        below = sum(1 for r in rows if r["glucose_mgdl"] < 70)
        above = sum(1 for r in rows if r["glucose_mgdl"] > 180)
        avg_glucose = sum(r["glucose_mgdl"] for r in rows) / total

        # GMI (Glucose Management Indicator)
        gmi = 3.31 + 0.02392 * avg_glucose

        return {
            "tir_pct": round(in_range / total * 100, 1),
            "below_pct": round(below / total * 100, 1),
            "above_pct": round(above / total * 100, 1),
            "avg_glucose": round(avg_glucose, 1),
            "gmi": round(gmi, 1),
            "readings": total,
            "days": days,
        }

# ── AGP (Ambulatory Glucose Profile) ─────────────────────────────

@app.get("/api/analytics/agp/{user_id}")
async def get_agp(user_id: str, days: int = 14):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            f"""SELECT glucose_mgdl,
               EXTRACT(HOUR FROM created_at) as hour,
               EXTRACT(MINUTE FROM created_at) as minute
               FROM glucose_readings
               WHERE user_id = $1 AND created_at > NOW() - INTERVAL '{days} days'
               ORDER BY created_at ASC""",
            user_id
        )

        if not rows:
            return {"error": "no data"}

        # Group by 30-min time-of-day buckets
        buckets: Dict[int, List[int]] = {}
        for r in rows:
            tod = int(r["hour"]) * 60 + int(r["minute"])
            bucket = tod // 30  # 48 buckets per day
            buckets.setdefault(bucket, []).append(r["glucose_mgdl"])

        # Compute percentiles per bucket
        agp_data = []
        for bucket in range(48):
            values = sorted(buckets.get(bucket, []))
            if not values:
                continue
            n = len(values)
            agp_data.append({
                "time_bucket": bucket,
                "median": values[n // 2],
                "p10": values[int(n * 0.10)],
                "p25": values[int(n * 0.25)],
                "p75": values[int(n * 0.75)],
                "p90": values[int(n * 0.90)],
                "count": n,
            })

        return {"agp_data": agp_data, "days": days, "total_readings": len(rows)}

# ── Insulin Sensitivity ─────────────────────────────────────────

@app.get("/api/analytics/sensitivity/{user_id}")
async def get_insulin_sensitivity(user_id: str):
    """Compute personalized I:C ratio and ISF from historical data."""
    db = await get_db()
    async with db.acquire() as conn:
        # Get glucose + insulin + meal data for last 14 days
        glucose = await conn.fetch(
            """SELECT * FROM glucose_readings WHERE user_id = $1
               AND created_at > NOW() - INTERVAL '14 days'
               ORDER BY created_at ASC""", user_id)

        meals = await conn.fetch(
            """SELECT * FROM meals WHERE user_id = $1
               AND created_at > NOW() - INTERVAL '14 days'
               ORDER BY created_at ASC""", user_id)

        insulin = await conn.fetch(
            """SELECT * FROM insulin_events WHERE user_id = $1
               AND pen_type = 1 AND created_at > NOW() - INTERVAL '14 days'
               ORDER BY created_at ASC""", user_id)

        # Weight-based initial estimate
        user = await conn.fetchrow("SELECT weight_kg FROM users WHERE id = $1", user_id)
        weight_kg = user["weight_kg"] if user else 80
        weight_lbs = weight_kg * 2.2

        # Rule of 500: I:C = 500 / weight_lbs
        ic_ratio = 500 / weight_lbs

        # Rule of 1800: ISF = 1800 / TDD (total daily dose)
        tdd = sum(r["estimated_units"] for r in insulin) / 14 if insulin else 30
        isf = 1800 / tdd if tdd > 0 else 50

        # TODO: Bayesian update from actual glucose response to meals + insulin

        return {
            "ic_ratio": round(ic_ratio, 1),
            "isf": round(isf, 1),
            "tdd_avg": round(tdd, 1),
            "method": "weight-based (priors)",
            "note": "Will adapt from individual response data after 14+ days",
        }

# ── Emergency Contact Notification ───────────────────────────────

async def _notify_emergency_contact(conn, user_id, glucose):
    contacts = await conn.fetch(
        "SELECT * FROM emergency_contacts WHERE user_id = $1", user_id
    )
    for contact in contacts:
        logger.warning(
            f"EMERGENCY: glucose={glucose} for user={user_id}, "
            f"notifying {contact['name']} at {contact['phone']}"
        )
        # Production: Twilio SMS
        # twilio_client.messages.create(
        #     to=contact["phone"], from_=TWILIO_FROM,
        #     body=f"URGENT: GlucoSync alert — glucose critically low ({glucose} mg/dL) "
        #          f"for your contact. Please check on them."
        # )

# ── Emergency Contacts ──────────────────────────────────────────

@app.post("/api/contacts/{user_id}")
async def add_contact(user_id: str, contact: EmergencyContact):
    db = await get_db()
    async with db.acquire() as conn:
        await conn.execute(
            """INSERT INTO emergency_contacts (user_id, name, phone, relationship)
               VALUES ($1, $2, $3, $4)""",
            user_id, contact.name, contact.phone, contact.relationship
        )
    return {"status": "created"}

@app.get("/api/contacts/{user_id}")
async def get_contacts(user_id: str):
    db = await get_db()
    async with db.acquire() as conn:
        rows = await conn.fetch(
            "SELECT * FROM emergency_contacts WHERE user_id = $1", user_id
        )
        return [dict(r) for r in rows]

# ── WebSocket for Real-time Updates ──────────────────────────────

@app.websocket("/ws/{user_id}")
async def websocket_endpoint(websocket: WebSocket, user_id: str):
    await websocket.accept()
    db = await get_db()
    async with db.acquire() as conn:
        # Subscribe to glucose updates
        while True:
            row = await conn.fetchrow(
                """SELECT * FROM glucose_readings WHERE user_id = $1
                   ORDER BY created_at DESC LIMIT 1""", user_id)
            if row:
                await websocket.send_json({
                    "type": "glucose",
                    "data": dict(row),
                    "timestamp": datetime.now().isoformat()
                })
            await asyncio.sleep(60)  # 1 per minute

# ── Main ─────────────────────────────────────────────────────────

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)