"""
PawSync Cloud Backend — FastAPI + MQTT

Ingests vitals, behavior, feeding, and alert events from the hub via MQTT,
stores them in PostgreSQL/TimescaleDB, serves the owner + vet APIs,
runs the alert engine, and exposes a WebSocket for real-time mobile alerts.
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
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://paw:paw@db:5432/pawsync")
MQTT_HOST    = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT    = int(os.getenv("MQTT_PORT", "1883"))
ALERT_HRV_DECLINE_THRESHOLD = 0.20  # 20% below baseline
ALERT_HR_ELEVATION_THRESHOLD = 0.15  # 15% above baseline
ALERT_APPETITE_LOSS_THRESHOLD = 0.25  # 25% uneaten
ALERT_WELLNESS_THRESHOLD = 50     # 0-100

engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False)
Base = declarative_base()


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------
class Pet(Base):
    __tablename__ = "pets"
    id            = Column(Integer, primary_key=True)
    name          = Column(String(120))
    species       = Column(String(20))     # dog / cat
    breed         = Column(String(120))
    birth_date    = Column(DateTime)
    weight_g      = Column(Integer)
    target_weight_g = Column(Integer)
    rfid_uid      = Column(String(40))     # hex UID
    owner_id      = Column(Integer)
    vet_id        = Column(Integer)
    created_at    = Column(DateTime, default=datetime.utcnow)


class VitalEvent(Base):
    """Time-series: one row per 60s reporting window from collar."""
    __tablename__ = "vital_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    pet_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    hr_bpm        = Column(Integer)
    hrv_rmssd_ms  = Column(Float)          # ms (centi-ms / 100)
    skin_temp_c   = Column(Float)          # degC (centi-degC / 100)
    activity_class = Column(Integer)       # 0-8
    gait_symmetry  = Column(Integer)       # 0-1000
    gait_stride_cm = Column(Integer)
    battery_pct   = Column(Integer)
    flags         = Column(Integer)


class BehaviorEvent(Base):
    """Time-series: behavior classification from camera."""
    __tablename__ = "behavior_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    pet_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    behavior_class = Column(Integer)       # 0-5
    vocalization   = Column(Integer)       # 0-6
    confidence     = Column(Integer)
    duration_s     = Column(Integer)
    is_anxiety_episode = Column(Boolean, default=False)
    clip_ref       = Column(String(256))   # object-store ref (if shared)


class FeedingEvent(Base):
    __tablename__ = "feeding_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    pet_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    dispensed_g   = Column(Integer)
    consumed_g    = Column(Integer)
    water_ml      = Column(Integer)
    hopper_pct    = Column(Integer)
    appetite_loss = Column(Boolean, default=False)


class WellnessScore(Base):
    """Daily wellness scores computed by the hub."""
    __tablename__ = "wellness_scores"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    pet_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    wellness      = Column(Integer)        # 0-100
    illness_risk  = Column(Integer)        # 0-100 (7-day forecast)
    anxiety_level = Column(Integer)        # 0-100


class AlertRecord(Base):
    __tablename__ = "alerts"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    pet_id        = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    alert_type    = Column(String(40))     # hrv_decline, hr_elevated, fever, lameness, scratching, appetite_loss, anxiety, high_risk
    severity      = Column(String(20))     # low, medium, high
    message       = Column(Text)
    acknowledged  = Column(Boolean, default=False)


class Baseline(Base):
    """Per-pet baseline learned over first 14 days."""
    __tablename__ = "baselines"
    pet_id        = Column(Integer, primary_key=True)
    baseline_hr   = Column(Float)
    baseline_hrv   = Column(Float)         # ms
    baseline_temp  = Column(Float)          # degC
    sample_count   = Column(Integer, default=0)
    established     = Column(Boolean, default=False)
    updated_at     = Column(DateTime, default=datetime.utcnow)


Base.metadata.create_all(bind=engine)


# ---------------------------------------------------------------------------
# Pydantic schemas
# ---------------------------------------------------------------------------
class VitalsIn(BaseModel):
    pet_id: int
    hr_bpm: int = 0
    hrv_rmssd_ms: float = 0.0
    skin_temp_c: float = 0.0
    activity_class: int = 0
    gait_symmetry: int = 0
    gait_stride_cm: int = 0
    battery_pct: int = 100
    flags: int = 0


class BehaviorIn(BaseModel):
    pet_id: int
    behavior_class: int
    vocalization: int = 0
    confidence: int = 0
    duration_s: int = 0
    is_anxiety_episode: bool = False
    clip_ref: str = ""


class FeedingIn(BaseModel):
    pet_id: int
    dispensed_g: int
    consumed_g: int
    water_ml: int = 0
    hopper_pct: int = 100
    appetite_loss: bool = False


class WellnessOut(BaseModel):
    wellness: int
    illness_risk: int
    anxiety_level: int
    trend: List[dict]


# ---------------------------------------------------------------------------
# WebSocket connection manager
# ---------------------------------------------------------------------------
class AlertManager:
    def __init__(self):
        self.connections: dict[int, list[WebSocket]] = {}

    async def connect(self, pet_id: int, ws: WebSocket):
        await ws.accept()
        self.connections.setdefault(pet_id, []).append(ws)

    def disconnect(self, pet_id: int, ws: WebSocket):
        if pet_id in self.connections:
            self.connections[pet_id].remove(ws)

    async def broadcast(self, pet_id: int, message: dict):
        for ws in self.connections.get(pet_id, []):
            try:
                await ws.send_json(message)
            except Exception:
                pass

alert_mgr = AlertManager()


# ---------------------------------------------------------------------------
# Alert engine
# ---------------------------------------------------------------------------
def evaluate_alerts(db: Session, pet_id: int, payload: dict) -> Optional[dict]:
    """Rule-based alerts (independent of ML model — fail-safe)."""
    alerts = []

    # Check baseline for HRV decline
    baseline = db.query(Baseline).filter(Baseline.pet_id == pet_id).first()
    hrv = payload.get("hrv_rmssd_ms", 0)
    if baseline and baseline.established and baseline.baseline_hrv > 0:
        hrv_ratio = hrv / baseline.baseline_hrv
        if hrv_ratio < (1 - ALERT_HRV_DECLINE_THRESHOLD):
            decline_pct = (1 - hrv_ratio) * 100
            alerts.append({
                "type": "hrv_decline",
                "severity": "high",
                "message": f"HRV declined {decline_pct:.0f}% below baseline — possible pain or illness",
            })

    # HR elevation
    hr = payload.get("hr_bpm", 0)
    if baseline and baseline.established and baseline.baseline_hr > 0:
        hr_ratio = hr / baseline.baseline_hr
        if hr_ratio > (1 + ALERT_HR_ELEVATION_THRESHOLD):
            alerts.append({
                "type": "hr_elevated",
                "severity": "medium",
                "message": f"Resting HR elevated ({hr} vs baseline {baseline.baseline_hr:.0f} bpm)",
            })

    # Flags
    flags = payload.get("flags", 0)
    if flags & 0x08:  # PAW_ALERT_LAMENESS
        alerts.append({"type": "lameness", "severity": "high",
                       "message": "Gait asymmetry detected — possible lameness or pain"})
    if flags & 0x10:  # PAW_ALERT_SCRATCHING
        alerts.append({"type": "scratching", "severity": "medium",
                       "message": "Excessive scratching detected — check for skin issues or allergies"})
    if flags & 0x20:  # PAW_ALERT_APPETITE_LOSS
        alerts.append({"type": "appetite_loss", "severity": "high",
                       "message": "Appetite loss detected — pet is eating less than usual"})
    if flags & 0x40:  # PAW_ALERT_ANXIETY
        alerts.append({"type": "anxiety", "severity": "medium",
                       "message": "Separation anxiety episode detected"})

    # Wellness score
    wellness = payload.get("wellness", 100)
    if wellness < ALERT_WELLNESS_THRESHOLD:
        alerts.append({"type": "high_risk", "severity": "high",
                       "message": f"Wellness score low ({wellness}/100) — consider vet visit"})

    for a in alerts:
        rec = AlertRecord(pet_id=pet_id, alert_type=a["type"],
                          severity=a["severity"], message=a["message"])
        db.add(rec)
    db.commit()
    return alerts[0] if alerts else None


# ---------------------------------------------------------------------------
# Baseline update
# ---------------------------------------------------------------------------
def update_baseline(db: Session, pet_id: int, hr: int, hrv_ms: float, temp_c: float):
    baseline = db.query(Baseline).filter(Baseline.pet_id == pet_id).first()
    if not baseline:
        baseline = Baseline(pet_id=pet_id)
        db.add(baseline)
    if baseline.established:
        return
    baseline.sample_count += 1
    n = baseline.sample_count
    if n == 1:
        baseline.baseline_hr = hr
        baseline.baseline_hrv = hrv_ms
        baseline.baseline_temp = temp_c
    else:
        baseline.baseline_hr = (baseline.baseline_hr * (n-1) + hr) / n
        baseline.baseline_hrv = (baseline.baseline_hrv * (n-1) + hrv_ms) / n
        baseline.baseline_temp = (baseline.baseline_temp * (n-1) + temp_c) / n
    if n >= 4032:  # 14 days × 288 samples/day
        baseline.established = True
    db.commit()


# ---------------------------------------------------------------------------
# MQTT subscriber
# ---------------------------------------------------------------------------
def on_mqtt_connect(client, userdata, flags, rc):
    client.subscribe("pawsync/+/vitals")
    client.subscribe("pawsync/+/behavior")
    client.subscribe("pawsync/+/feeding")
    client.subscribe("pawsync/+/wellness")
    client.subscribe("pawsync/+/alert")


def on_mqtt_message(client, userdata, msg):
    topic_parts = msg.topic.split("/")
    pet_id = int(topic_parts[1])
    kind = topic_parts[2]
    data = json.loads(msg.payload)
    db = SessionLocal()
    try:
        if kind == "vitals":
            ev = VitalEvent(pet_id=pet_id, hr_bpm=data.get("hr", 0),
                            hrv_rmssd_ms=data.get("hrv", 0) / 100.0,
                            skin_temp_c=data.get("temp", 0) / 100.0,
                            activity_class=data.get("act", 0),
                            gait_symmetry=data.get("gait_sym", 0),
                            battery_pct=data.get("batt", 100),
                            flags=data.get("flags", 0))
            db.add(ev)
            db.commit()
            update_baseline(db, pet_id, data.get("hr", 0),
                            data.get("hrv", 0) / 100.0,
                            data.get("temp", 0) / 100.0)
            alert = evaluate_alerts(db, pet_id, {
                "hr_bpm": data.get("hr", 0),
                "hrv_rmssd_ms": data.get("hrv", 0) / 100.0,
                "flags": data.get("flags", 0),
            })
            if alert:
                asyncio.run(alert_mgr.broadcast(pet_id, alert))
        elif kind == "behavior":
            ev = BehaviorEvent(pet_id=pet_id,
                               behavior_class=data.get("behavior", 0),
                               vocalization=data.get("vocal", 0),
                               confidence=data.get("conf", 0),
                               duration_s=data.get("dur", 0),
                               is_anxiety_episode=data.get("anxiety", False),
                               clip_ref=data.get("clip", ""))
            db.add(ev)
            db.commit()
            if data.get("anxiety"):
                alert = evaluate_alerts(db, pet_id, {"flags": 0x40})
                if alert:
                    asyncio.run(alert_mgr.broadcast(pet_id, alert))
        elif kind == "feeding":
            appetite_loss = False
            disp = data.get("disp", 0)
            cons = data.get("cons", 0)
            if disp > 0 and (1 - cons / disp) > ALERT_APPETITE_LOSS_THRESHOLD:
                appetite_loss = True
            ev = FeedingEvent(pet_id=pet_id, dispensed_g=disp,
                              consumed_g=cons, water_ml=data.get("water", 0),
                              hopper_pct=data.get("hopper", 100),
                              appetite_loss=appetite_loss)
            db.add(ev)
            db.commit()
            if appetite_loss:
                alert = evaluate_alerts(db, pet_id, {"flags": 0x20})
                if alert:
                    asyncio.run(alert_mgr.broadcast(pet_id, alert))
        elif kind == "wellness":
            score = WellnessScore(pet_id=pet_id,
                                  wellness=data.get("wellness", 100),
                                  illness_risk=data.get("risk", 0),
                                  anxiety_level=data.get("anxiety", 0))
            db.add(score)
            db.commit()
            alert = evaluate_alerts(db, pet_id, {
                "wellness": data.get("wellness", 100),
                "flags": data.get("flags", 0),
            })
            if alert:
                asyncio.run(alert_mgr.broadcast(pet_id, alert))
    finally:
        db.close()


mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message


@asynccontextmanager
async def lifespan(app: FastAPI):
    mqtt_client.connect(MQTT_HOST, MQTT_PORT, 60)
    mqtt_client.loop_start()
    yield
    mqtt_client.loop_stop()


# ---------------------------------------------------------------------------
# FastAPI app
# ---------------------------------------------------------------------------
app = FastAPI(title="PawSync API", version="1.0.0", lifespan=lifespan)
app.add_middleware(CORSMiddleware, allow_origins=["*"],
                   allow_methods=["*"], allow_headers=["*"])


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


@app.get("/api/v1/health")
def health():
    return {"status": "ok"}


@app.post("/api/v1/ingest/vitals")
def ingest_vitals(payload: VitalsIn, db: Session = Depends(get_db)):
    ev = VitalEvent(pet_id=payload.pet_id, hr_bpm=payload.hr_bpm,
                    hrv_rmssd_ms=payload.hrv_rmssd_ms,
                    skin_temp_c=payload.skin_temp_c,
                    activity_class=payload.activity_class,
                    gait_symmetry=payload.gait_symmetry,
                    battery_pct=payload.battery_pct,
                    flags=payload.flags)
    db.add(ev)
    db.commit()
    update_baseline(db, payload.pet_id, payload.hr_bpm,
                    payload.hrv_rmssd_ms, payload.skin_temp_c)
    alerts = evaluate_alerts(db, payload.pet_id, {
        "hr_bpm": payload.hr_bpm,
        "hrv_rmssd_ms": payload.hrv_rmssd_ms,
        "flags": payload.flags,
    })
    return {"status": "stored", "alerts": alerts or []}


@app.post("/api/v1/ingest/behavior")
def ingest_behavior(payload: BehaviorIn, db: Session = Depends(get_db)):
    ev = BehaviorEvent(pet_id=payload.pet_id,
                       behavior_class=payload.behavior_class,
                       vocalization=payload.vocalization,
                       confidence=payload.confidence,
                       duration_s=payload.duration_s,
                       is_anxiety_episode=payload.is_anxiety_episode,
                       clip_ref=payload.clip_ref)
    db.add(ev)
    db.commit()
    if payload.is_anxiety_episode:
        alerts = evaluate_alerts(db, payload.pet_id, {"flags": 0x40})
        return {"status": "stored", "alerts": alerts or []}
    return {"status": "stored"}


@app.post("/api/v1/ingest/feeding")
def ingest_feeding(payload: FeedingIn, db: Session = Depends(get_db)):
    ev = FeedingEvent(pet_id=payload.pet_id, dispensed_g=payload.dispensed_g,
                      consumed_g=payload.consumed_g, water_ml=payload.water_ml,
                      hopper_pct=payload.hopper_pct,
                      appetite_loss=payload.appetite_loss)
    db.add(ev)
    db.commit()
    if payload.appetite_loss:
        alerts = evaluate_alerts(db, payload.pet_id, {"flags": 0x20})
        return {"status": "stored", "alerts": alerts or []}
    return {"status": "stored"}


@app.get("/api/v1/pet/{pid}/wellness")
def get_wellness(pid: int, db: Session = Depends(get_db)):
    """Current + 7-day wellness score trend."""
    rows = db.query(WellnessScore).filter(WellnessScore.pet_id == pid) \
        .order_by(WellnessScore.ts.desc()).limit(168).all()  # 7 days @ 15min
    trend = [{"ts": r.ts.isoformat(), "wellness": r.wellness,
              "illness_risk": r.illness_risk, "anxiety": r.anxiety_level}
             for r in reversed(rows)]
    latest = rows[0] if rows else None
    return WellnessOut(
        wellness=latest.wellness if latest else 100,
        illness_risk=latest.illness_risk if latest else 0,
        anxiety_level=latest.anxiety_level if latest else 0,
        trend=trend)


@app.get("/api/v1/pet/{pid}/activity")
def get_activity(pid: int, db: Session = Depends(get_db)):
    """Activity timeline (24h)."""
    rows = db.query(VitalEvent).filter(VitalEvent.pet_id == pid) \
        .order_by(VitalEvent.ts.desc()).limit(1440).all()  # 24h @ 1min
    activity_names = ["resting", "walking", "running", "sleeping",
                      "scratching", "head_shaking", "licking", "eating", "playing"]
    return [{"ts": r.ts.isoformat(), "activity": activity_names[r.activity_class]
             if r.activity_class < 9 else "unknown",
             "hr": r.hr_bpm, "hrv": r.hrv_rmssd_ms}
            for r in reversed(rows)]


@app.get("/api/v1/pet/{pid}/vitals")
def get_vitals(pid: int, db: Session = Depends(get_db)):
    """Latest vitals + 7-day trend."""
    rows = db.query(VitalEvent).filter(VitalEvent.pet_id == pid) \
        .order_by(VitalEvent.ts.desc()).limit(10080).all()  # 7 days @ 1min
    latest = rows[0] if rows else None
    baseline = db.query(Baseline).filter(Baseline.pet_id == pid).first()
    return {
        "current": {
            "hr": latest.hr_bpm if latest else 0,
            "hrv_ms": latest.hrv_rmssd_ms if latest else 0,
            "temp_c": latest.skin_temp_c if latest else 0,
            "activity": latest.activity_class if latest else 0,
        },
        "baseline": {
            "hr": baseline.baseline_hr if baseline else 0,
            "hrv_ms": baseline.baseline_hrv if baseline else 0,
            "established": baseline.established if baseline else False,
        },
        "trend": [{"ts": r.ts.isoformat(), "hr": r.hr_bpm, "hrv": r.hrv_rmssd_ms}
                  for r in reversed(rows[-1440:])],  # last 24h
    }


@app.get("/api/v1/pet/{pid}/feeding")
def get_feeding(pid: int, db: Session = Depends(get_db)):
    """Feeding log + intake trends."""
    rows = db.query(FeedingEvent).filter(FeedingEvent.pet_id == pid) \
        .order_by(FeedingEvent.ts.desc()).limit(100).all()
    return [{"ts": r.ts.isoformat(), "dispensed_g": r.dispensed_g,
             "consumed_g": r.consumed_g, "water_ml": r.water_ml,
             "hopper_pct": r.hopper_pct,
             "appetite_loss": r.appetite_loss}
            for r in rows]


@app.get("/api/v1/pet/{pid}/anxiety")
def get_anxiety(pid: int, db: Session = Depends(get_db)):
    """Separation anxiety episodes + severity."""
    rows = db.query(BehaviorEvent).filter(
        BehaviorEvent.pet_id == pid,
        BehaviorEvent.is_anxiety_episode == True
    ).order_by(BehaviorEvent.ts.desc()).limit(50).all()
    return [{"ts": r.ts.isoformat(), "duration_s": r.duration_s,
             "behavior": ["resting","pacing","vocalizing","destructive",
                          "elimination","playing"][r.behavior_class]
             if r.behavior_class < 6 else "unknown",
             "vocalization": r.vocalization}
            for r in rows]


@app.get("/api/v1/pet/{pid}/alerts")
def get_alerts(pid: int, db: Session = Depends(get_db)):
    rows = db.query(AlertRecord).filter(AlertRecord.pet_id == pid) \
        .order_by(AlertRecord.ts.desc()).limit(100).all()
    return [{"id": r.id, "ts": r.ts.isoformat(), "type": r.alert_type,
             "severity": r.severity, "message": r.message,
             "acknowledged": r.acknowledged} for r in rows]


@app.post("/api/v1/vet/report/{pid}")
def vet_report(pid: int, db: Session = Depends(get_db)):
    """Generate a structured health report for the vet portal."""
    vitals = get_vitals(pid, db)
    wellness = get_wellness(pid, db)
    feeding = get_feeding(pid, db)
    anxiety = get_anxiety(pid, db)
    alerts = get_alerts(pid, db)
    baseline = db.query(Baseline).filter(Baseline.pet_id == pid).first()
    return {
        "pet_id": pid,
        "generated_at": datetime.utcnow().isoformat(),
        "baseline": {
            "established": baseline.established if baseline else False,
            "baseline_hr": baseline.baseline_hr if baseline else 0,
            "baseline_hrv_ms": baseline.baseline_hrv if baseline else 0,
        },
        "current_vitals": vitals["current"],
        "wellness": wellness,
        "feeding_summary": {
            "recent_meals": feeding[:10],
            "appetite_loss_count": sum(1 for f in feeding if f["appetite_loss"]),
        },
        "anxiety_episodes": anxiety[:5],
        "recent_alerts": alerts[:10],
    }


@app.websocket("/api/v1/ws/alerts/{pet_id}")
async def ws_alerts(ws: WebSocket, pet_id: int):
    await alert_mgr.connect(pet_id, ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        alert_mgr.disconnect(pet_id, ws)