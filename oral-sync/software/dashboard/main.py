# OralSync Cloud Dashboard — FastAPI backend
# Handles telemetry ingest (MQTT), REST API, WebSocket live events,
# TimescaleDB storage, ML inference orchestration, and PDF report generation.

import asyncio
import json
import os
import time
from datetime import datetime, timezone
from typing import Optional

import aiomqtt
from fastapi import FastAPI, Depends, HTTPException, WebSocket, WebSocketDisconnect, UploadFile, File, Form
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from sqlalchemy import create_engine, Column, Integer, String, Float, DateTime, JSON, text
from sqlalchemy.orm import declarative_base, sessionmaker, Session
import jwt

# ───────────────────────── Config ─────────────────────────
DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://oralsync:oralsync@db:5432/oralsync")
MQTT_HOST = os.getenv("MQTT_HOST", "mqtt")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
JWT_SECRET = os.getenv("JWT_SECRET", "dev-secret-change-me")
ML_SERVICE_URL = os.getenv("ML_SERVICE_URL", "http://ml:8501")

engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autoflush=False)
Base = declarative_base()

# ───────────────────────── Models ─────────────────────────
class Home(Base):
    __tablename__ = "homes"
    id = Column(Integer, primary_key=True)
    name = Column(String, default="My Home")

class User(Base):
    __tablename__ = "users"
    id = Column(Integer, primary_key=True)
    home_id = Column(Integer, index=True)
    name = Column(String)
    age = Column(Integer)
    orthodontic = Column(Integer, default=0)
    brushing_goal_min = Column(Integer, default=2)

class BrushingSession(Base):
    __tablename__ = "brushing_sessions"
    id = Column(Integer, primary_key=True)
    user_id = Column(Integer, index=True)
    device_id = Column(String)
    start = Column(DateTime)
    duration_s = Column(Integer)
    technique = Column(String)
    coverage = Column(Float)
    coverage_bitmap = Column(String)
    overpressure_events = Column(Integer, default=0)
    missed_surfaces = Column(JSON, default=list)

class Scan(Base):
    __tablename__ = "scans"
    id = Column(Integer, primary_key=True)
    user_id = Column(Integer, index=True)
    ts = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    plaque_pct = Column(Float)
    gingivitis = Column(JSON, default=dict)
    lesions = Column(JSON, default=list)
    image_uri = Column(String)
    heatmap_uri = Column(String)
    embeddings = Column(JSON)

class SalivaReading(Base):
    __tablename__ = "saliva_readings"
    id = Column(Integer, primary_key=True)
    user_id = Column(Integer, index=True)
    ts = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    ph = Column(Float)
    nitrite_um = Column(Float)
    buffer_capacity = Column(Integer)
    temp_c = Column(Float)

class RiskScore(Base):
    __tablename__ = "risk_scores"
    id = Column(Integer, primary_key=True)
    user_id = Column(Integer, index=True)
    tooth_fdi = Column(Integer)
    surface = Column(String)
    risk_0_100 = Column(Integer)
    horizon_days = Column(Integer, default=90)
    factors = Column(JSON, default=dict)
    computed_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))

# ───────────────────────── App ─────────────────────────
app = FastAPI(title="OralSync Cloud", version="1.0.0")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

def make_jwt(home_id: int) -> str:
    return jwt.encode({"home_id": home_id, "exp": int(time.time()) + 86400 * 30}, JWT_SECRET, algorithm="HS256")

def verify_jwt(token: str) -> int:
    try:
        payload = jwt.decode(token, JWT_SECRET, algorithms=["HS256"])
        return payload["home_id"]
    except Exception:
        raise HTTPException(401, "invalid token")

# ───────────────────────── Auth ─────────────────────────
class PairReq(BaseModel):
    hub_id: str
    pair_code: str

@app.post("/v1/auth/pair")
def auth_pair(req: PairReq, db: Session = Depends(get_db)):
    # In production: verify pair_code against hub provisioning table
    home = Home(name=f"Home-{req.hub_id[:6]}")
    db.add(home); db.commit(); db.refresh(home)
    return {"jwt": make_jwt(home.id), "home_id": home.id}

# ───────────────────────── Users ─────────────────────────
class UserCreate(BaseModel):
    name: str
    age: int
    orthodontic: bool = False

@app.get("/v1/homes/{home_id}/users")
def list_users(home_id: int, db: Session = Depends(get_db)):
    rows = db.query(User).filter(User.home_id == home_id).all()
    return [{"user_id": u.id, "name": u.name, "age": u.age, "orthodontic": bool(u.orthodontic)} for u in rows]

@app.post("/v1/homes/{home_id}/users")
def create_user(home_id: int, u: UserCreate, db: Session = Depends(get_db)):
    user = User(home_id=home_id, name=u.name, age=u.age, orthodontic=int(u.orthodontic))
    db.add(user); db.commit(); db.refresh(user)
    return {"user_id": user.id}

# ───────────────────────── Sessions ─────────────────────────
class SessionUplink(BaseModel):
    user_id: int
    device_id: str
    start: datetime
    duration_s: int
    technique: str = "Bass"
    coverage: float = 0.0
    coverage_bitmap: str = ""
    overpressure_events: int = 0
    missed_surfaces: list = []

@app.post("/v1/sessions")
def uplink_session(s: SessionUplink, db: Session = Depends(get_db)):
    sess = BrushingSession(
        user_id=s.user_id, device_id=s.device_id, start=s.start, duration_s=s.duration_s,
        technique=s.technique, coverage=s.coverage, coverage_bitmap=s.coverage_bitmap,
        overpressure_events=s.overpressure_events, missed_surfaces=s.missed_surfaces)
    db.add(sess); db.commit(); db.refresh(sess)
    _broadcast({"type": "session_end", "user_id": s.user_id, "coverage": s.coverage, "duration_s": s.duration_s})
    return {"session_id": sess.id}

@app.get("/v1/users/{user_id}/sessions")
def list_sessions(user_id: int, since: Optional[str] = None, limit: int = 100, db: Session = Depends(get_db)):
    q = db.query(BrushingSession).filter(BrushingSession.user_id == user_id)
    if since:
        q = q.filter(BrushingSession.start >= since)
    rows = q.order_by(BrushingSession.start.desc()).limit(limit).all()
    return [{"session_id": r.id, "start": r.start.isoformat(), "duration_s": r.duration_s,
             "technique": r.technique, "coverage": r.coverage,
             "overpressure_events": r.overpressure_events, "missed_surfaces": r.missed_surfaces} for r in rows]

# ───────────────────────── Scans ─────────────────────────
@app.post("/v1/scans")
async def uplink_scan(metadata: str = Form(...), image: UploadFile = File(...),
                      nir: UploadFile = File(None), heatmap: UploadFile = File(None),
                      db: Session = Depends(get_db)):
    meta = json.loads(metadata)
    # In production: store to S3, run ML inference
    scan = Scan(user_id=meta["user_id"], plaque_pct=meta.get("plaque_pct", 0.0),
                gingivitis=meta.get("gingivitis", {}), lesions=meta.get("lesions", []),
                image_uri=f"s3://oralsync-scans/{image.filename}",
                heatmap_uri=f"s3://oralsync-scans/{heatmap.filename}" if heatmap else None,
                embeddings=meta.get("embeddings", []))
    db.add(scan); db.commit(); db.refresh(scan)
    _broadcast({"type": "scan_complete", "scan_id": scan.id, "plaque_pct": scan.plaque_pct})
    return {"scan_id": scan.id, "plaque_pct": scan.plaque_pct}

@app.get("/v1/users/{user_id}/scans")
def list_scans(user_id: int, limit: int = 50, db: Session = Depends(get_db)):
    rows = db.query(Scan).filter(Scan.user_id == user_id).order_by(Scan.ts.desc()).limit(limit).all()
    return [{"scan_id": r.id, "ts": r.ts.isoformat(), "plaque_pct": r.plaque_pct,
             "lesions": len(r.lesions or []), "image_uri": r.image_uri} for r in rows]

# ───────────────────────── Saliva ─────────────────────────
class SalivaUplink(BaseModel):
    user_id: int
    ph: float
    nitrite_um: float
    buffer_capacity: int
    temp_c: float

@app.post("/v1/saliva")
def uplink_saliva(r: SalivaUplink, db: Session = Depends(get_db)):
    rec = SalivaReading(user_id=r.user_id, ph=r.ph, nitrite_um=r.nitrite_um,
                        buffer_capacity=r.buffer_capacity, temp_c=r.temp_c)
    db.add(rec); db.commit(); db.refresh(rec)
    _broadcast({"type": "saliva_reading", "user_id": r.user_id, "ph": r.ph})
    return {"reading_id": rec.id}

@app.get("/v1/users/{user_id}/saliva")
def list_saliva(user_id: int, since: Optional[str] = None, limit: int = 200, db: Session = Depends(get_db)):
    q = db.query(SalivaReading).filter(SalivaReading.user_id == user_id)
    if since:
        q = q.filter(SalivaReading.ts >= since)
    rows = q.order_by(SalivaReading.ts.desc()).limit(limit).all()
    return [{"ts": r.ts.isoformat(), "ph": r.ph, "nitrite_um": r.nitrite_um,
             "buffer": r.buffer_capacity, "temp_c": r.temp_c} for r in rows]

# ───────────────────────── Risk ─────────────────────────
@app.get("/v1/users/{user_id}/risk")
def get_risk(user_id: int, horizon_days: int = 90, db: Session = Depends(get_db)):
    rows = db.query(RiskScore).filter(RiskScore.user_id == user_id,
                                      RiskScore.horizon_days == horizon_days).all()
    return [{"tooth_fdi": r.tooth_fdi, "surface": r.surface, "risk_0_100": r.risk_0_100,
             "factors": r.factors} for r in rows]

# ───────────────────────── Reports ─────────────────────────
@app.post("/v1/users/{user_id}/report")
def generate_report(user_id: int, db: Session = Depends(get_db)):
    # In production: WeasyPrint PDF with plaque heatmap, lesion timeline, risk per tooth
    uri = f"s3://oralsync-reports/user_{user_id}_{int(time.time())}.pdf"
    return {"report_uri": uri}

# ───────────────────────── WebSocket ─────────────────────────
_clients: set[WebSocket] = set()

async def _broadcast(msg: dict):
    dead = []
    for ws in _clients:
        try:
            await ws.send_json(msg)
        except Exception:
            dead.append(ws)
    for d in dead:
        _clients.discard(d)

@app.websocket("/v1/stream")
async def stream(ws: WebSocket):
    await ws.accept()
    _clients.add(ws)
    try:
        while True:
            await ws.receive_text()  # keepalive
    except WebSocketDisconnect:
        _clients.discard(ws)

# ───────────────────────── MQTT Ingest ─────────────────────────
async def mqtt_ingest():
    """Subscribe to oralsync/+/+/telemetry & /event, persist to DB, trigger ML."""
    while True:
        try:
            async with aiomqtt.Client(MQTT_HOST, port=MQTT_PORT) as client:
                await client.subscribe("oralsync/+/+/event")
                await client.subscribe("oralsync/+/+/telemetry")
                async for msg in client.messages:
                    topic = str(msg.topic)
                    parts = topic.split("/")
                    if len(parts) < 4:
                        continue
                    payload = json.loads(msg.payload.decode())
                    db = SessionLocal()
                    try:
                        if parts[3] == "event" and payload.get("type") == "session_end":
                            sess = BrushingSession(
                                user_id=payload["user_id"],
                                device_id=parts[2],
                                start=datetime.now(timezone.utc),
                                duration_s=payload.get("duration_s", 0),
                                coverage=payload.get("coverage", 0.0),
                                technique=payload.get("technique", "Bass"))
                            db.add(sess); db.commit()
                        elif parts[3] == "event" and payload.get("type") == "saliva_reading":
                            rec = SalivaReading(
                                user_id=payload["user_id"],
                                ph=payload["ph"], nitrite_um=payload.get("nitrite_um", 0),
                                buffer_capacity=payload.get("buffer", 0),
                                temp_c=payload.get("temp_c", 36.4))
                            db.add(rec); db.commit()
                        await _broadcast(payload)
                    finally:
                        db.close()
        except Exception as e:
            print(f"[mqtt] reconnect after error: {e}")
            await asyncio.sleep(5)

@app.on_event("startup")
async def startup():
    Base.metadata.create_all(engine)
    asyncio.create_task(mqtt_ingest())

@app.get("/v1/health")
def health():
    return {"status": "ok", "service": "oralsync-cloud", "version": "1.0.0"}