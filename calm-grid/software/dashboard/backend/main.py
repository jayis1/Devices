"""
CalmGrid Cloud Backend — FastAPI + MQTT

Ingests vitals, prosody, environment, intervention, and alert events
from the hub via MQTT, stores them in PostgreSQL/TimescaleDB, serves
the user + therapist APIs, runs the alert engine, and exposes a
WebSocket for real-time mobile alerts.
"""

import os
import json
import asyncio
from datetime import datetime, timezone
from typing import Optional, List
from contextlib import asynccontextmanager

import paho.mqtt.client as mqtt
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from sqlalchemy import create_engine, Column, Integer, Float, String, DateTime, Boolean, Text, BigInteger
from sqlalchemy.orm import sessionmaker, declarative_base, Session

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://calm:calm@db:5432/calmgrid")
MQTT_HOST    = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT    = int(os.getenv("MQTT_PORT", "1883"))
ALERT_HRV_DECLINE_THRESHOLD = 0.20
ALERT_HR_ELEVATION_THRESHOLD = 0.10
ALERT_EDA_AROUSAL_THRESHOLD = 2.0
ALERT_STRESS_THRESHOLD = 70
ALERT_BURNOUT_THRESHOLD = 60

engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)
Base = declarative_base()


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------
class User(Base):
    __tablename__ = "users"
    id            = Column(Integer, primary_key=True)
    name          = Column(String(120))
    age           = Column(Integer)
    sex           = Column(String(20))
    work_pattern  = Column(String(40))
    sleep_target_h = Column(Integer, default=8)
    therapist_id  = Column(Integer)
    created_at    = Column(DateTime, default=datetime.utcnow)


class VitalEvent(Base):
    """Time-series: one row per 60s reporting window from wrist band."""
    __tablename__ = "vital_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    hr_bpm        = Column(Integer)
    hrv_rmssd_ms  = Column(Float)
    eda_scl_us    = Column(Float)           # microsiemens
    eda_scr_rate  = Column(Float)           # events per minute
    skin_temp_c   = Column(Float)
    activity_class = Column(Integer)        # 0-7
    step_count    = Column(Integer)
    battery_pct   = Column(Integer)
    flags         = Column(Integer)


class ProsodyEvent(Base):
    """Time-series: prosody stress from room sentinel."""
    __tablename__ = "prosody_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    prosody_class = Column(Integer)         # 0=calm 1=neutral 2=elevated 3=high
    confidence    = Column(Integer)
    speech_minutes = Column(Integer)
    f0_deviation  = Column(Integer)         # cents * 10


class EnvironmentEvent(Base):
    """Time-series: ambient environment from room sentinel."""
    __tablename__ = "environment_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    ambient_lux   = Column(Float)
    cct_kelvin    = Column(Integer)
    temp_c        = Column(Float)
    humidity_pct  = Column(Float)
    noise_db      = Column(Float)
    env_flags     = Column(Integer)


class InterventionEvent(Base):
    """Intervention triggered + outcome."""
    __tablename__ = "intervention_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    intervention_type = Column(Integer)     # 0=breathing 1=soundscape 2=lighting 3=combined
    param1        = Column(Integer)          # breathing pattern or soundscape id
    param2        = Column(Integer)
    duration_s    = Column(Integer)
    pre_hrv_ms    = Column(Float)
    post_hrv_ms   = Column(Float)
    pre_eda_scr   = Column(Float)
    post_eda_scr  = Column(Float)
    hrv_delta     = Column(Float)
    eda_delta     = Column(Float)
    efficacy      = Column(Integer)         # 0-100


class StressScore(Base):
    """Daily stress scores computed by the hub."""
    __tablename__ = "stress_scores"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    stress        = Column(Integer)        # 0-100
    burnout_risk  = Column(Integer)        # 0-100 (14-day forecast)
    recovery      = Column(Integer)        # 0-100


class AlertRecord(Base):
    __tablename__ = "alerts"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    user_id       = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    alert_type    = Column(String(40))
    severity      = Column(String(20))     # low, medium, high
    message       = Column(Text)
    acknowledged  = Column(Boolean, default=False)


# ---------------------------------------------------------------------------
# MQTT Client
# ---------------------------------------------------------------------------
mqtt_client = mqtt.Client()
mqtt_connected = False
ws_clients: dict[int, list[WebSocket]] = {}


def on_mqtt_connect(client, userdata, flags, rc):
    global mqtt_connected
    mqtt_connected = (rc == 0)
    if mqtt_connected:
        client.subscribe("calmgrid/+/vitals")
        client.subscribe("calmgrid/+/prosody")
        client.subscribe("calmgrid/+/environment")
        client.subscribe("calmgrid/+/intervention")
        client.subscribe("calmgrid/+/stress")


def on_mqtt_message(client, userdata, msg):
    """Process incoming MQTT messages from hubs."""
    try:
        data = json.loads(msg.payload)
        db = SessionLocal()
        topic_parts = msg.topic.split("/")
        user_id = int(topic_parts[1])
        msg_type = topic_parts[2]

        if msg_type == "vitals":
            event = VitalEvent(
                user_id=user_id, hr_bpm=data.get("hr"),
                hrv_rmssd_ms=data.get("hrv_ms"),
                eda_scl_us=data.get("eda_scl"),
                eda_scr_rate=data.get("eda_scr"),
                skin_temp_c=data.get("temp_c"),
                activity_class=data.get("activity"),
                step_count=data.get("steps"),
                battery_pct=data.get("battery"),
                flags=data.get("flags", 0),
            )
            db.add(event)
            check_vital_alerts(db, user_id, data)

        elif msg_type == "prosody":
            event = ProsodyEvent(
                user_id=user_id, prosody_class=data.get("class"),
                confidence=data.get("confidence"),
                speech_minutes=data.get("speech_min"),
                f0_deviation=data.get("f0_dev"),
            )
            db.add(event)

        elif msg_type == "environment":
            event = EnvironmentEvent(
                user_id=user_id, ambient_lux=data.get("lux"),
                cct_kelvin=data.get("cct"),
                temp_c=data.get("temp_c"),
                humidity_pct=data.get("humidity"),
                noise_db=data.get("noise_db"),
                env_flags=data.get("flags", 0),
            )
            db.add(event)

        elif msg_type == "stress":
            score = StressScore(
                user_id=user_id, stress=data.get("stress"),
                burnout_risk=data.get("burnout"),
                recovery=data.get("recovery"),
            )
            db.add(score)
            check_stress_alerts(db, user_id, data)

        elif msg_type == "intervention":
            event = InterventionEvent(
                user_id=user_id, intervention_type=data.get("type"),
                param1=data.get("p1", 0), param2=data.get("p2", 0),
                duration_s=data.get("duration", 0),
                pre_hrv_ms=data.get("pre_hrv"),
                post_hrv_ms=data.get("post_hrv"),
                hrv_delta=data.get("hrv_delta"),
                eda_delta=data.get("eda_delta"),
                efficacy=data.get("efficacy", 0),
            )
            db.add(event)

        db.commit()
        db.close()
    except Exception as e:
        print(f"MQTT message error: {e}")


def check_vital_alerts(db, user_id, data):
    """Check vitals for alert conditions."""
    alerts = []
    flags = data.get("flags", 0)
    if flags & 0x01:  # HRV decline
        alerts.append(("hrv_decline", "medium", "HRV significantly below baseline"))
    if flags & 0x02:  # HR elevated
        alerts.append(("hr_elevated", "low", "Resting heart rate elevated"))
    if flags & 0x04:  # EDA arousal
        alerts.append(("eda_arousal", "medium", "Skin conductance arousal detected"))
    if flags & 0x10:  # Acute stress
        alerts.append(("acute_stress", "high", "Acute stress episode detected"))
    if flags & 0x40:  # Environmental stress
        alerts.append(("env_stress", "low", "Environmental stressor detected"))

    for atype, sev, msg in alerts:
        alert = AlertRecord(user_id=user_id, alert_type=atype, severity=sev, message=msg)
        db.add(alert)
        asyncio.create_task(push_ws_alert(user_id, {"type": atype, "severity": sev, "message": msg}))


def check_stress_alerts(db, user_id, data):
    """Check stress scores for alert conditions."""
    stress = data.get("stress", 0)
    burnout = data.get("burnout", 0)
    if stress >= ALERT_STRESS_THRESHOLD:
        alert = AlertRecord(user_id=user_id, alert_type="high_stress",
                            severity="high", message=f"High stress detected: {stress}/100")
        db.add(alert)
        asyncio.create_task(push_ws_alert(user_id, {"type": "high_stress", "message": "High stress - intervene now"}))
    if burnout >= ALERT_BURNOUT_THRESHOLD:
        alert = AlertRecord(user_id=user_id, alert_type="burnout_risk",
                            severity="high", message=f"Burnout risk elevated: {burnout}/100")
        db.add(alert)
        asyncio.create_task(push_ws_alert(user_id, {"type": "burnout", "message": "Burnout risk rising"}))


async def push_ws_alert(user_id: int, alert: dict):
    """Push alert to connected WebSocket clients."""
    for ws in ws_clients.get(user_id, []):
        try:
            await ws.send_json(alert)
        except Exception:
            pass


mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message


@asynccontextmanager
async def lifespan(app: FastAPI):
    Base.metadata.create_all(engine)
    mqtt_client.connect(MQTT_HOST, MQTT_PORT, 60)
    mqtt_client.loop_start()
    yield
    mqtt_client.loop_stop()
    mqtt_client.disconnect()


# ---------------------------------------------------------------------------
# FastAPI App
# ---------------------------------------------------------------------------
app = FastAPI(title="CalmGrid API", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


@app.get("/api/v1/health")
def health():
    return {"status": "ok", "mqtt": mqtt_connected}


@app.post("/api/v1/ingest/vitals")
def ingest_vitals(user_id: int, hr: int, hrv_ms: float, eda_scl: float,
                  eda_scr: float, temp_c: float, activity: int,
                  steps: int, battery: int, flags: int = 0,
                  db: Session = Depends(get_db)):
    event = VitalEvent(user_id=user_id, hr_bpm=hr, hrv_rmssd_ms=hrv_ms,
                       eda_scl_us=eda_scl, eda_scr_rate=eda_scr,
                       skin_temp_c=temp_c, activity_class=activity,
                       step_count=steps, battery_pct=battery, flags=flags)
    db.add(event)
    db.commit()
    return {"status": "stored"}


@app.post("/api/v1/ingest/prosody")
def ingest_prosody(user_id: int, prosody_class: int, confidence: int,
                   speech_min: int, f0_dev: int, db: Session = Depends(get_db)):
    event = ProsodyEvent(user_id=user_id, prosody_class=prosody_class,
                         confidence=confidence, speech_minutes=speech_min,
                         f0_deviation=f0_dev)
    db.add(event)
    db.commit()
    return {"status": "stored"}


@app.get("/api/v1/user/{uid}/stress")
def get_stress(uid: int, days: int = 14, db: Session = Depends(get_db)):
    scores = db.query(StressScore).filter(
        StressScore.user_id == uid
    ).order_by(StressScore.ts.desc()).limit(days * 96).all()
    return {
        "current": {"stress": scores[0].stress, "burnout_risk": scores[0].burnout_risk,
                     "recovery": scores[0].recovery} if scores else None,
        "trend": [{"ts": s.ts.isoformat(), "stress": s.stress,
                    "burnout": s.burnout_risk, "recovery": s.recovery}
                   for s in reversed(scores)],
    }


@app.get("/api/v1/user/{uid}/vitals")
def get_vitals(uid: int, hours: int = 24, db: Session = Depends(get_db)):
    events = db.query(VitalEvent).filter(
        VitalEvent.user_id == uid
    ).order_by(VitalEvent.ts.desc()).limit(hours * 60).all()
    return [{"ts": e.ts.isoformat(), "hr": e.hr_bpm, "hrv_ms": e.hrv_rmssd_ms,
             "eda_scl": e.eda_scl_us, "eda_scr": e.eda_scr_rate,
             "temp_c": e.skin_temp_c, "activity": e.activity_class,
             "steps": e.step_count}
            for e in reversed(events)]


@app.get("/api/v1/user/{uid}/episodes")
def get_episodes(uid: int, days: int = 7, db: Session = Depends(get_db)):
    """Get acute stress episodes (flagged vitals)."""
    events = db.query(VitalEvent).filter(
        VitalEvent.user_id == uid,
        VitalEvent.flags.op('&')(0x10) != 0  # acute stress flag
    ).order_by(VitalEvent.ts.desc()).limit(days * 100).all()
    return [{"ts": e.ts.isoformat(), "hr": e.hr_bpm, "hrv_ms": e.hrv_rmssd_ms,
             "eda_scr": e.eda_scr_rate, "activity": e.activity_class}
            for e in reversed(events)]


@app.get("/api/v1/user/{uid}/burnout")
def get_burnout(uid: int, db: Session = Depends(get_db)):
    scores = db.query(StressScore).filter(
        StressScore.user_id == uid
    ).order_by(StressScore.ts.desc()).limit(30 * 96).all()
    if not scores:
        return {"risk": 0, "trend": []}
    avg_stress = sum(s.stress for s in scores) / len(scores)
    latest = scores[0]
    return {
        "risk": latest.burnout_risk,
        "current_stress": latest.stress,
        "avg_stress_30d": avg_stress,
        "trend": [{"ts": s.ts.isoformat(), "burnout": s.burnout_risk}
                   for s in reversed(scores[-30:])],
    }


@app.get("/api/v1/user/{uid}/interventions")
def get_interventions(uid: int, days: int = 7, db: Session = Depends(get_db)):
    events = db.query(InterventionEvent).filter(
        InterventionEvent.user_id == uid
    ).order_by(InterventionEvent.ts.desc()).limit(days * 20).all()
    return [{"ts": e.ts.isoformat(), "type": e.intervention_type,
             "duration_s": e.duration_s, "efficacy": e.efficacy,
             "hrv_delta": e.hrv_delta}
            for e in reversed(events)]


@app.get("/api/v1/user/{uid}/alerts")
def get_alerts(uid: int, days: int = 7, db: Session = Depends(get_db)):
    alerts = db.query(AlertRecord).filter(
        AlertRecord.user_id == uid
    ).order_by(AlertRecord.ts.desc()).limit(days * 50).all()
    return [{"id": a.id, "ts": a.ts.isoformat(), "type": a.alert_type,
             "severity": a.severity, "message": a.message,
             "acknowledged": a.acknowledged}
            for a in reversed(alerts)]


@app.post("/api/v1/therapist/report/{uid}")
def therapist_report(uid: int, db: Session = Depends(get_db)):
    """Generate a structured therapist report."""
    recent_vitals = db.query(VitalEvent).filter(
        VitalEvent.user_id == uid
    ).order_by(VitalEvent.ts.desc()).limit(288).all()  # 24h

    recent_scores = db.query(StressScore).filter(
        StressScore.user_id == uid
    ).order_by(StressScore.ts.desc()).limit(30).all()

    interventions = db.query(InterventionEvent).filter(
        InterventionEvent.user_id == uid
    ).order_by(InterventionEvent.ts.desc()).limit(50).all()

    alerts = db.query(AlertRecord).filter(
        AlertRecord.user_id == uid
    ).order_by(AlertRecord.ts.desc()).limit(20).all()

    avg_hrv = sum(v.hrv_rmssd_ms for v in recent_vitals) / max(len(recent_vitals), 1)
    avg_stress = sum(s.stress for s in recent_scores) / max(len(recent_scores), 1)

    return {
        "user_id": uid,
        "generated_at": datetime.utcnow().isoformat(),
        "summary": {
            "avg_hrv_24h": avg_hrv,
            "avg_stress_30d": avg_stress,
            "burnout_risk": recent_scores[0].burnout_risk if recent_scores else 0,
            "acute_stress_episodes": sum(1 for v in recent_vitals if v.flags & 0x10),
        },
        "vitals_trend": [{"ts": v.ts.isoformat(), "hrv_ms": v.hrv_rmssd_ms,
                          "eda_scr": v.eda_scr_rate}
                         for v in reversed(recent_vitals[-48:])],
        "stress_trend": [{"ts": s.ts.isoformat(), "stress": s.stress,
                          "burnout": s.burnout_risk}
                         for s in reversed(recent_scores)],
        "interventions": [{"ts": i.ts.isoformat(), "type": i.intervention_type,
                           "efficacy": i.efficacy}
                          for i in reversed(interventions)],
        "recent_alerts": [{"ts": a.ts.isoformat(), "type": a.alert_type,
                           "severity": a.severity, "message": a.message}
                          for a in reversed(alerts)],
    }


@app.websocket("/api/v1/ws/alerts/{uid}")
async def ws_alerts(ws: WebSocket, uid: int):
    await ws.accept()
    ws_clients.setdefault(uid, []).append(ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        ws_clients[uid].remove(ws)