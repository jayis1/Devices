from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field

RoutineName = Literal["workday", "school", "gym", "bedtime", "travel", "custom"]
FocusState = Literal["distracted", "focused", "transition-needed", "hyperfocus-risk"]
NudgeKind = Literal["none", "led_shift", "epaper_prompt", "tag_chirp", "haptic", "tone", "push"]


class DepartureRequest(BaseModel):
    routine: RoutineName
    missing_items: int = Field(ge=0, le=8)
    tray_mass_delta: float = Field(ge=0)
    door_open: bool
    minutes_to_deadline: int = Field(ge=0, le=240)
    focus_fragmentation: float = Field(ge=0, le=1)


class DepartureResponse(BaseModel):
    miss_risk: float
    likely_missing_item: str
    nudge: NudgeKind
    detail: str
    confidence: float


class FindItemRequest(BaseModel):
    item_name: str
    last_seen_room: str
    minutes_since_seen: int = Field(ge=0)
    movement_events: int = Field(ge=0)
    doorway_seen: bool


class FindItemResponse(BaseModel):
    item_name: str
    best_room: str
    confidence: float
    search_order: list[str]


class FocusRequest(BaseModel):
    seat_exits: int = Field(ge=0)
    noise_db: float = Field(ge=0)
    session_minutes: int = Field(ge=0)
    co2_proxy: int = Field(ge=0)
    low_light: bool = False


class FocusResponse(BaseModel):
    focus_state: FocusState
    cue: NudgeKind
    explanation: str


class RoutineStatus(BaseModel):
    name: RoutineName
    completion_rate: float
    average_slip_minutes: float
    next_due: datetime


class OverviewResponse(BaseModel):
    household: str
    generated_at: datetime
    active_routine: RoutineName
    departure_risk: float
    focus_state: FocusState
    missing_items: list[str]
    tags_online: int
    recommendations: list[str]
