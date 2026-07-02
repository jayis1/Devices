"""
JointSync Cloud Backend — FastAPI Application
"""
import os
from datetime import datetime, timedelta
from typing import Optional, List
from uuid import UUID, uuid4

from fastapi import FastAPI, Depends, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.security import OAuth2PasswordBearer, OAuth2PasswordRequestForm
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from sqlalchemy import create_engine, Column, String, Float, Integer, DateTime, ForeignKey, Boolean
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, Session
from passlib.context import CryptContext
from jose import jwt, JWTError
import paho.mqtt.client as mqtt
import asyncio
import json

# ─────────────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────────────

DATABASE_URL = os.getenv("DATABASE_URL", "postgresql://jointsync:jointsync@localhost:5432/jointsync")
SECRET_KEY = os.getenv("SECRET_KEY", "jointsync-dev-secret-change-in-production")
ALGORITHM = "HS256"
ACCESS_TOKEN_EXPIRE_MINUTES = 1440  # 24 hours
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))

# ─────────────────────────────────────────────────────────────────────
# Database
# ─────────────────────────────────────────────────────────────────────

engine = create_engine(DATABASE_URL, pool_preconnect=True)
SessionLocal = sessionmaker(bind=engine, autocommit=False, autoflush=False)
Base = declarative_base()
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")
oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/login")

# ─────────────────────────────────────────────────────────────────────
# Models
# ─────────────────────────────────────────────────────────────────────

class Patient(Base):
    __tablename__ = "patients"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    email = Column(String, unique=True, index=True)
    name = Column(String)
    hashed_password = Column(String)
    birth_date = Column(DateTime)
    diagnosis = Column(String, default="oa")  # 'ra', 'oa', 'psa', 'other'
    created_at = Column(DateTime, default=datetime.utcnow)

class Joint(Base):
    __tablename__ = "joints"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    patient_id = Column(String, ForeignKey("patients.id"))
    joint_type = Column(String)   # 'knee', 'elbow', 'wrist', 'ankle'
    side = Column(String)          # 'left', 'right'
    tag_id = Column(Integer)
    created_at = Column(DateTime, default=datetime.utcnow)

class IMUReading(Base):
    __tablename__ = "imu_readings"
    id = Column(Integer, primary_key=True, autoincrement=True)
    time = Column(DateTime, default=datetime.utcnow, index=True)
    joint_id = Column(String, ForeignKey("joints.id"))
    accel_x = Column(Float)
    accel_y = Column(Float)
    accel_z = Column(Float)
    gyro_x = Column(Float)
    gyro_y = Column(Float)
    gyro_z = Column(Float)
    joint_angle = Column(Float)
    activity = Column(String)

class TempReading(Base):
    __tablename__ = "temp_readings"
    id = Column(Integer, primary_key=True, autoincrement=True)
    time = Column(DateTime, default=datetime.utcnow, index=True)
    joint_id = Column(String, ForeignKey("joints.id"))
    skin_temp = Column(Float)
    ambient_temp = Column(Float)
    bilateral_delta = Column(Float)

class PPGReading(Base):
    __tablename__ = "ppg_readings"
    id = Column(Integer, primary_key=True, autoincrement=True)
    time = Column(DateTime, default=datetime.utcnow, index=True)
    joint_id = Column(String, ForeignKey("joints.id"))
    hr = Column(Integer)
    hrv_ms = Column(Float)
    spo2 = Column(Integer)

class ThermalScan(Base):
    __tablename__ = "thermal_scans"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    joint_id = Column(String, ForeignKey("joints.id"))
    time = Column(DateTime, default=datetime.utcnow)
    thermal_data = Column(String)  # JSON
    max_temp = Column(Float)
    mean_temp = Column(Float)
    thermal_asymmetry = Column(Float)
    swelling_grade = Column(Integer)
    image_path = Column(String, nullable=True)

class FlarePrediction(Base):
    __tablename__ = "flare_predictions"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    patient_id = Column(String, ForeignKey("patients.id"))
    prediction_time = Column(DateTime, default=datetime.utcnow)
    target_date = Column(DateTime)
    risk_score = Column(Float)
    confidence = Column(Float)
    contributing_factors = Column(String)  # JSON

class TherapySession(Base):
    __tablename__ = "therapy_sessions"
    id = Column(String, primary_key=True, default=lambda: str(uuid4()))
    joint_id = Column(String, ForeignKey("joints.id"))
    start_time = Column(DateTime)
    end_time = Column(DateTime, nullable=True)
    mode = Column(String)
    target_mmhg = Column(Integer)
    achieved_mmhg = Column(Integer, nullable=True)
    duration_sec = Column(Integer, default=0)

Base.metadata.create_all(engine)

# ─────────────────────────────────────────────────────────────────────
# Schemas
# ─────────────────────────────────────────────────────────────────────

class PatientBase(BaseModel):
    email: str
    name: str
    birth_date: Optional[datetime] = None
    diagnosis: str = "oa"

class PatientCreate(PatientBase):
    password: str

class PatientResponse(PatientBase):
    id: str
    class Config:
        from_attributes = True

class JointCreate(BaseModel):
    joint_type: str
    side: str
    tag_id: int

class JointResponse(BaseModel):
    id: str
    joint_type: str
    side: str
    tag_id: int
    class Config:
        from_attributes = True

class ROMReading(BaseModel):
    time: datetime
    joint_angle: float

class TempReadingResponse(BaseModel):
    time: datetime
    skin_temp: float
    bilateral_delta: float
    class Config:
        from_attributes = True

class FlarePredictionResponse(BaseModel):
    target_date: datetime
    risk_score: float
    confidence: float
    contributing_factors: dict

class TherapySessionCreate(BaseModel):
    joint_id: str
    mode: str
    target_mmhg: int

class TherapySessionResponse(BaseModel):
    id: str
    joint_id: str
    start_time: datetime
    end_time: Optional[datetime]
    mode: str
    target_mmhg: int
    achieved_mmhg: Optional[int]
    duration_sec: int
    class Config:
        from_attributes = True

class Token(BaseModel):
    access_token: str
    token_type: str

# ─────────────────────────────────────────────────────────────────────
# Auth
# ─────────────────────────────────────────────────────────────────────

def create_access_token(data: dict):
    to_encode = data.copy()
    expire = datetime.utcnow() + timedelta(minutes=ACCESS_TOKEN_EXPIRE_MINUTES)
    to_encode.update({"exp": expire})
    return jwt.encode(to_encode, SECRET_KEY, algorithm=ALGORITHM)

def get_current_user(token: str = Depends(oauth2_scheme), db: Session = Depends(lambda: SessionLocal())):
    try:
        payload = jwt.decode(token, SECRET_KEY, algorithms=[ALGORITHM])
        email = payload.get("sub")
        if email is None:
            raise HTTPException(401, "Invalid token")
        user = db.query(Patient).filter(Patient.email == email).first()
        if user is None:
            raise HTTPException(401, "User not found")
        return user
    except JWTError:
        raise HTTPException(401, "Invalid token")

# ─────────────────────────────────────────────────────────────────────
# FastAPI
# ─────────────────────────────────────────────────────────────────────

app = FastAPI(title="JointSync API", version="1.0.0")

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

@app.post("/api/v1/auth/register", response_model=PatientResponse)
def register(patient: PatientCreate, db: Session = Depends(get_db)):
    if db.query(Patient).filter(Patient.email == patient.email).first():
        raise HTTPException(400, "Email already registered")
    hashed = pwd_context.hash(patient.password)
    db_patient = Patient(
        email=patient.email,
        name=patient.name,
        hashed_password=hashed,
        birth_date=patient.birth_date,
        diagnosis=patient.diagnosis,
    )
    db.add(db_patient)
    db.commit()
    db.refresh(db_patient)
    return db_patient

@app.post("/api/v1/auth/login", response_model=Token)
def login(form: OAuth2PasswordRequestForm = Depends(), db: Session = Depends(get_db)):
    user = db.query(Patient).filter(Patient.email == form.username).first()
    if not user or not pwd_context.verify(form.password, user.hashed_password):
        raise HTTPException(401, "Invalid credentials")
    token = create_access_token({"sub": user.email})
    return {"access_token": token, "token_type": "bearer"}

# ─────────────────────────────────────────────────────────────────────
# Patient Endpoints
# ─────────────────────────────────────────────────────────────────────

@app.get("/api/v1/patients/me", response_model=PatientResponse)
def get_me(user: Patient = Depends(get_current_user)):
    return user

# ─────────────────────────────────────────────────────────────────────
# Joint Endpoints
# ─────────────────────────────────────────────────────────────────────

@app.get("/api/v1/joints", response_model=List[JointResponse])
def list_joints(user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    return db.query(Joint).filter(Joint.patient_id == user.id).all()

@app.post("/api/v1/joints", response_model=JointResponse)
def create_joint(joint: JointCreate, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    db_joint = Joint(patient_id=user.id, **joint.dict())
    db.add(db_joint)
    db.commit()
    db.refresh(db_joint)
    return db_joint

@app.get("/api/v1/joints/{joint_id}/rom", response_model=List[ROMReading])
def get_rom_history(joint_id: str, hours: int = 24, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    since = datetime.utcnow() - timedelta(hours=hours)
    return db.query(IMUReading).filter(
        IMUReading.joint_id == joint_id,
        IMUReading.time >= since
    ).order_by(IMUReading.time).all()

@app.get("/api/v1/joints/{joint_id}/temperature", response_model=List[TempReadingResponse])
def get_temp_history(joint_id: str, hours: int = 24, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    since = datetime.utcnow() - timedelta(hours=hours)
    return db.query(TempReading).filter(
        TempReading.joint_id == joint_id,
        TempReading.time >= since
    ).order_by(TempReading.time).all()

@app.get("/api/v1/joints/{joint_id}/thermal")
def get_thermal_scans(joint_id: str, limit: int = 10, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    scans = db.query(ThermalScan).filter(
        ThermalScan.joint_id == joint_id
    ).order_by(ThermalScan.time.desc()).limit(limit).all()
    return [{"id": s.id, "time": s.time, "max_temp": s.max_temp,
             "mean_temp": s.mean_temp, "asymmetry": s.thermal_asymmetry,
             "swelling_grade": s.swelling_grade} for s in scans]

# ─────────────────────────────────────────────────────────────────────
# Flare Prediction
# ─────────────────────────────────────────────────────────────────────

@app.get("/api/v1/joints/{joint_id}/flare-risk", response_model=FlarePredictionResponse)
def get_flare_risk(joint_id: str, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    prediction = db.query(FlarePrediction).join(Joint).filter(
        Joint.id == joint_id,
        Joint.patient_id == user.id
    ).order_by(FlarePrediction.prediction_time.desc()).first()

    if not prediction:
        return FlarePredictionResponse(
            target_date=datetime.utcnow() + timedelta(days=7),
            risk_score=0.0,
            confidence=0.0,
            contributing_factors={}
        )

    factors = json.loads(prediction.contributing_factors) if prediction.contributing_factors else {}
    return FlarePredictionResponse(
        target_date=prediction.target_date,
        risk_score=prediction.risk_score,
        confidence=prediction.confidence,
        contributing_factors=factors
    )

# ─────────────────────────────────────────────────────────────────────
# Therapy Endpoints
# ─────────────────────────────────────────────────────────────────────

@app.post("/api/v1/therapy/sessions", response_model=TherapySessionResponse)
def create_therapy_session(session: TherapySessionCreate, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    db_session = TherapySession(
        joint_id=session.joint_id,
        start_time=datetime.utcnow(),
        mode=session.mode,
        target_mmhg=session.target_mmhg,
    )
    db.add(db_session)
    db.commit()
    db.refresh(db_session)
    return db_session

@app.get("/api/v1/therapy/sessions", response_model=List[TherapySessionResponse])
def list_therapy_sessions(limit: int = 20, user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    return db.query(TherapySession).join(Joint).filter(
        Joint.patient_id == user.id
    ).order_by(TherapySession.start_time.desc()).limit(limit).all()

# ─────────────────────────────────────────────────────────────────────
# Reports
# ─────────────────────────────────────────────────────────────────────

@app.get("/api/v1/reports/clinical")
def generate_clinical_report(user: Patient = Depends(get_current_user), db: Session = Depends(get_db)):
    """Generate a rheumatologist-ready clinical report."""
    joints = db.query(Joint).filter(Joint.patient_id == user.id).all()

    report = {
        "patient": {"name": user.name, "email": user.email, "diagnosis": user.diagnosis},
        "report_date": datetime.utcnow().isoformat(),
        "joints": [],
    }

    for joint in joints:
        # Get latest ROM
        latest_rom = db.query(IMUReading).filter(
            IMUReading.joint_id == joint.id
        ).order_by(IMUReading.time.desc()).first()

        # Get latest temperature
        latest_temp = db.query(TempReading).filter(
            TempReading.joint_id == joint.id
        ).order_by(TempReading.time.desc()).first()

        # Get latest thermal scan
        latest_scan = db.query(ThermalScan).filter(
            ThermalScan.joint_id == joint.id
        ).order_by(ThermalScan.time.desc()).first()

        # Get flare prediction
        flare = db.query(FlarePrediction).filter(
            FlarePrediction.patient_id == user.id
        ).order_by(FlarePrediction.prediction_time.desc()).first()

        report["joints"].append({
            "type": joint.joint_type,
            "side": joint.side,
            "current_rom": latest_rom.joint_angle if latest_rom else None,
            "current_temp": latest_temp.skin_temp if latest_temp else None,
            "bilateral_delta": latest_temp.bilateral_delta if latest_temp else None,
            "thermal_max": latest_scan.max_temp if latest_scan else None,
            "swelling_grade": latest_scan.swelling_grade if latest_scan else None,
            "flare_risk_7day": flare.risk_score if flare else 0,
        })

    return report

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
        self.active_connections.remove(websocket)

    async def broadcast(self, message: str):
        for conn in self.active_connections:
            await conn.send_text(message)

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

def on_mqtt_connect(client, userdata, flags, rc):
    print(f"MQTT connected with result code {rc}")
    client.subscribe("jointsync/data/#")
    client.subscribe("jointsync/alerts/#")

def on_mqtt_message(client, userdata, msg):
    """Process incoming MQTT messages from Hub."""
    topic = msg.topic
    payload = msg.payload.decode('utf-8', errors='replace')

    if "data/imu" in topic:
        data = json.loads(payload)
        db = SessionLocal()
        try:
            reading = IMUReading(
                joint_id=data.get("joint", "unknown"),
                accel_x=data.get("ax", 0),
                accel_y=data.get("ay", 0),
                accel_z=data.get("az", 0),
                gyro_x=data.get("gx", 0),
                gyro_y=data.get("gy", 0),
                gyro_z=data.get("gz", 0),
                joint_angle=data.get("angle", 0),
            )
            db.add(reading)
            db.commit()
        finally:
            db.close()

    elif "data/temp" in topic:
        data = json.loads(payload)
        db = SessionLocal()
        try:
            reading = TempReading(
                joint_id=data.get("joint", "unknown"),
                skin_temp=data.get("temp", 0),
                bilateral_delta=data.get("delta", 0),
            )
            db.add(reading)
            db.commit()
        finally:
            db.close()

    elif "data/ppg" in topic:
        data = json.loads(payload)
        db = SessionLocal()
        try:
            reading = PPGReading(
                joint_id=data.get("joint", "unknown"),
                hr=data.get("hr", 0),
                hrv_ms=data.get("hrv", 0),
                spo2=data.get("spo2", 0),
            )
            db.add(reading)
            db.commit()
        finally:
            db.close()

    elif "alerts" in topic:
        # Broadcast to WebSocket clients
        asyncio.run_coroutine_threadsafe(
            manager.broadcast(payload),
            asyncio.get_event_loop()
        )

mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

@app.on_event("startup")
def startup_event():
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
        print("MQTT client started")
    except Exception as e:
        print(f"MQTT connection failed: {e}")

@app.on_event("shutdown")
def shutdown_event():
    mqtt_client.loop_stop()
    mqtt_client.disconnect()

# ─────────────────────────────────────────────────────────────────────
# Health
# ─────────────────────────────────────────────────────────────────────

@app.get("/health")
def health():
    return {"status": "ok", "service": "jointsync-api", "version": "1.0.0"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)