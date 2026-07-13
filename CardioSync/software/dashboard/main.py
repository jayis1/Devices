"""
CardioSync Cloud Backend — FastAPI + MQTT + TimescaleDB

Central cloud server for the CardioSync system:
  - Receives ECG streams, BP records, PPG data via MQTT
  - Stores all data in TimescaleDB (hypertables for time-series)
  - Runs cloud ML pipeline (BP trend, stroke risk, sleep apnea)
  - Generates cardiologist-ready PDF reports
  - Dispatches emergency alerts
  - Serves REST API + WebSocket for mobile app

License: MIT
"""
import os
import json
import time
import asyncio
from datetime import datetime, timedelta, timezone
from typing import Optional, List
from contextlib import asynccontextmanager

import paho.mqtt.client as mqtt
import redis
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Depends, HTTPException
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from passlib.context import CryptContext
from jose import jwt, JWTError
import sqlalchemy
from sqlalchemy import create_engine, text
from sqlalchemy.orm import sessionmaker, Session
import numpy as np

# ── Configuration ─────────────────────────────────────────────
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://cardiosync:cardiosync@localhost:5432/cardiosync")
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
REDIS_URL = os.getenv("REDIS_URL", "redis://localhost:6379/0")
SECRET_KEY = os.getenv("SECRET_KEY", "cardiosync-dev-secret-key-change-in-production")
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 1440  # 24 hours

# ── Database ──────────────────────────────────────────────────
engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

# ── Redis ─────────────────────────────────────────────────────
redis_client = redis.from_url(REDIS_URL, decode_responses=True)

# ── Security ─────────────────────────────────────────────────
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")

# ── MQTT Client ───────────────────────────────────────────────
mqtt_client = mqtt.Client()
mqtt_connected = False

def on_mqtt_connect(client, userdata, flags, rc):
    global mqtt_connected
    mqtt_connected = (rc == 0)
    print(f"MQTT connected: {mqtt_connected}")
    client.subscribe("cardiosync/+/hub/#")

def on_mqtt_message(client, userdata, msg):
    """Handle incoming MQTT messages from Hub"""
    topic = msg.topic
    payload = json.loads(msg.payload.decode())
    parts = topic.split("/")

    if len(parts) < 4:
        return

    user_id = parts[1]
    data_type = parts[3]

    db = SessionLocal()
    try:
        if data_type == "ecg":
            handle_ecg_message(db, user_id, payload)
        elif data_type == "events":
            handle_event_message(db, user_id, payload)
        elif data_type == "bp":
            handle_bp_message(db, user_id, payload)
        elif data_type == "ppg":
            handle_ppg_message(db, user_id, payload)
        elif data_type == "alerts":
            handle_emergency_alert(db, user_id, payload)
    finally:
        db.close()

def handle_ecg_message(db, user_id, payload):
    """Store ECG stream data"""
    samples = payload.get("samples", [])
    ts = payload.get("timestamp", datetime.now(timezone.utc).isoformat())
    for i, sample in enumerate(samples):
        db.execute(text(
            "INSERT INTO ecg_stream (user_id, sample_value, timestamp) "
            "VALUES (:uid, :val, :ts)"
        ), {"uid": int(user_id), "val": int(sample),
            "ts": datetime.now(timezone.utc)})
    db.commit()

def handle_event_message(db, user_id, payload):
    """Store arrhythmia event"""
    db.execute(text(
        "INSERT INTO ecg_events (user_id, event_type, confidence, heart_rate_bpm, ecg_strip, timestamp) "
        "VALUES (:uid, :type, :conf, :hr, :strip, :ts)"
    ), {
        "uid": int(user_id),
        "type": payload.get("event_type"),
        "conf": payload.get("confidence"),
        "hr": payload.get("heart_rate"),
        "strip": json.dumps(payload.get("ecg_strip", [])),
        "ts": datetime.now(timezone.utc),
    })
    db.commit()

    # Publish to Redis for real-time WebSocket
    redis_client.publish(f"cardiosync:{user_id}:events", json.dumps(payload))

def handle_bp_message(db, user_id, payload):
    """Store BP record"""
    db.execute(text(
        "INSERT INTO bp_records (user_id, systolic, diastolic, map, heart_rate, "
        "position_ok, quality, schedule_id, timestamp) "
        "VALUES (:uid, :sys, :dia, :map, :hr, :pos, :qual, :sched, :ts)"
    ), {
        "uid": int(user_id),
        "sys": payload.get("systolic"),
        "dia": payload.get("diastolic"),
        "map": payload.get("map"),
        "hr": payload.get("heart_rate"),
        "pos": payload.get("position_ok"),
        "qual": payload.get("quality"),
        "sched": payload.get("schedule_id"),
        "ts": datetime.now(timezone.utc),
    })
    db.commit()

def handle_ppg_message(db, user_id, payload):
    """Store PPG HR data"""
    db.execute(text(
        "INSERT INTO ppg_hr (user_id, heart_rate, spo2, skin_temp_c10, timestamp) "
        "VALUES (:uid, :hr, :spo2, :temp, :ts)"
    ), {
        "uid": int(user_id),
        "hr": payload.get("heart_rate"),
        "spo2": payload.get("spo2"),
        "temp": payload.get("skin_temp_c10"),
        "ts": datetime.now(timezone.utc),
    })
    db.commit()

def handle_emergency_alert(db, user_id, payload):
    """Handle emergency alert — dispatch contacts"""
    alert_type = payload.get("alert_type")
    hr = payload.get("heart_rate")

    # Store alert
    db.execute(text(
        "INSERT INTO emergency_alerts (user_id, alert_type, heart_rate, timestamp) "
        "VALUES (:uid, :type, :hr, :ts)"
    ), {
        "uid": int(user_id), "type": alert_type, "hr": hr,
        "ts": datetime.now(timezone.utc),
    })
    db.commit()

    # Notify emergency contacts (in production: SMS via Twilio, push via FCM)
    print(f"EMERGENCY ALERT: User {user_id} — {alert_type} HR={hr}")
    redis_client.publish(f"cardiosync:{user_id}:emergency", json.dumps(payload))

# ── MQTT Init ─────────────────────────────────────────────────
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

def start_mqtt():
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"MQTT connection failed: {e}")

# ── FastAPI App ──────────────────────────────────────────────
@asynccontextmanager
async def lifespan(app: FastAPI):
    start_mqtt()
    yield
    mqtt_client.loop_stop()

app = FastAPI(
    title="CardioSync API",
    description="AI-powered cardiovascular health & arrhythmia detection system",
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

# ── Database Dependency ──────────────────────────────────────
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# ── Auth ─────────────────────────────────────────────────────
def create_access_token(data: dict):
    to_encode = data.copy()
    expire = datetime.now(timezone.utc) + timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)

def get_current_user(token: str = Depends(oauth2_scheme), db: Session = Depends(get_db)):
    credentials_exception = HTTPException(
        status_code=401, detail="Could not validate credentials")
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        user_id: int = payload.get("sub")
        if user_id is None:
            raise credentials_exception
    except JWTError:
        raise credentials_exception
    return user_id

# ── Models ────────────────────────────────────────────────────
class UserRegister(BaseModel):
    username: str
    email: str
    password: str
    full_name: Optional[str] = None

class BPRecord(BaseModel):
    systolic: int
    diastolic: int
    map: int
    heart_rate: Optional[int] = None

class StrokeRiskResponse(BaseModel):
    stroke_risk_30d: float
    afib_burden_pct: float
    bp_category: str
    hrv_trend: str
    risk_factors: dict

class MonthlyReportResponse(BaseModel):
    report_id: str
    period: str
    download_url: str

# ── API Endpoints ────────────────────────────────────────────

@app.post("/api/v1/auth/register")
def register(user: UserRegister, db: Session = Depends(get_db)):
    """Register a new CardioSync user."""
    password_hash = pwd_context.hash(user.password)
    try:
        db.execute(text(
            "INSERT INTO users (username, email, password_hash, full_name) "
            "VALUES (:u, :e, :p, :n)"
        ), {"u": user.username, "e": user.email, "p": password_hash,
            "n": user.full_name})
        db.commit()
    except sqlalchemy.exc.IntegrityError:
        raise HTTPException(status_code=400, detail="Username or email already exists")
    return {"status": "registered", "username": user.username}

@app.post("/api/v1/auth/login")
def login(form_data: OAuth2PasswordRequestForm = Depends(), db: Session = Depends(get_db)):
    """Login and get JWT access token."""
    result = db.execute(text(
        "SELECT id, password_hash FROM users WHERE username = :u"
    ), {"u": form_data.username}).fetchone()

    if not result or not pwd_context.verify(form_data.password, result[1]):
        raise HTTPException(status_code=401, detail="Invalid credentials")

    access_token = create_access_token({"sub": str(result[0])})
    return {"access_token": access_token, "token_type": "bearer"}

@app.get("/api/v1/ecg/events")
def get_ecg_events(limit: int = 50, offset: int = 0,
                   user_id: int = Depends(get_current_user),
                   db: Session = Depends(get_db)):
    """List arrhythmia events (paginated)."""
    results = db.execute(text(
        "SELECT id, event_type, confidence, heart_rate_bpm, timestamp "
        "FROM ecg_events WHERE user_id = :uid "
        "ORDER BY timestamp DESC LIMIT :lim OFFSET :off"
    ), {"uid": user_id, "lim": limit, "off": offset}).fetchall()

    return [{"id": r[0], "event_type": r[1], "confidence": float(r[2]),
             "heart_rate": r[3], "timestamp": r[4].isoformat()} for r in results]

@app.get("/api/v1/ecg/events/{event_id}")
def get_ecg_event_detail(event_id: int,
                          user_id: int = Depends(get_current_user),
                          db: Session = Depends(get_db)):
    """Get a specific ECG event with ECG strip."""
    result = db.execute(text(
        "SELECT id, event_type, confidence, heart_rate_bpm, ecg_strip, timestamp "
        "FROM ecg_events WHERE id = :eid AND user_id = :uid"
    ), {"eid": event_id, "uid": user_id}).fetchone()

    if not result:
        raise HTTPException(status_code=404, detail="Event not found")

    return {"id": result[0], "event_type": result[1],
            "confidence": float(result[2]), "heart_rate": result[3],
            "ecg_strip": json.loads(result[4]) if result[4] else [],
            "timestamp": result[5].isoformat()}

@app.get("/api/v1/bp/records")
def get_bp_records(limit: int = 50, offset: int = 0,
                   user_id: int = Depends(get_current_user),
                   db: Session = Depends(get_db)):
    """List blood pressure records."""
    results = db.execute(text(
        "SELECT id, systolic, diastolic, map, heart_rate, quality, timestamp "
        "FROM bp_records WHERE user_id = :uid "
        "ORDER BY timestamp DESC LIMIT :lim OFFSET :off"
    ), {"uid": user_id, "lim": limit, "off": offset}).fetchall()

    return [{"id": r[0], "systolic": r[1], "diastolic": r[2], "map": r[3],
             "heart_rate": r[4], "quality": r[5],
             "timestamp": r[6].isoformat()} for r in results]

@app.get("/api/v1/bp/trends")
def get_bp_trends(days: int = 7,
                   user_id: int = Depends(get_current_user),
                   db: Session = Depends(get_db)):
    """BP trend analysis (7/30/90-day)."""
    since = datetime.now(timezone.utc) - timedelta(days=days)
    results = db.execute(text(
        "SELECT systolic, diastolic, map, heart_rate, timestamp "
        "FROM bp_records WHERE user_id = :uid AND timestamp >= :since "
        "ORDER BY timestamp ASC"
    ), {"uid": user_id, "since": since}).fetchall()

    if not results:
        return {"period_days": days, "count": 0, "trend": []}

    systolics = [r[0] for r in results]
    diastolics = [r[1] for r in results]

    # Calculate trend (linear regression slope)
    x = np.arange(len(systolics))
    sys_slope = np.polyfit(x, systolics, 1)[0] if len(systolics) > 1 else 0
    dia_slope = np.polyfit(x, diastolics, 1)[0] if len(diastolics) > 1 else 0

    # Classify latest BP
    latest_sys = systolics[-1]
    latest_dia = diastolics[-1]
    if latest_sys >= 180 or latest_dia >= 110:
        category = "Hypertension Stage 3 (Severe)"
    elif latest_sys >= 160 or latest_dia >= 100:
        category = "Hypertension Stage 2"
    elif latest_sys >= 140 or latest_dia >= 90:
        category = "Hypertension Stage 1"
    elif latest_sys >= 130 or latest_dia >= 85:
        category = "High Normal"
    elif latest_sys >= 120 or latest_dia >= 80:
        category = "Normal"
    else:
        category = "Optimal"

    return {
        "period_days": days,
        "count": len(results),
        "avg_systolic": float(np.mean(systolics)),
        "avg_diastolic": float(np.mean(diastolics)),
        "avg_map": float(np.mean([r[2] for r in results])),
        "sys_trend_slope": float(sys_slope),  # mmHg/day
        "dia_trend_slope": float(dia_slope),
        "latest_category": category,
        "latest_systolic": latest_sys,
        "latest_diastolic": latest_dia,
        "trend": [{"systolic": r[0], "diastolic": r[1], "map": r[2],
                    "heart_rate": r[3], "timestamp": r[4].isoformat()}
                   for r in results]
    }

@app.get("/api/v1/hrv/trends")
def get_hrv_trends(days: int = 7,
                    user_id: int = Depends(get_current_user),
                    db: Session = Depends(get_db)):
    """HRV trends (RMSSD, SDNN)."""
    since = datetime.now(timezone.utc) - timedelta(days=days)
    results = db.execute(text(
        "SELECT rmssd_ms, sdnn_ms, source, timestamp "
        "FROM hrv_records WHERE user_id = :uid AND timestamp >= :since "
        "ORDER BY timestamp ASC"
    ), {"uid": user_id, "since": since}).fetchall()

    if not results:
        return {"period_days": days, "count": 0}

    rmssds = [r[0] for r in results if r[0]]
    sdnns = [r[1] for r in results if r[1]]

    return {
        "period_days": days,
        "count": len(results),
        "avg_rmssd": float(np.mean(rmssds)) if rmssds else 0,
        "avg_sdnn": float(np.mean(sdnns)) if sdnns else 0,
        "hrv_trend": "improving" if len(rmssds) > 1 and rmssds[-1] > rmssds[0] else "stable",
        "records": [{"rmssd": r[0], "sdnn": r[1], "source": r[2],
                      "timestamp": r[3].isoformat()} for r in results]
    }

@app.get("/api/v1/risk/stroke")
def get_stroke_risk(user_id: int = Depends(get_current_user),
                     db: Session = Depends(get_db)):
    """30-day stroke risk forecast.

    Combines:
    - AFib burden (% time in AFib last 24h)
    - BP trend (latest category + slope)
    - HRV trend (RMSSD)
    - CHA₂DS₂-VASc clinical score
    """
    # Get AFib burden (events in last 24h)
    since_24h = datetime.now(timezone.utc) - timedelta(hours=24)
    afib_events = db.execute(text(
        "SELECT COUNT(*) FROM ecg_events "
        "WHERE user_id = :uid AND event_type = 'AFib' AND timestamp >= :since"
    ), {"uid": user_id, "since": since_24h}).scalar() or 0

    # Get latest BP
    latest_bp = db.execute(text(
        "SELECT systolic, diastolic FROM bp_records "
        "WHERE user_id = :uid ORDER BY timestamp DESC LIMIT 1"
    ), {"uid": user_id}).fetchone()

    # Get latest HRV
    latest_hrv = db.execute(text(
        "SELECT rmssd_ms FROM hrv_records "
        "WHERE user_id = :uid ORDER BY timestamp DESC LIMIT 1"
    ), {"uid": user_id}).fetchone()

    # Get CHA₂DS₂-VASc score
    chads = db.execute(text(
        "SELECT chads_vasc_score FROM users WHERE id = :uid"
    ), {"uid": user_id}).scalar() or 0

    # Calculate risk (simplified — production uses XGBoost model)
    risk = 0.0
    risk += min(afib_events * 2.0, 30.0)  # AFib burden contribution
    if latest_bp:
        if latest_bp[0] >= 180 or latest_bp[1] >= 110:
            risk += 25.0
        elif latest_bp[0] >= 160 or latest_bp[1] >= 100:
            risk += 15.0
        elif latest_bp[0] >= 140 or latest_bp[1] >= 90:
            risk += 8.0
    risk += chads * 5.0  # CHA₂DS₂-VASc contribution
    if latest_hrv and latest_hrv[0] and latest_hrv[0] < 20:
        risk += 5.0  # Low HRV = increased risk

    risk = min(risk, 100.0)

    # Store assessment
    db.execute(text(
        "INSERT INTO risk_assessments (user_id, stroke_risk_30d, afib_burden_pct, "
        "timestamp) VALUES (:uid, :risk, :burden, :ts)"
    ), {"uid": user_id, "risk": risk, "burden": afib_events * 2.0,
        "ts": datetime.now(timezone.utc)})
    db.commit()

    return StrokeRiskResponse(
        stroke_risk_30d=risk,
        afib_burden_pct=min(afib_events * 2.0, 100.0),
        bp_category=f"{latest_bp[0]}/{latest_bp[1]}" if latest_bp else "N/A",
        hrv_trend="low" if latest_hrv and latest_hrv[0] and latest_hrv[0] < 20 else "normal",
        risk_factors={
            "afib_events_24h": afib_events,
            "latest_bp": f"{latest_bp[0]}/{latest_bp[1]}" if latest_bp else None,
            "latest_rmssd": latest_hrv[0] if latest_hrv else None,
            "chads_vasc_score": chads,
        }
    )

@app.get("/api/v1/reports/monthly")
def get_monthly_report(user_id: int = Depends(get_current_user),
                        db: Session = Depends(get_db)):
    """Generate cardiologist-ready PDF report.

    Includes:
    - ECG strips of all arrhythmia events
    - AFib burden (% time in AFib)
    - BP trends (AM/PM, 30-day)
    - HRV trends (RMSSD, SDNN)
    - Stroke risk assessment
    """
    # In production, generates PDF with reportlab
    report_id = f"report_{user_id}_{datetime.now().strftime('%Y%m')}"

    # Gather data
    since = datetime.now(timezone.utc) - timedelta(days=30)
    events = db.execute(text(
        "SELECT event_type, confidence, heart_rate_bpm, timestamp "
        "FROM ecg_events WHERE user_id = :uid AND timestamp >= :since "
        "ORDER BY timestamp"
    ), {"uid": user_id, "since": since}).fetchall()

    bp_records = db.execute(text(
        "SELECT systolic, diastolic, timestamp "
        "FROM bp_records WHERE user_id = :uid AND timestamp >= :since "
        "ORDER BY timestamp"
    ), {"uid": user_id, "since": since}).fetchall()

    afib_count = sum(1 for e in events if e[0] == "AFib")

    return {
        "report_id": report_id,
        "period": f"{since.strftime('%Y-%m-%d')} to {datetime.now().strftime('%Y-%m-%d')}",
        "summary": {
            "total_ecg_events": len(events),
            "afib_events": afib_count,
            "afib_burden_pct": min(afib_count * 2.0, 100.0),
            "bp_readings": len(bp_records),
            "avg_systolic": float(np.mean([r[0] for r in bp_records])) if bp_records else 0,
            "avg_diastolic": float(np.mean([r[1] for r in bp_records])) if bp_records else 0,
        },
        "events": [{"type": e[0], "confidence": float(e[1]),
                     "hr": e[2], "timestamp": e[3].isoformat()} for e in events],
        "download_url": f"/api/v1/reports/{report_id}/pdf",
    }

@app.get("/api/v1/alerts/contacts")
def get_emergency_contacts(user_id: int = Depends(get_current_user),
                            db: Session = Depends(get_db)):
    """Get emergency contacts."""
    result = db.execute(text(
        "SELECT emergency_contact_1, emergency_contact_2 FROM users WHERE id = :uid"
    ), {"uid": user_id}).fetchone()
    return {"contact_1": result[0] if result else None,
            "contact_2": result[1] if result else None}

@app.post("/api/v1/alerts/contacts")
def set_emergency_contacts(contacts: dict,
                            user_id: int = Depends(get_current_user),
                            db: Session = Depends(get_db)):
    """Set emergency contacts."""
    db.execute(text(
        "UPDATE users SET emergency_contact_1 = :c1, emergency_contact_2 = :c2 "
        "WHERE id = :uid"
    ), {"c1": contacts.get("contact_1"), "c2": contacts.get("contact_2"),
        "uid": user_id})
    db.commit()
    return {"status": "updated"}

@app.post("/api/v1/config/bp-schedule")
def set_bp_schedule(schedule: dict,
                     user_id: int = Depends(get_current_user),
                     db: Session = Depends(get_db)):
    """Set BP measurement schedule (sent to Hub via MQTT)."""
    # Publish to Hub via MQTT
    mqtt_client.publish(f"cardiosync/{user_id}/cloud/commands",
                        json.dumps({"type": "bp_schedule", "schedule": schedule}))
    return {"status": "schedule_updated"}

@app.get("/api/v1/health")
def health_check():
    """Health check endpoint."""
    return {"status": "healthy", "mqtt_connected": mqtt_connected}

@app.get("/api/v1/dashboard")
def get_dashboard(user_id: int = Depends(get_current_user),
                   db: Session = Depends(get_db)):
    """Get dashboard summary (all key metrics)."""
    since_24h = datetime.now(timezone.utc) - timedelta(hours=24)

    # Latest HR
    latest_hr = db.execute(text(
        "SELECT heart_rate FROM ppg_hr WHERE user_id = :uid "
        "ORDER BY timestamp DESC LIMIT 1"
    ), {"uid": user_id}).scalar() or 0

    # Latest SpO2
    latest_spo2 = db.execute(text(
        "SELECT spo2 FROM ppg_hr WHERE user_id = :uid "
        "ORDER BY timestamp DESC LIMIT 1"
    ), {"uid": user_id}).scalar() or 0

    # Latest BP
    latest_bp = db.execute(text(
        "SELECT systolic, diastolic FROM bp_records "
        "WHERE user_id = :uid ORDER BY timestamp DESC LIMIT 1"
    ), {"uid": user_id}).fetchone()

    # AFib events in last 24h
    afib_24h = db.execute(text(
        "SELECT COUNT(*) FROM ecg_events WHERE user_id = :uid "
        "AND event_type = 'AFib' AND timestamp >= :since"
    ), {"uid": user_id, "since": since_24h}).scalar() or 0

    # Latest HRV
    latest_hrv = db.execute(text(
        "SELECT rmssd_ms, sdnn_ms FROM hrv_records "
        "WHERE user_id = :uid ORDER BY timestamp DESC LIMIT 1"
    ), {"uid": user_id}).fetchone()

    return {
        "heart_rate": latest_hr,
        "spo2": latest_spo2,
        "blood_pressure": {"systolic": latest_bp[0], "diastolic": latest_bp[1]} if latest_bp else None,
        "afib_events_24h": afib_24h,
        "hrv": {"rmssd": latest_hrv[0], "sdnn": latest_hrv[1]} if latest_hrv else None,
        "timestamp": datetime.now(timezone.utc).isoformat(),
    }

# ── WebSocket for real-time ECG ──────────────────────────────
@app.websocket("/api/v1/ecg/stream")
async def ecg_stream_ws(websocket: WebSocket, user_id: int = 1):
    """WebSocket for real-time ECG streaming to mobile app."""
    await websocket.accept()
    pubsub = redis_client.pubsub()
    pubsub.subscribe(f"cardiosync:{user_id}:ecg")

    try:
        while True:
            message = pubsub.get_message()
            if message and message["type"] == "message":
                await websocket.send_text(message["data"])
            await asyncio.sleep(0.01)
    except WebSocketDisconnect:
        pass
    finally:
        pubsub.unsubscribe()

# ── Main ─────────────────────────────────────────────────────
if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)