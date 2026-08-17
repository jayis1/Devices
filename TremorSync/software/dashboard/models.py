"""
TremorSync Dashboard — Pydantic models for API request/response validation.
"""

from datetime import datetime
from typing import Optional, List
from pydantic import BaseModel, Field


class TremorData(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    tremor_class: int = Field(ge=0, le=3)
    tremor_amplitude: float
    bradykinesia: float = Field(ge=0, le=100)
    onoff_state: int = Field(ge=0, le=2)
    hr: int = Field(ge=0, le=255)
    battery: int = Field(ge=0, le=100)


class GaitData(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    stride_length: float
    cadence: float
    freeze_index: float = Field(ge=0, le=1)
    fog_detected: bool
    festination: bool
    battery: int = Field(ge=0, le=100)


class VoiceData(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    speech_class: int = Field(ge=0, le=4)
    hypophonia_score: float = Field(ge=0, le=100)
    f0_mean: float
    f0_std: float
    swallow_event: int = Field(ge=0, le=3)
    battery: int = Field(ge=0, le=100)


class MedData(BaseModel):
    device_id: str
    timestamp: Optional[datetime] = None
    dose_taken: bool
    pill_weight_mg: int
    minutes_since_dose: int
    onoff_state: int = Field(ge=0, le=2)
    battery: int = Field(ge=0, le=100)


class FallRiskResponse(BaseModel):
    fall_risk_score: float = Field(ge=0, le=100)
    threshold: int = 60
    recommendation: str


class ClinicalReport(BaseModel):
    report_type: str
    period: str
    generated: datetime
    tremor: dict
    gait: dict
    voice: dict
    medication: dict
    mds_updrs_iii_estimate: Optional[float] = None


class DevicePair(BaseModel):
    device_type: str
    device_id: str
    mac_address: Optional[str] = None