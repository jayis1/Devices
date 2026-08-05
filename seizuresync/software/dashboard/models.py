"""SeizureSync — SQLAlchemy + Pydantic models."""
from datetime import datetime
from typing import Optional, List
from pydantic import BaseModel, Field
from sqlalchemy import Column, Integer, String, Float, DateTime, Boolean, JSON
from sqlalchemy.orm import declarative_base

Base = declarative_base()


# ---- SQLAlchemy ORM models (TimescaleDB hypertables) ----

class Patient(Base):
    __tablename__ = "patients"
    id = Column(String, primary_key=True)          # UUID
    name = Column(String, nullable=False)
    birth_date = Column(DateTime)
    epilepsy_type = Column(String)                   # focal / generalized / unknown
    seizure_frequency = Column(Float)                # per month
    sudep_risk_score = Column(Float, default=0.0)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow)


class SeizureEvent(Base):
    __tablename__ = "seizure_events"
    id = Column(Integer, primary_key=True, autoincrement=True)
    patient_id = Column(String, nullable=False, index=True)
    onset = Column(DateTime, nullable=False)
    duration_s = Column(Integer)
    semiology = Column(String)                        # ILAE class
    severity = Column(Integer)
    confidence = Column(Float)
    recovery_state = Column(String)
    triggers = Column(JSON)                            # list of identified triggers
    raw_signal_ref = Column(String)                    # MinIO object key
    created_at = Column(DateTime, default=datetime.utcnow)


class SignalChunk(Base):
    __tablename__ = "signal_chunks"
    id = Column(Integer, primary_key=True, autoincrement=True)
    patient_id = Column(String, nullable=False, index=True)
    timestamp = Column(DateTime, nullable=False)
    signal_type = Column(String)                        # accel / ppg / eda / bcg / spo2
    sample_rate = Column(Integer)
    data_ref = Column(String)                           # MinIO object key
    duration_s = Column(Integer)


class RiskForecast(Base):
    __tablename__ = "risk_forecasts"
    id = Column(Integer, primary_key=True, autoincrement=True)
    patient_id = Column(String, nullable=False, index=True)
    timestamp = Column(DateTime, nullable=False)
    risk_24h = Column(Float)                             # 0-100
    risk_7d = Column(Float)
    model_version = Column(String)


class SUDEPRisk(Base):
    __tablename__ = "sudep_risk"
    id = Column(Integer, primary_key=True, autoincrement=True)
    patient_id = Column(String, nullable=False, index=True)
    timestamp = Column(DateTime, nullable=False)
    apnea_density = Column(Float)                        # apnea events per night
    prone_episodes = Column(Integer)
    annual_risk_pct = Column(Float)


# ---- Pydantic schemas ----

class PatientCreate(BaseModel):
    name: str
    birth_date: Optional[datetime] = None
    epilepsy_type: Optional[str] = None
    seizure_frequency: Optional[float] = 0.0


class PatientOut(BaseModel):
    id: str
    name: str
    epilepsy_type: Optional[str]
    seizure_frequency: float
    sudep_risk_score: float

    class Config:
        from_attributes = True


class EventCreate(BaseModel):
    onset: datetime
    duration_s: Optional[int] = 0
    semiology: Optional[str] = "unknown"
    severity: Optional[int] = 0
    confidence: Optional[float] = 0.0
    recovery_state: Optional[str] = "active"
    triggers: Optional[List[str]] = []


class EventOut(BaseModel):
    id: int
    patient_id: str
    onset: datetime
    duration_s: int
    semiology: str
    severity: int
    confidence: float
    recovery_state: str
    triggers: Optional[list] = None

    class Config:
        from_attributes = True


class RiskOut(BaseModel):
    patient_id: str
    risk_24h: float
    risk_7d: float
    timestamp: datetime


class SUDEPOut(BaseModel):
    patient_id: str
    annual_risk_pct: float
    apnea_density: float
    prone_episodes: int
    timestamp: datetime