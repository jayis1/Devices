"""
DriveSync Cloud Backend — FastAPI Application

Receives trip data and alerts from the Dash Hub via MQTT.
Stores in TimescaleDB, computes trip safety scores,
generates coaching reports, and sends emergency contact notifications.

License: MIT
"""

import os
from datetime import datetime, timedelta
from typing import Optional, List
from uuid import uuid4

from fastapi import FastAPI, Depends, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from sqlalchemy import (
    create_engine, Column, String, Float, Integer, DateTime,
    ForeignKey, Boolean, Text,
)
from sqlalchemy.orm import sessionmaker, declarative_base, Session
from passlib.context import CryptContext
from jose import jwt, JWTError
import paho.mqtt.client as mqtt
import asyncio
import json

# ─────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────

DATABASE_URL = os.getenv(
    "DATABASE_URL",
    "postgresql://drivesync:drivesync@localhost:5432/drivesync",
)
SECRET_KEY = os.getenv("SECRET_KEY", "drivesync-dev-secret-change-in-production")
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 1440  # 24 hours
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))

# ─────────────────────────────────────────────────────────────────────
# Database
# ─────────────────────────────────────────────────────────────────────

engine = create_engine(DATABASE_URL, pool_pre_ping=True)
SessionLocal = sessionmaker(bind=engine, autocommit=False, autoflush=False)
Base = declarative_base()
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")

# ─────────────────────────────────────────────────────────────────────
# Models
# ─────────────────────────────────────────────────────────────────────


class User(Base):
    __tablename__ = "users"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    email = Column(String, unique=True, index=True)
    name = Column(String)
    hashed_password = Column(String)
    emergency_contact_name = Column(String, nullable=True)
    emergency_contact_phone = Column(String, nullable=True)
    created_at = Column(DateTime, default=datetime.utcnow)


class Trip(Base):
    __tablename__ = "trips"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    user_id = Column(String, ForeignKey("users.id"))
    hub_id = Column(String)
    start_time = Column(DateTime, default=datetime.utcnow)
    end_time = Column(DateTime, nullable=True)
    duration_sec = Column(Integer, default=0)
    distance_km = Column(Float, default=0)
    safety_score = Column(Float, default=100.0)
    avg_risk = Column(Float, default=0.0)
    max_risk = Column(Float, default=0.0)
    drowsiness_events = Column(Integer, default=0)
    distraction_events = Column(Integer, default=0)
    created_at = Column(DateTime, default=datetime.utcnow)


class RiskEvent(Base):
    """A drowsiness or distraction event during a trip."""
    __tablename__ = "risk_events"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    trip_id = Column(String, ForeignKey("trips.id"))
    time = Column(DateTime, default=datetime.utcnow, index=True)
    event_type = Column(String)  # 'drowsiness', 'distraction', 'critical'
    risk_score = Column(Integer)  # 0-100
    alert_level = Column(Integer)  # 0-4
    source = Column(String)  # 'perclos', 'steering', 'hrv', 'fusion'
    speed_kmh = Column(Float, nullable=True)
    latitude = Column(Float, nullable=True)
    longitude = Column(Float, nullable=True)


class FusionReading(Base):
    """Time-series of fused drowsiness risk data."""
    __tablename__ = "fusion_readings"
    id = Column(Integer, primary_key=True, autoincrement=True)
    time = Column(DateTime, default=datetime.utcnow, index=True)
    trip_id = Column(String, ForeignKey("trips.id"), nullable=True)
    hub_id = Column(String)
    risk_score = Column(Integer)
    perclos = Column(Float)
    blink_rate = Column(Integer)
    head_bob_count = Column(Integer)
    hrv_rmssd = Column(Integer)
    hr = Column(Integer)
    speed_kmh = Column(Integer)
    rpm = Column(Integer)
    steering_risk = Column(Integer)
    hrv_risk = Column(Integer)


class Device(Base):
    __tablename__ = "devices"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    user_id = Column(String, ForeignKey("users.id"))
    hub_id = Column(String)
    wheel_node_id = Column(String, nullable=True)
    belt_tag_id = Column(String, nullable=True)
    obd_dongle_id = Column(String, nullable=True)
    firmware_version = Column(String, nullable=True)
    paired_at = Column(DateTime, default=datetime.utcnow)


Base.metadata.create_all(engine)

# ─────────────────────────────────────────────────────────────────────
# Schemas
# ─────────────────────────────────────────────────────────────────────


class UserBase(BaseModel):
    email: str
    name: str
    emergency_contact_name: Optional[str] = None
    emergency_contact_phone: Optional[str] = None


class UserCreate(UserBase):
    password: str


class UserResponse(UserBase):
    id: str

    class Config:
        from_attributes = True


class TripResponse(BaseModel):
    id: str
    start_time: datetime
    end_time: Optional[datetime]
    duration_sec: int
    distance_km: float
    safety_score: float
    avg_risk: float
    max_risk: float
    drowsiness_events: int
    distraction_events: int

    class Config:
        from_attributes = True


class RiskEventResponse(BaseModel):
    id: str
    time: datetime
    event_type: str
    risk_score: int
    alert_level: int
    source: str
    speed_kmh: Optional[float]

    class Config:
        from_attributes = True


class Token(BaseModel):
    access_token: str
    token_type: str


class CoachingReport(BaseModel):
    week_start: datetime
    week_end: datetime
    total_trips: int
    total_distance_km: float
    avg_safety_score: float
    total_drowsiness_events: int
    total_distraction_events: int
    riskiest_time_of_day: Optional[str]
    recommendations: List[str]


# ─────────────────────────────────────────────────────────────────────
# Auth
# ─────────────────────────────────────────────────────────────────────


def create_access_token(data: dict):
    to_encode = data.copy()
    expire = datetime.utcnow() + timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)


def get_current_user(
    token: str = Depends(oauth2_scheme),
    db: Session = Depends(lambda: SessionLocal()),
):
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        email = payload.get("sub")
        if email is None:
            raise HTTPException(401, "Invalid token")
        user = db.query(User).filter(User.email == email).first()
        if user is None:
            raise HTTPException(401, "User not found")
        return user
    except JWTError:
        raise HTTPException(401, "Invalid token")


# ─────────────────────────────────────────────────────────────────────
# FastAPI
# ─────────────────────────────────────────────────────────────────────

app = FastAPI(title="DriveSync API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


# ─────────────────────────────────────────────────────────────────────
# Auth Endpoints
# ─────────────────────────────────────────────────────────────────────


@app.post("/api/v1/auth/register", response_model=UserResponse)
def register(user: UserCreate, db: Session = Depends(get_db)):
    if db.query(User).filter(User.email == user.email).first():
        raise HTTPException(400, "Email already registered")
    hashed = pwd_context.hash(user.password)
    db_user = User(
        email=user.email,
        name=user.name,
        hashed_password=hashed,
        emergency_contact_name=user.emergency_contact_name,
        emergency_contact_phone=user.emergency_contact_phone,
    )
    db.add(db_user)
    db.commit()
    db.refresh(db_user)
    return db_user


@app.post("/api/v1/auth/login", response_model=Token)
def login(form: OAuth2PasswordRequestForm = Depends(), db: Session = Depends(get_db)):
    user = db.query(User).filter(User.email == form.username).first()
    if not user or not pwd_context.verify(form.password, user.hashed_password):
        raise HTTPException(401, "Invalid credentials")
    token = create_access_token({"sub": user.email})
    return {"access_token": token, "token_type": "bearer"}


# ─────────────────────────────────────────────────────────────────────
# Trip Endpoints
# ─────────────────────────────────────────────────────────────────────


@app.get("/api/v1/trips", response_model=List[TripResponse])
def list_trips(
    limit: int = 20,
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    return (
        db.query(Trip)
        .filter(Trip.user_id == user.id)
        .order_by(Trip.start_time.desc())
        .limit(limit)
        .all()
    )


@app.get("/api/v1/trips/{trip_id}", response_model=TripResponse)
def get_trip(trip_id: str, user: User = Depends(get_current_user), db: Session = Depends(get_db)):
    trip = db.query(Trip).filter(Trip.id == trip_id, Trip.user_id == user.id).first()
    if not trip:
        raise HTTPException(404, "Trip not found")
    return trip


@app.get("/api/v1/trips/{trip_id}/events", response_model=List[RiskEventResponse])
def get_trip_events(
    trip_id: str,
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    trip = db.query(Trip).filter(Trip.id == trip_id, Trip.user_id == user.id).first()
    if not trip:
        raise HTTPException(404, "Trip not found")
    return (
        db.query(RiskEvent)
        .filter(RiskEvent.trip_id == trip_id)
        .order_by(RiskEvent.time)
        .all()
    )


@app.get("/api/v1/trips/{trip_id}/timeline")
def get_trip_timeline(
    trip_id: str,
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    """Get the full risk timeline for a trip (fusion readings)."""
    trip = db.query(Trip).filter(Trip.id == trip_id, Trip.user_id == user.id).first()
    if not trip:
        raise HTTPException(404, "Trip not found")
    readings = (
        db.query(FusionReading)
        .filter(FusionReading.trip_id == trip_id)
        .order_by(FusionReading.time)
        .all()
    )
    return [
        {
            "time": r.time.isoformat(),
            "risk_score": r.risk_score,
            "perclos": r.perclos,
            "blink_rate": r.blink_rate,
            "hrv_rmssd": r.hrv_rmssd,
            "hr": r.hr,
            "speed_kmh": r.speed_kmh,
            "rpm": r.rpm,
        }
        for r in readings
    ]


# ─────────────────────────────────────────────────────────────────────
# Coaching Report
# ─────────────────────────────────────────────────────────────────────


@app.get("/api/v1/coaching/weekly", response_model=CoachingReport)
def get_weekly_coaching(
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    """Generate a weekly coaching report with recommendations."""
    week_ago = datetime.utcnow() - timedelta(days=7)
    trips = (
        db.query(Trip)
        .filter(Trip.user_id == user.id, Trip.start_time >= week_ago)
        .all()
    )

    total_trips = len(trips)
    total_distance = sum(t.distance_km for t in trips)
    avg_safety = sum(t.safety_score for t in trips) / max(total_trips, 1)
    total_drowsy = sum(t.drowsiness_events for t in trips)
    total_distract = sum(t.distraction_events for t in trips)

    # Find riskiest time of day
    riskiest_hour = None
    if trips:
        events = (
            db.query(RiskEvent)
            .join(Trip)
            .filter(Trip.user_id == user.id, Trip.start_time >= week_ago)
            .all()
        )
        if events:
            hour_counts = {}
            for e in events:
                h = e.time.hour
                hour_counts[h] = hour_counts.get(h, 0) + 1
            riskiest_hour = max(hour_counts, key=lambda h: hour_counts[h])

    # Generate recommendations
    recommendations = []
    if total_drowsy > 3:
        recommendations.append(
            "You experienced multiple drowsiness events this week. "
            "Consider taking breaks every 2 hours on long trips."
        )
    if avg_safety < 70:
        recommendations.append(
            "Your average safety score is below 70. Try to get more rest "
            "before driving, especially for early morning trips."
        )
    if total_distract > 5:
        recommendations.append(
            "You had multiple distraction events. Keep your phone out of reach "
            "while driving and set up your navigation before starting."
        )
    if riskiest_hour is not None:
        if riskiest_hour < 7:
            recommendations.append(
                f"Your riskiest driving time is around {riskiest_hour}:00. "
                "Consider shifting departure times or getting more sleep."
            )
        elif riskiest_hour >= 14 and riskiest_hour < 17:
            recommendations.append(
                f"Your riskiest driving time is around {riskiest_hour}:00. "
                "The mid-afternoon dip is common — consider a short walk "
                "or coffee before driving."
            )
    if not recommendations:
        recommendations.append("Great driving this week! Keep up the safe habits.")

    return CoachingReport(
        week_start=week_ago,
        week_end=datetime.utcnow(),
        total_trips=total_trips,
        total_distance_km=total_distance,
        avg_safety_score=round(avg_safety, 1),
        total_drowsiness_events=total_drowsy,
        total_distraction_events=total_distract,
        riskiest_time_of_day=f"{riskiest_hour}:00" if riskiest_hour else None,
        recommendations=recommendations,
    )


# ─────────────────────────────────────────────────────────────────────
# Device Pairing
# ─────────────────────────────────────────────────────────────────────


class DevicePairRequest(BaseModel):
    hub_id: str
    wheel_node_id: Optional[str] = None
    belt_tag_id: Optional[str] = None
    obd_dongle_id: Optional[str] = None


@app.post("/api/v1/devices/pair")
def pair_device(
    req: DevicePairRequest,
    user: User = Depends(get_current_user),
    db: Session = Depends(get_db),
):
    device = Device(
        user_id=user.id,
        hub_id=req.hub_id,
        wheel_node_id=req.wheel_node_id,
        belt_tag_id=req.belt_tag_id,
        obd_dongle_id=req.obd_dongle_id,
    )
    db.add(device)
    db.commit()
    db.refresh(device)
    return {"device_id": device.id, "status": "paired"}


# ─────────────────────────────────────────────────────────────────────
# WebSocket for real-time alerts
# ─────────────────────────────────────────────────────────────────────


class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: str):
        for conn in self.active_connections:
            try:
                await conn.send_text(message)
            except Exception:
                pass


manager = ConnectionManager()


@app.websocket("/api/v1/ws/alerts")
async def ws_alerts(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)


# ─────────────────────────────────────────────────────────────────────
# MQTT Integration
# ─────────────────────────────────────────────────────────────────────

mqtt_client = mqtt.Client()
current_trip_ids = {}  # hub_id → trip_id


def on_mqtt_connect(client, userdata, flags, rc):
    print(f"MQTT connected with result code {rc}")
    client.subscribe("drivesync/data/#")
    client.subscribe("drivesync/alerts/#")
    client.subscribe("drivesync/status/#")


def on_mqtt_message(client, userdata, msg):
    """Process incoming MQTT messages from Hub."""
    topic = msg.topic
    payload = msg.payload.decode("utf-8", errors="replace")

    if "data/fusion" in topic:
        data = json.loads(payload)
        db = SessionLocal()
        try:
            reading = FusionReading(
                hub_id=data.get("hub", "unknown"),
                risk_score=data.get("risk", 0),
                perclos=data.get("perclos", 0),
                blink_rate=data.get("blink", 0),
                head_bob_count=data.get("head_bob", 0),
                hrv_rmssd=data.get("hrv", 0),
                hr=data.get("hr", 0),
                speed_kmh=data.get("speed", 0),
                rpm=data.get("rpm", 0),
                steering_risk=data.get("steering_risk", 0),
                hrv_risk=data.get("hrv_risk", 0),
            )
            db.add(reading)
            db.commit()
        finally:
            db.close()

    elif "alerts/critical" in topic:
        data = json.loads(payload)
        db = SessionLocal()
        try:
            event = RiskEvent(
                event_type="critical",
                risk_score=data.get("risk", 100),
                alert_level=4,
                source="fusion",
            )
            db.add(event)
            db.commit()
        finally:
            db.close()

        # Broadcast to WebSocket clients
        asyncio.run(manager.broadcast(json.dumps(data)))

    elif "status/hub" in topic:
        data = json.loads(payload)
        hub_id = data.get("hub", "unknown")
        driving = data.get("driving", False)

        db = SessionLocal()
        try:
            if driving and hub_id not in current_trip_ids:
                trip = Trip(
                    hub_id=hub_id,
                    safety_score=100.0,
                )
                db.add(trip)
                db.commit()
                db.refresh(trip)
                current_trip_ids[hub_id] = trip.id
            elif not driving and hub_id in current_trip_ids:
                trip = db.query(Trip).filter(Trip.id == current_trip_ids[hub_id]).first()
                if trip:
                    trip.end_time = datetime.utcnow()
                    if trip.start_time:
                        trip.duration_sec = int(
                            (trip.end_time - trip.start_time).total_seconds()
                        )
                    # Compute safety score
                    readings = (
                        db.query(FusionReading)
                        .filter(FusionReading.trip_id == trip.id)
                        .all()
                    )
                    if readings:
                        avg_risk = sum(r.risk_score for r in readings) / len(readings)
                        max_risk = max(r.risk_score for r in readings)
                        drowsy_count = sum(1 for r in readings if r.risk_score > 50)
                        trip.avg_risk = avg_risk
                        trip.max_risk = max_risk
                        trip.drowsiness_events = drowsy_count
                        trip.safety_score = max(0.0, 100.0 - avg_risk * 0.8)
                    db.commit()
                del current_trip_ids[hub_id]
        finally:
            db.close()


mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

try:
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()
except Exception as e:
    print(f"MQTT connection failed: {e}")


@app.get("/api/v1/health")
def health():
    return {"status": "ok", "mqtt_connected": mqtt_client.is_connected()}


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8000)