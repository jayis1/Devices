"""
CycleGuard Dashboard — Pydantic models for API request/response validation.
"""

from datetime import datetime
from typing import Optional, List
from pydantic import BaseModel, Field


class RideDataModel(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    speed_kmh: float = Field(ge=0, le=120)
    cadence_rpm: int = Field(ge=0, le=200)
    tire_pressure: float = Field(ge=0, le=150)
    gps_lat: float = Field(ge=-90, le=90)
    gps_lon: float = Field(ge=-180, le=180)
    heading: float = Field(ge=0, le=360)
    battery: int = Field(ge=0, le=100)


class HelmetDataModel(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    crash_class: int = Field(ge=0, le=3)
    impact_g: float
    rot_velocity: float
    horn_detected: bool
    siren_detected: bool
    hr: int = Field(ge=0, le=255)
    battery: int = Field(ge=0, le=100)


class LightDataModel(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    braking: bool
    turn_signal: int = Field(ge=0, le=3)
    headlight_pct: int = Field(ge=0, le=100)
    taillight_pct: int = Field(ge=0, le=100)
    ambient_lux: float
    battery: int = Field(ge=0, le=100)


class LockDataModel(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    lock_state: int = Field(ge=0, le=4)
    gps_lat: float = Field(ge=-90, le=90)
    gps_lon: float = Field(ge=-180, le=180)
    tamper_count: int = Field(ge=0)
    load_cell_kg: int = Field(ge=0)
    battery: int = Field(ge=0, le=100)


class CrashEventModel(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    gps_lat: float
    gps_lon: float
    speed_kmh: float
    impact_g: Optional[float] = None
    rot_velocity: Optional[float] = None
    dispatched: bool
    cancelled: bool = False


class TheftEventModel(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    gps_lat: float
    gps_lon: float
    tamper_count: int
    siren_activated: bool


class RouteRequestModel(BaseModel):
    start_lat: float = Field(ge=-90, le=90)
    start_lon: float = Field(ge=-180, le=180)
    end_lat: float = Field(ge=-90, le=90)
    end_lon: float = Field(ge=-180, le=180)


class SafetyForecastModel(BaseModel):
    forecast: List[dict]
    recommended_window: str


class RideReportModel(BaseModel):
    ride_id: int
    distance_km: float
    avg_speed: float
    max_speed: float
    safety_score: float = Field(ge=0, le=100)
    calories: int
    generated: datetime