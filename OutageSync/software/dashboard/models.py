from __future__ import annotations

from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field


class PanelTelemetry(BaseModel):
    node_id: str
    vrms: float
    freq_hz: float
    thd_pct: float
    grid_present: bool
    battery_soc: float = Field(ge=0, le=100)
    reserve_minutes: int = Field(ge=0)
    ts: datetime


class ColdTelemetry(BaseModel):
    node_id: str
    zone: Literal['fridge', 'freezer', 'medicine', 'cooler']
    product_temp_c: float
    ambient_temp_c: float
    door_open_seconds: int = Field(ge=0)
    hold_minutes_remaining: int = Field(ge=0)
    ts: datetime


class OutletTelemetry(BaseModel):
    node_id: str
    label: str
    watts: float = Field(ge=0)
    priority: int = Field(ge=0, le=5)
    relay_enabled: bool
    medical: bool = False
    ts: datetime


class FuelTelemetry(BaseModel):
    node_id: str
    fuel_level_pct: float = Field(ge=0, le=100)
    co2_ppm: int = Field(ge=0)
    enclosure_temp_c: float
    safe_to_start: bool
    runtime_minutes: int = Field(ge=0)
    ts: datetime


class LoadDecision(BaseModel):
    label: str
    action: Literal['keep_on', 'shed', 'cycle', 'manual_review']
    rationale: str


class OutageForecast(BaseModel):
    expected_minutes: int
    confidence: float
    strategy: str


class SystemOverview(BaseModel):
    forecast: OutageForecast
    decisions: list[LoadDecision]
    summary: str
