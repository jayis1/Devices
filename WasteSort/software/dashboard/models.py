from __future__ import annotations

from datetime import datetime
from typing import Literal, Optional

from pydantic import BaseModel, Field

Stream = Literal["unknown", "recycle", "compost", "landfill", "glass", "deposit", "special_dropoff"]


class SortResolveRequest(BaseModel):
    barcode: Optional[str] = None
    rgb_features: list[float] = Field(default_factory=list)
    spectral: list[float] = Field(default_factory=list)
    municipality: str


class SortResolveResponse(BaseModel):
    stream: Stream
    material: str
    confidence: float
    instructions: str


class BinTelemetry(BaseModel):
    bin_id: str
    stream: Stream
    fill_pct: int
    mass_grams: int
    voc_index: int
    temp_c: float
    humidity_pct: float
    battery_mv: int
    updated_at: datetime = Field(default_factory=datetime.utcnow)


class PickupForecast(BaseModel):
    next_pickup: datetime
    overflow_risk: float
    miss_risk: float
    curb_placed: bool


class Recommendation(BaseModel):
    kind: Literal["tip", "reminder", "shopping", "pickup", "maintenance"]
    title: str
    detail: str
    priority: int = 1
