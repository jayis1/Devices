from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field


class BandTelemetry(BaseModel):
    node_id: str
    movement_count_10m: int = Field(ge=0)
    movement_variability: float = Field(ge=0)
    posture_pct_left: float = Field(ge=0, le=100)
    posture_pct_supine: float = Field(ge=0, le=100)
    skin_temp_c: float
    ehg_activity_index: float = Field(ge=0)
    battery_mv: int
    ts: datetime


class CuffTelemetry(BaseModel):
    node_id: str
    systolic_mmHg: int
    diastolic_mmHg: int
    map_mmHg: int
    pulse_rate_bpm: int
    waveform_quality: float = Field(ge=0, le=1)
    motion_artifact_score: float = Field(ge=0, le=1)
    ts: datetime


class StripTelemetry(BaseModel):
    node_id: str
    protein_level: Literal['negative', 'trace', '1+', '2+', '3+']
    glucose_level: Literal['negative', 'trace', '1+', '2+', '3+']
    ketone_level: Literal['negative', 'trace', 'small', 'moderate', 'large']
    specific_gravity: float
    nitrite_positive: bool
    hydration_bottle_ml: int = Field(ge=0)
    ts: datetime


class PadTelemetry(BaseModel):
    node_id: str
    hours_recorded: float = Field(gt=0)
    supine_minutes: int = Field(ge=0)
    left_side_minutes: int = Field(ge=0)
    respiration_rate: float = Field(gt=0)
    restlessness_index: float = Field(ge=0)
    ts: datetime


class CheckIn(BaseModel):
    symptom: str
    severity: int = Field(ge=0, le=5)
    note: str = ''


class RiskSummary(BaseModel):
    score: int = Field(ge=0, le=100)
    status: str
    rationale: str


class Overview(BaseModel):
    reduced_movement_risk: RiskSummary
    hypertensive_risk: RiskSummary
    supine_sleep_risk: RiskSummary
    hydration_status: str
    alerts: list[str]
    recommended_actions: list[str]
