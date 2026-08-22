from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, Field


class RoutineEventIn(BaseModel):
    child_id: str
    event_type: str
    timestamp: datetime
    details: dict[str, Any] = Field(default_factory=dict)


class ReadinessRequest(BaseModel):
    child_id: str
    minutes_to_departure: int
    backpack_present: bool
    lunch_packed: bool
    weather_complexity: float = 0.0
    interventions: int = 0


class ReadinessResponse(BaseModel):
    readiness_score: int
    late_5m_prob: float
    late_10m_prob: float
    late_20m_prob: float


class LunchSafetyRequest(BaseModel):
    child_id: str
    mass_grams: int
    ice_pack_present: bool
    plate_temp_c: float
    ambient_temp_c: float
    minutes_until_lunch: int


class LunchSafetyResponse(BaseModel):
    risk_level: str
    safe_until_minutes: int
    recommended_action: str


class RouteAnomalyRequest(BaseModel):
    child_id: str
    route_minutes: int
    expected_minutes: int
    boarded: bool
    child_present: bool
    backpack_present: bool


class RouteAnomalyResponse(BaseModel):
    anomaly_score: float
    escalation: str
