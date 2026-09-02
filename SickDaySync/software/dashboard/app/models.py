from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Dict, List, Literal, Optional

from pydantic import BaseModel, Field


class TelemetryEvent(BaseModel):
    patient_id: str = Field(..., min_length=1)
    node: Literal["hub", "recovery-band", "room-sentinel", "med-station", "vent-controller"]
    room_id: str = Field(..., min_length=1)
    fever_c: float = 36.5
    spo2: float = 98.0
    resting_hr: float = 72.0
    coughs_per_hour: int = 0
    co2_ppm: int = 600
    humidity_pct: float = 45.0
    hydration_ml: int = 0
    dose_taken: bool = False
    symptom_button: bool = False
    timestamp: Optional[datetime] = None


class RecommendationResponse(BaseModel):
    patient_id: str
    risk_level: Literal["green", "amber", "red"]
    summary: str
    actions: List[str]


@dataclass
class EventStore:
    events: Dict[str, List[TelemetryEvent]] = field(default_factory=dict)

    def add(self, event: TelemetryEvent) -> TelemetryEvent:
        if event.timestamp is None:
            event.timestamp = datetime.now(timezone.utc)
        self.events.setdefault(event.patient_id, []).append(event)
        return event

    def list_for_patient(self, patient_id: str) -> List[TelemetryEvent]:
        return self.events.get(patient_id, [])
