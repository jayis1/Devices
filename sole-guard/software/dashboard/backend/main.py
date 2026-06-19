"""
SoleGuard Cloud Backend — FastAPI + MQTT

Ingests pressure/temperature/gait/edema/scan events from the hub via MQTT,
stores them in PostgreSQL/TimescaleDB, serves the patient + clinician APIs,
runs the alert engine, and exposes a WebSocket for real-time mobile alerts.
"""

import os
import json
import time
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
import boto3

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://sole:sole@db:5432/soleguard")
MQTT_HOST    = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT    = int(os.getenv("MQTT_PORT", "1883"))
S3_ENDPOINT  = os.getenv("S3_ENDPOINT", "http://minio:9000")
S3_BUCKET    = os.getenv("S3_BUCKET", "sole-scans")
ALERT_TEMP_ASYM_THRESHOLD = 2.2   # °C — validated clinical threshold
ALERT_RISK_THRESHOLD      = 65     # 0-100

engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autodeexpire_on_commit=False)
Base = declarative_base()
s3 = boto3.client("s3", endpoint_url=S3_ENDPOINT,
                  aws_access_key_id=os.getenv("S3_KEY", "minio"),
                  aws_secret_access_key=os.getenv("S3_SECRET", "minio123"))


# ---------------------------------------------------------------------------
# Models
# ---------------------------------------------------------------------------
class Patient(Base):
    __tablename__ = "patients"
    id            = Column(Integer, primary_key=True)
    name          = Column(String(120))
    dob           = Column(DateTime)
    clinician_id  = Column(Integer)
    diabetes_type = Column(Integer)       # 1 or 2
    neuropathy_stage = Column(Integer)    # 0-4 (Semmes-Weinstein)
    created_at    = Column(DateTime, default=datetime.utcnow)


class FootEvent(Base):
    """Time-series: one row per 30s reporting window from a node."""
    __tablename__ = "foot_events"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    patient_id    = Column(Integer, index=True)
    node_id       = Column(Integer)        # 1=L-insole 2=R-insole 3=ankle
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    pressure_l    = Column(Text)           # JSON: 24 values
    pressure_r    = Column(Text)
    temp_l        = Column(Text)           # JSON: 8 centi-degC
    temp_r        = Column(Text)
    pti_l         = Column(Integer)
    pti_r         = Column(Integer)
    gait          = Column(Text)           # JSON: 8 features
    edema_index   = Column(Integer)
    risk_l        = Column(Integer)
    risk_r        = Column(Integer)
    flags         = Column(Integer)


class ScanRecord(Base):
    __tablename__ = "scan_records"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    patient_id    = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    foot_side     = Column(Integer)        # 0=left 1=right
    wound_class   = Column(Integer)        # 0=normal..6
    confidence    = Column(Integer)
    weight_dag    = Column(Integer)
    image_key     = Column(String(256))    # S3 object key


class AlertRecord(Base):
    __tablename__ = "alerts"
    id            = Column(BigInteger, primary_key=True, autoincrement=True)
    patient_id    = Column(Integer, index=True)
    ts            = Column(DateTime, default=datetime.utcnow, index=True)
    alert_type    = Column(String(40))     # hotspot, temp_asym, fall, wound, edema, high_risk
    severity      = Column(String(20))     # low, medium, high
    message       = Column(Text)
    acknowledged  = Column(Boolean, default=False)


Base.metadata.create_all(bind=engine)


# ---------------------------------------------------------------------------
# Pydantic schemas
# ---------------------------------------------------------------------------
class PressureTempIn(BaseModel):
    patient_id: int
    node_id: int
    pressure: List[int] = Field(..., max_length=24)
    temp_centic: List[int] = Field(..., max_length=8)
    pti_centic: int = 0
    flags: int = 0


class GaitIn(BaseModel):
    patient_id: int
    node_id: int
    gait: List[int] = Field(..., max_length=8)
    flags: int = 0


class EdemaIn(BaseModel):
    patient_id: int
    impedance_ohm: int
    edema_index: int
    skin_temp_centic: int
    flags: int = 0


class ScanIn(BaseModel):
    patient_id: int
    foot_side: int
    wound_class: int
    confidence: int
    weight_dag: int
    image_key: str


class RiskOut(BaseModel):
    risk_l: int
    risk_r: int
    trend: List[dict]


# ---------------------------------------------------------------------------
# WebSocket connection manager (for real-time mobile alerts)
# ---------------------------------------------------------------------------
class AlertManager:
    def __init__(self):
        self.connections: dict[int, list[WebSocket]] = {}

    async def connect(self, patient_id: int, ws: WebSocket):
        await ws.accept()
        self.connections.setdefault(patient_id, []).append(ws)

    def disconnect(self, patient_id: int, ws: WebSocket):
        if patient_id in self.connections:
            self.connections[patient_id].remove(ws)

    async def broadcast(self, patient_id: int, message: dict):
        for ws in self.connections.get(patient_id, []):
            try:
                await ws.send_json(message)
            except Exception:
                pass

alert_mgr = AlertManager()


# ---------------------------------------------------------------------------
# Alert engine
# ---------------------------------------------------------------------------
def evaluate_alerts(db: Session, patient_id: int, payload: dict) -> Optional[dict]:
    """Rule-based clinical alerts (independent of ML model — fail-safe)."""
    alerts = []

    temp_l = payload.get("temp_l", [])
    temp_r = payload.get("temp_r", [])
    if temp_l and temp_r:
        for i in range(min(len(temp_l), len(temp_r))):
            asym = abs(temp_l[i] - temp_r[i]) / 100.0
            if asym > ALERT_TEMP_ASYM_THRESHOLD:
                alerts.append({
                    "type": "temp_asym",
                    "severity": "high",
                    "message": f"Temperature asymmetry {asym:.1f}°C at zone {i} — offload and check feet",
                })

    risk_l = payload.get("risk_l", 0)
    risk_r = payload.get("risk_r", 0)
    if risk_l > ALERT_RISK_THRESHOLD or risk_r > ALERT_RISK_THRESHOLD:
        side = "left" if risk_l > risk_r else "right"
        alerts.append({
            "type": "high_risk",
            "severity": "high",
            "message": f"Ulcer risk {max(risk_l, risk_r)}% on {side} foot — offload now",
        })

    flags = payload.get("flags", 0)
    if flags & 0x04:
        alerts.append({"type": "fall", "severity": "high",
                       "message": "Fall detected — emergency contact notified"})
    if flags & 0x08:
        alerts.append({"type": "wound", "severity": "high",
                       "message": "Wound detected by foot scanner — contact clinician"})
    if flags & 0x20:
        alerts.append({"type": "edema", "severity": "medium",
                       "message": "Ankle edema elevated — elevate legs, contact clinician"})

    for a in alerts:
        rec = AlertRecord(patient_id=patient_id, alert_type=a["type"],
                          severity=a["severity"], message=a["message"])
        db.add(rec)
    db.commit()
    return alerts[0] if alerts else None


# ---------------------------------------------------------------------------
# MQTT subscriber (runs in background thread)
# ---------------------------------------------------------------------------
def on_mqtt_connect(client, userdata, flags, rc):
    client.subscribe("soleguard/+/pressure")
    client.subscribe("soleguard/+/gait")
    client.subscribe("soleguard/+/edema")
    client.subscribe("soleguard/+/scan")
    client.subscribe("soleguard/+/risk")


def on_mqtt_message(client, userdata, msg):
    topic_parts = msg.topic.split("/")
    patient_id = int(topic_parts[1])
    kind = topic_parts[2]
    data = json.loads(msg.payload)
    db = SessionLocal()
    try:
        if kind == "pressure":
            ev = FootEvent(patient_id=patient_id, node_id=data["node_id"],
                           pressure_l=json.dumps(data.get("pressure", [])),
                           temp_l=json.dumps(data.get("temp_centic", [])),
                           pti_l=data.get("pti_centic", 0),
                           flags=data.get("flags", 0))
            db.add(ev)
            db.commit()
            alerts = evaluate_alerts(db, patient_id, data)
            if alerts:
                asyncio.run(alert_mgr.broadcast(patient_id, alerts))
        elif kind == "scan":
            rec = ScanRecord(patient_id=patient_id, foot_side=data["foot_side"],
                             wound_class=data["wound_class"],
                             confidence=data["confidence"],
                             weight_dag=data["weight_dag"],
                             image_key=data["image_key"])
            db.add(rec)
            db.commit()
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
app = FastAPI(title="SoleGuard API", version="1.0.0", lifespan=lifespan)
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


@app.post("/api/v1/ingest/pressure")
def ingest_pressure(payload: PressureTempIn, db: Session = Depends(get_db)):
    ev = FootEvent(patient_id=payload.patient_id, node_id=payload.node_id,
                   pressure_l=json.dumps(payload.pressure),
                   temp_l=json.dumps(payload.temp_centic),
                   pti_l=payload.pti_centic, flags=payload.flags)
    db.add(ev)
    db.commit()
    alerts = evaluate_alerts(db, payload.patient_id, {
        "temp_l": payload.temp_centic, "flags": payload.flags
    })
    return {"status": "stored", "alerts": alerts or []}


@app.get("/api/v1/patient/{pid}/risk")
def get_risk(pid: int, db: Session = Depends(get_db)):
    """Current + 7-day ulcer-risk trend."""
    rows = db.query(FootEvent).filter(FootEvent.patient_id == pid) \
        .order_by(FootEvent.ts.desc()).limit(336).all()  # 7 days @ 30s
    trend = [{"ts": r.ts.isoformat(), "risk_l": r.risk_l, "risk_r": r.risk_r}
             for r in reversed(rows)]
    latest = rows[0] if rows else None
    return RiskOut(risk_l=latest.risk_l if latest else 0,
                   risk_r=latest.risk_r if latest else 0,
                   trend=trend)


@app.get("/api/v1/patient/{pid}/heatmap")
def get_heatmap(pid: int, db: Session = Depends(get_db)):
    """Latest pressure + temperature heat map data."""
    row = db.query(FootEvent).filter(FootEvent.patient_id == pid) \
        .order_by(FootEvent.ts.desc()).first()
    if not row:
        raise HTTPException(404, "No data")
    return {
        "ts": row.ts.isoformat(),
        "pressure_l": json.loads(row.pressure_l) if row.pressure_l else [],
        "pressure_r": json.loads(row.pressure_r) if row.pressure_r else [],
        "temp_l": json.loads(row.temp_l) if row.temp_l else [],
        "temp_r": json.loads(row.temp_r) if row.temp_r else [],
        "pti_l": row.pti_l, "pti_r": row.pti_r,
    }


@app.get("/api/v1/patient/{pid}/gait")
def get_gait(pid: int, db: Session = Depends(get_db)):
    rows = db.query(FootEvent).filter(FootEvent.patient_id == pid) \
        .order_by(FootEvent.ts.desc()).limit(336).all()
    return [{"ts": r.ts.isoformat(), "gait": json.loads(r.gait) if r.gait else []}
            for r in reversed(rows)]


@app.get("/api/v1/patient/{pid}/scans")
def get_scans(pid: int, db: Session = Depends(get_db)):
    rows = db.query(ScanRecord).filter(ScanRecord.patient_id == pid) \
        .order_by(ScanRecord.ts.desc()).limit(100).all()
    classes = ["normal", "callus", "blister", "fissure", "ulcer", "fungal", "maceration"]
    return [{"id": r.id, "ts": r.ts.isoformat(), "foot": "L" if r.foot_side == 0 else "R",
             "wound_class": classes[r.wound_class], "confidence": r.confidence,
             "weight_kg": r.weight_dag / 10.0, "image_key": r.image_key}
            for r in rows]


@app.get("/api/v1/patient/{pid}/alerts")
def get_alerts(pid: int, db: Session = Depends(get_db)):
    rows = db.query(AlertRecord).filter(AlertRecord.patient_id == pid) \
        .order_by(AlertRecord.ts.desc()).limit(100).all()
    return [{"id": r.id, "ts": r.ts.isoformat(), "type": r.alert_type,
             "severity": r.severity, "message": r.message,
             "acknowledged": r.acknowledged} for r in rows]


@app.post("/api/v1/clinician/report/{pid}")
def clinician_report(pid: int, db: Session = Depends(get_db)):
    """Generate a structured wound + risk report for the clinician portal."""
    risk = get_risk(pid, db)
    heatmap = get_heatmap(pid, db)
    scans = get_scans(pid, db)
    alerts = get_alerts(pid, db)
    return {
        "patient_id": pid,
        "generated_at": datetime.utcnow().isoformat(),
        "risk": risk,
        "heatmap": heatmap,
        "recent_scans": scans[:5],
        "recent_alerts": alerts[:10],
    }


@app.websocket("/api/v1/ws/alerts/{patient_id}")
async def ws_alerts(ws: WebSocket, patient_id: int):
    await alert_mgr.connect(patient_id, ws)
    try:
        while True:
            await ws.receive_text()
    except WebSocketDisconnect:
        alert_mgr.disconnect(patient_id, ws)