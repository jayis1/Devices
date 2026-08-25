from pydantic import BaseModel


class RoomTelemetryIn(BaseModel):
    room_id: str
    air_temp_c: float
    rh: float
    surface_temp_c: float
    co2_ppm: int
    voc_index: int
    moisture_pf: float


class PlumbingTelemetryIn(BaseModel):
    branch_id: str
    flow_ml_min: float
    leak_signal: float
    cold_pipe_c: float
    room_temp_c: float


class InspectionIn(BaseModel):
    room_id: str
    thermal_delta_c: float
    conductivity_score: float
    spectral_mildew_index: float
