from __future__ import annotations

from datetime import datetime, timezone
from typing import Literal

from pydantic import BaseModel, Field


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


class FlowTelemetry(BaseModel):
    node_id: str
    zone: str
    branch_id: str
    duration_ms: int = Field(ge=0)
    turbulence: float = Field(ge=0)
    vibration_rms: float = Field(ge=0)
    gas_index: float = Field(ge=0)
    leak: bool = False
    temperature_c: float
    ts: datetime = Field(default_factory=utc_now)


class TrapTelemetry(BaseModel):
    node_id: str
    zone: str
    trap_depth_raw: int = Field(ge=0)
    h2s_ppb: float = Field(ge=0)
    humidity_rh: float = Field(ge=0, le=100)
    primer_cycles: int = Field(ge=0)
    water_present: bool = False
    ts: datetime = Field(default_factory=utc_now)


class StackTelemetry(BaseModel):
    node_id: str
    level_mm: float = Field(ge=0)
    diff_pressure_pa: float
    surge_count: int = Field(ge=0)
    battery_mv: int = Field(ge=0)
    ts: datetime = Field(default_factory=utc_now)


class ActuatorTelemetry(BaseModel):
    node_id: str
    position_pct: float = Field(ge=0, le=100)
    target_pct: float = Field(ge=0, le=100)
    motor_current_ma: float = Field(ge=0)
    fault_bits: int = Field(ge=0)
    healthy: bool = True
    ts: datetime = Field(default_factory=utc_now)


class ValveCommand(BaseModel):
    command: Literal['open', 'close', 'hold']
    reason: str = 'manual'


class RiskItem(BaseModel):
    branch_id: str
    risk: float


class TrapRecommendation(BaseModel):
    node_id: str
    action: Literal['none', 'prime', 'inspect']
    volume_ml: int = 0


class BackupForecast(BaseModel):
    risk: float
    state: Literal['green', 'yellow', 'red']
    hours_to_peak: int


class ValveState(BaseModel):
    position: Literal['open', 'closed', 'partial']
    healthy: bool


class Overview(BaseModel):
    backup_forecast: BackupForecast
    valve: ValveState
    top_clog_risks: list[RiskItem]
    trap_recommendations: list[TrapRecommendation]
