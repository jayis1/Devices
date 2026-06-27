"""
CompostSync Backend — SQLAlchemy Models / Pydantic Schemas
"""
from sqlalchemy import Column, Integer, String, Float, DateTime, Boolean, ForeignKey, JSON
from sqlalchemy.dialects.postgresql import UUID, JSONB
from sqlalchemy.orm import relationship
from datetime import datetime
import uuid
from pydantic import BaseModel
from db import Base


# ============ SQLAlchemy Models ============

class User(Base):
    __tablename__ = "users"
    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    email = Column(String, unique=True, nullable=False)
    name = Column(String)
    created_at = Column(DateTime, default=datetime.utcnow)
    devices = relationship("Device", back_populates="user")


class Device(Base):
    __tablename__ = "devices"
    id = Column(String, primary_key=True)  # e.g., "compost-hub-001"
    user_id = Column(UUID(as_uuid=True), ForeignKey("users.id"))
    name = Column(String, default="My Compost Bin")
    hub_version = Column(String)
    bin_volume_liters = Column(Integer, default=200)
    compost_type = Column(String, default="hot")  # hot, cold, vermicompost
    created_at = Column(DateTime, default=datetime.utcnow)
    last_seen = Column(DateTime)
    user = relationship("User", back_populates="devices")


class TelemetryRecord(Base):
    __tablename__ = "telemetry"
    __table_args__ = {"timescaledb_hypertable": {"time_column": "timestamp"}}

    id = Column(Integer, primary_key=True)
    device_id = Column(String, ForeignKey("devices.id"), nullable=False)
    node_id = Column(String, nullable=False)
    timestamp = Column(DateTime, default=datetime.utcnow, nullable=False)
    uptime_s = Column(Integer)
    battery_pct = Column(Integer)
    temp_c = Column(JSON)        # [t1, t2, t3] or [t1,t2,t3,t4]
    moisture_pct = Column(JSON)   # [m1, m2, m3]
    co2_ppm = Column(Integer)
    methane_ppm = Column(Integer)
    mass_grams = Column(Integer)
    ph = Column(Float, nullable=True)
    vent_position = Column(Integer)
    phase = Column(String)
    alerts = Column(Integer)
    wind_speed_ms = Column(Float, nullable=True)
    wind_dir_deg = Column(Integer, nullable=True)
    rain_mm = Column(Float, nullable=True)
    ambient_temp_c = Column(Float, nullable=True)
    ambient_humidity = Column(Float, nullable=True)
    ambient_pressure = Column(Integer, nullable=True)


class CompostCycle(Base):
    __tablename__ = "compost_cycles"
    id = Column(Integer, primary_key=True)
    device_id = Column(String, ForeignKey("devices.id"), nullable=False)
    start_date = Column(DateTime, default=datetime.utcnow)
    end_date = Column(DateTime, nullable=True)
    start_mass_g = Column(Integer)
    end_mass_g = Column(Integer, nullable=True)
    initial_cn_ratio = Column(Float, nullable=True)
    final_cn_ratio = Column(Float, nullable=True)
    maturity_score = Column(Float, default=0.0)
    phase = Column(String, default="mesophilic")
    days_thermophilic = Column(Integer, default=0)
    turns_count = Column(Integer, default=0)
    total_diverted_kg = Column(Float, default=0.0)


class Alert(Base):
    __tablename__ = "alerts"
    id = Column(Integer, primary_key=True)
    device_id = Column(String, ForeignKey("devices.id"), nullable=False)
    timestamp = Column(DateTime, default=datetime.utcnow)
    alert_type = Column(String, nullable=False)  # methane_high, overheat, etc.
    severity = Column(Integer, default=1)  # 0=info, 1=warning, 2=critical
    message = Column(String)
    data = Column(JSON)
    acknowledged = Column(Boolean, default=False)


# ============ Pydantic Schemas (API) ============

class UserCreate(BaseModel):
    email: str
    name: str | None = None

class UserResponse(BaseModel):
    id: str
    email: str
    name: str | None

class DeviceCreate(BaseModel):
    id: str
    name: str = "My Compost Bin"
    bin_volume_liters: int = 200
    compost_type: str = "hot"

class DeviceResponse(BaseModel):
    id: str
    name: str
    bin_volume_liters: int
    compost_type: str
    last_seen: datetime | None

class TelemetryResponse(BaseModel):
    timestamp: datetime
    node_id: str
    temp_c: list
    moisture_pct: list
    co2_ppm: int
    methane_ppm: int
    mass_grams: int
    ph: float | None
    vent_position: int
    phase: str
    alerts: int

class CompostStatusResponse(BaseModel):
    device_id: str
    phase: str
    maturity_score: float
    cn_ratio: float
    days_to_ready: int
    recommendation: str
    mass_kg: float
    diverted_kg: float

class AlertResponse(BaseModel):
    id: int
    alert_type: str
    severity: int
    message: str
    timestamp: datetime
    acknowledged: bool