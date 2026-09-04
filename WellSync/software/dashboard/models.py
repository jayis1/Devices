from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field

WaterState = Literal["safe", "watch", "treat", "do_not_drink"]
TelemetryKind = Literal["water_quality", "pump_state", "tap_event", "weather"]


class TelemetryEvent(BaseModel):
    node_id: str
    kind: TelemetryKind
    metrics: dict[str, float | int | str | bool]
    timestamp: datetime = Field(default_factory=datetime.utcnow)


class ServiceLogEntry(BaseModel):
    action: str
    performed_by: str
    notes: str = ""
    timestamp: datetime = Field(default_factory=datetime.utcnow)


class RiskScores(BaseModel):
    contamination: float
    pump_failure: float
    dry_well: float
    treatment_integrity: float


class OverviewResponse(BaseModel):
    state: WaterState
    risks: RiskScores
    latest: dict[str, dict]
    advisories: list[str]
    service_log_count: int
