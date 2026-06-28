"""
PestSync Backend — SQLAlchemy Models
software/dashboard/models/schemas.py
"""
from sqlalchemy import Column, Integer, String, Float, DateTime, Boolean, ForeignKey, JSON
from sqlalchemy.orm import relationship
from datetime import datetime, timezone

from db import Base


class User(Base):
    __tablename__ = "users"
    id = Column(Integer, primary_key=True, index=True)
    email = Column(String(255), unique=True, nullable=False)
    hashed_password = Column(String(255), nullable=False)
    display_name = Column(String(100))
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    is_active = Column(Boolean, default=True)

    devices = relationship("Device", back_populates="user")


class Device(Base):
    __tablename__ = "devices"
    id = Column(String(64), primary_key=True)  # node_id as hex string
    user_id = Column(Integer, ForeignKey("users.id"))
    name = Column(String(100))
    node_type = Column(String(50))  # hub, sentinel, trap, deterrent
    firmware_version = Column(String(20))
    last_seen = Column(DateTime)
    created_at = Column(DateTime, default=lambda: datetime.now(timezone.utc))
    is_active = Column(Boolean, default=True)
    config = Column(JSON)  # device-specific configuration

    user = relationship("User", back_populates="devices")


class Detection(Base):
    __tablename__ = "detections"
    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(String(64), ForeignKey("devices.id"))
    user_id = Column(Integer, ForeignKey("users.id"))
    timestamp = Column(DateTime, default=lambda: datetime.now(timezone.utc), index=True)
    pest_class = Column(Integer)  # 0-14 or 255
    pest_name = Column(String(50))
    confidence = Column(Float)
    count = Column(Integer, default=1)
    thermal_max_c = Column(Float)
    ir_illumination = Column(Boolean)
    alerts = Column(Integer, default=0)


class TrapEvent(Base):
    __tablename__ = "trap_events"
    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(String(64), ForeignKey("devices.id"))
    user_id = Column(Integer, ForeignKey("users.id"))
    timestamp = Column(DateTime, default=lambda: datetime.now(timezone.utc), index=True)
    trap_status = Column(Integer)
    catch_weight_g = Column(Integer)
    catch_class = Column(Integer)
    bait_level = Column(Integer)
    battery_pct = Column(Integer)
    alerts = Column(Integer, default=0)


class DeterrentStatus(Base):
    __tablename__ = "deterrent_status"
    id = Column(Integer, primary_key=True, autoincrement=True)
    device_id = Column(String(64), ForeignKey("devices.id"))
    user_id = Column(Integer, ForeignKey("users.id"))
    timestamp = Column(DateTime, default=lambda: datetime.now(timezone.utc), index=True)
    ultrasonic_active = Column(Boolean)
    strobe_active = Column(Boolean)
    diffuser_active = Column(Boolean)
    oil_level = Column(Integer)
    total_ultrasonic_s = Column(Integer)
    diffuser_doses = Column(Integer)
    battery_pct = Column(Integer)
    alerts = Column(Integer, default=0)


class InfestationRisk(Base):
    __tablename__ = "infestation_risk"
    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id"))
    pest_type = Column(String(50))
    risk_score = Column(Float)  # 0-1
    risk_level = Column(String(20))  # low, moderate, high, critical
    forecast_days = Column(Integer, default=30)
    timestamp = Column(DateTime, default=lambda: datetime.now(timezone.utc), index=True)
    recommendation = Column(String(500))


class Alert(Base):
    __tablename__ = "alerts"
    id = Column(Integer, primary_key=True, autoincrement=True)
    user_id = Column(Integer, ForeignKey("users.id"))
    device_id = Column(String(64))
    timestamp = Column(DateTime, default=lambda: datetime.now(timezone.utc), index=True)
    alert_type = Column(String(50))
    severity = Column(Integer)  # 0=info, 1=warning, 2=critical
    title = Column(String(200))
    message = Column(String(500))
    is_read = Column(Boolean, default=False)
    data = Column(JSON)