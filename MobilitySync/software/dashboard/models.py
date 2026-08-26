from pydantic import BaseModel, Field

class WalkerTelemetryIn(BaseModel):
    walker_id: str
    grip_force_n: float = Field(ge=0)
    wheel_speed_rps: float = Field(ge=0)
    slip_score: float = Field(ge=0)
    brake_state: str

class TransferTelemetryIn(BaseModel):
    zone_id: str
    asymmetry_pct: float = Field(ge=0, le=100)
    unload_rate: float = Field(ge=0)
    retries: int = Field(ge=0)
    occupied: bool

class DoorwayTelemetryIn(BaseModel):
    doorway_id: str
    range_m: float = Field(ge=0)
    obstruction: bool
    mode: str

class BandTelemetryIn(BaseModel):
    wearer_id: str
    hr_bpm: int = Field(ge=20, le=240)
    hrv_proxy: int = Field(ge=0)
    fall_confidence: int = Field(ge=0, le=100)
    sos: bool = False

class CommandOut(BaseModel):
    target: str
    action: str
    reason: str
