"""
PostureSync Pydantic models for API request/response validation
"""

from pydantic import BaseModel, Field
from typing import Optional, List
from datetime import datetime


class PostureReading(BaseModel):
    user_id: str = "default"
    posture_class: int
    pitch: float
    roll: float
    yaw: float = 0
    score: int
    hr: int = 0
    hrv: int = 0
    spo2: int = 0
    timestamp: Optional[datetime] = None


class SpineAngle(BaseModel):
    pitch: float
    roll: float
    yaw: float
    cervical: float = 0
    thoracic: float = 0
    lumbar: float = 0


class EMGData(BaseModel):
    emg_rms: List[int] = Field(default_factory=lambda: [0] * 8)
    cervical_angle: float = 0
    thoracic_angle: float = 0
    lumbar_angle: float = 0
    asymmetry_pct: int = 0
    fatigue_idx: int = 0


class PostureScore(BaseModel):
    score: int
    posture_class: int
    timestamp: datetime


class RiskForecast(BaseModel):
    risk_score: int
    risk_level: str
    forecast_days: int = 90
    factors: List[str] = Field(default_factory=list)


class SpinalAge(BaseModel):
    spinal_age: int
    chronological_age: int
    delta: int
    interpretation: str


class ScoliosisScreen(BaseModel):
    risk_score: int
    confidence: float
    threshold: float
    recommendation: str


class DeviceInfo(BaseModel):
    node_id: int
    node_type: str
    firmware_version: str = "1.0.0"
    battery: int = 100


class CalibrationRequest(BaseModel):
    node_id: int
    calibration_type: str = "full"


class CorrectionTrigger(BaseModel):
    haptic_pattern: int = 2
    duration_sec: int = 2
    message: str = "Sit up straight!"