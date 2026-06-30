"""
AsthmaSync — Pydantic Models (shared schemas)
"""
from pydantic import BaseModel, Field
from datetime import datetime
from typing import Optional


class RiskForecast(BaseModel):
    risk_score: float = Field(..., ge=0, le=100)
    risk_level: str
    confidence: float = Field(..., ge=0, le=1)
    forecast_days: int = 7
    contributing_factors: list[dict]
    trend: str


class TriggerAttribution(BaseModel):
    trigger: str
    contribution_pct: float
    exposure_level: str
    recommendation: str


class AdherenceSummary(BaseModel):
    rescue_count_7d: int
    rescue_count_30d: int
    controller_adherence_pct: float
    last_rescue: Optional[str]
    gina_controlled: bool


class EventLog(BaseModel):
    timestamp: datetime
    event_type: str
    severity: int
    message: str


class ManualEvent(BaseModel):
    event_type: str
    value: Optional[float] = None
    note: Optional[str] = None