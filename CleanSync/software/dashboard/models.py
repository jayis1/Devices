from __future__ import annotations

from typing import Literal
from pydantic import BaseModel, Field

ResidueClass = Literal["clean", "dust", "soap", "grease", "biofilm", "mildew-risk"]

class DirtTelemetry(BaseModel):
    node_id: str
    room: str
    dust_index: int = Field(ge=0, le=100)
    humidity_pct: float = Field(ge=0, le=100)
    temp_c: float
    traffic_score: int = Field(ge=0, le=100)
    wet_floor_probability: float = Field(ge=0, le=1)
    odor_index: int = Field(ge=0, le=100)
    battery_mv: int

class DockState(BaseModel):
    node_id: str
    clean_tank_ml: int
    dirty_tank_ml: int
    detergent_ml: int
    leak_detected: bool = False
    uv_cycle_ready: bool = True
    current_ma: int

class WandScan(BaseModel):
    node_id: str
    room: str
    surface: str
    residue_class: ResidueClass
    confidence: float = Field(ge=0, le=1)
    fluorescence_score: float = Field(ge=0, le=100)
    image_tag: str

class Recommendation(BaseModel):
    title: str
    detail: str
    priority: int = Field(ge=1, le=5)

class ScheduleRequest(BaseModel):
    quiet_hours_start: str = "21:00"
    quiet_hours_end: str = "07:00"
    available_windows: list[str] = Field(default_factory=lambda: ["09:30", "13:00", "18:30"])
    prefer_mop: bool = True

class ScheduleResponse(BaseModel):
    recommended_window: str
    reason: str
    rooms: list[str]

class OverviewResponse(BaseModel):
    cleanliness_score: int
    active_alerts: list[str]
    rooms: list[dict]
    supply_forecast: dict
    recommendations: list[Recommendation]
