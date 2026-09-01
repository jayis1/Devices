from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field


class RiskSummary(BaseModel):
    score: float = Field(ge=0.0, le=1.0)
    level: Literal['low', 'medium', 'high']
    reason: str


class Overview(BaseModel):
    readiness_risk: RiskSummary
    lateness_risk: RiskSummary
    exposure_risk: RiskSummary
    theft_risk: RiskSummary
    recommended_actions: list[str]


class EntryTelemetry(BaseModel):
    node_id: str
    required_items: int = Field(ge=0)
    confirmed_items: int = Field(ge=0)
    bag_present: bool
    badge_seen: bool
    keys_seen: bool
    departure_in_minutes: int = Field(ge=0)
    ts: datetime


class BagTelemetry(BaseModel):
    node_id: str
    tamper_score: float = Field(ge=0.0, le=1.0)
    separation_m: float = Field(ge=0.0)
    motion_state: str
    battery_mv: int = Field(ge=0)
    ts: datetime


class MobilityTelemetry(BaseModel):
    node_id: str
    route_minutes: int = Field(ge=0)
    eta_delta_minutes: int
    pm25_ug_m3: float = Field(ge=0.0)
    voc_index: int = Field(ge=0)
    vibration_rms: float = Field(ge=0.0)
    crash_flag: bool
    ts: datetime


class DeskTelemetry(BaseModel):
    node_id: str
    arrival_confirmed: bool
    items_left_behind: int = Field(ge=0)
    bag_present: bool
    laptop_present: bool
    ts: datetime


class CheckIn(BaseModel):
    event: str
    note: str = ''
