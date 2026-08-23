from __future__ import annotations

from datetime import datetime
from typing import Dict, List, Optional

from pydantic import BaseModel, Field


class TelemetryIn(BaseModel):
    user_id: str
    node: str
    ts: datetime
    metrics: Dict[str, float | int | str]


class SymptomEvent(BaseModel):
    user_id: str
    ts: datetime
    pain_score: int = Field(ge=0, le=10)
    flow_score: int = Field(ge=0, le=10)
    mood_score: Optional[int] = Field(default=None, ge=0, le=10)
    notes: str = ""
    interventions: List[str] = []


class ReliefCommand(BaseModel):
    user_id: str
    mode: str
    target_temp_c: float = Field(ge=37.0, le=43.0)
    duration_min: int = Field(ge=1, le=25)
    haptics: bool = True
