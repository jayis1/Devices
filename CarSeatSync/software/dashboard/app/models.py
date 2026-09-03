from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Dict, List, Literal, Optional

from pydantic import BaseModel, Field


class TelemetryEvent(BaseModel):
    child_id: str = Field(..., min_length=1)
    vehicle_id: str = Field(..., min_length=1)
    node: Literal['vehicle-hub', 'safelatch-clip', 'cabin-sentinel', 'child-band', 'handoff-beacon']
    ignition_on: bool = False
    child_present: bool = True
    buckle_closed: bool = True
    caregiver_nearby: bool = True
    cabin_temp_c: float = 24.0
    heat_slope_c_per_min: float = 0.0
    strap_tension_n: float = 30.0
    chest_clip_ratio: float = 0.55
    cry_score: float = 0.0
    heart_rate_bpm: int = 100
    skin_temp_c: float = 36.5
    handoff_complete: bool = False
    bag_present: bool = True
    latitude: Optional[float] = None
    longitude: Optional[float] = None
    timestamp: Optional[datetime] = None


class RecommendationResponse(BaseModel):
    child_id: str
    risk_level: Literal['green', 'amber', 'red']
    summary: str
    actions: List[str]


@dataclass
class EventStore:
    events: Dict[str, List[TelemetryEvent]] = field(default_factory=dict)

    def add(self, event: TelemetryEvent) -> TelemetryEvent:
        if event.timestamp is None:
            event.timestamp = datetime.now(timezone.utc)
        self.events.setdefault(event.child_id, []).append(event)
        return event

    def list_for_child(self, child_id: str) -> List[TelemetryEvent]:
        return self.events.get(child_id, [])
