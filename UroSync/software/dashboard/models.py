from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field


class VoidTelemetry(BaseModel):
    event_id: int
    volume_ml: int = Field(ge=0)
    peak_flow_ml_min: int = Field(ge=0)
    flow_duration_s: int = Field(ge=0)
    color_index: int = Field(ge=0, le=10)
    leukocyte: int = Field(ge=0, le=3)
    nitrite: int = Field(ge=0, le=1)
    blood: int = Field(ge=0, le=3)
    protein: int = Field(ge=0, le=3)
    ketone: int = Field(ge=0, le=3)
    glucose: int = Field(ge=0, le=3)
    ph_bin: int = Field(ge=4, le=9)
    sg_q1000: int = Field(ge=1000, le=1040)
    occurred_at: datetime | None = None


class MatTelemetry(BaseModel):
    transfer_latency_ms: int
    sway_index: int
    left_load_pct: int
    right_load_pct: int
    left_temp_c: float
    right_temp_c: float
    trip_duration_s: int
    slip_flag: bool


class BottleTelemetry(BaseModel):
    bottle_mass_g: int
    consumed_ml_day: int
    sip_count_day: int
    adherence_score: int = Field(ge=0, le=100)
    reminder_acked: bool


class EnvironmentTelemetry(BaseModel):
    temp_c: float
    humidity_pct: float
    voc_index: int
    leak_detected: bool
    fan_active: bool
    lux: int


class RiskOverview(BaseModel):
    hydration_risk: float
    uti_risk: float
    fall_risk: float
    nocturia_forecast_next_7d: list[float]
    recommendations: list[str]


class TrendRequest(BaseModel):
    voids_per_night: list[int]
    bottle_intake_ml: list[int]
    strip_sg_q1000: list[int]
    room_temp_c: list[float]


class TrendResponse(BaseModel):
    dehydration_risk_24h: float
    uti_risk: float
    stone_adherence_score: float
    next_best_nudge: str


class AlertEvent(BaseModel):
    severity: Literal['info', 'warning', 'urgent']
    kind: Literal['hydration', 'uti', 'fall', 'leak']
    message: str
