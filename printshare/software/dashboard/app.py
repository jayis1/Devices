from fastapi import FastAPI
from pydantic import BaseModel, Field

app = FastAPI(title="PrintShare local telemetry")

class Telemetry(BaseModel):
    version: int = Field(ge=1, le=1)
    type: int = Field(ge=1, le=2)
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=65535)
    uptime_s: int = Field(ge=0, le=4294967295)
    state: int = Field(ge=0, le=255)
    humidity_centi_rh: int = Field(ge=0, le=10000)
    temperature_centi_c: int = Field(ge=-5000, le=15000)
    battery_mv: int = Field(ge=0, le=10000)

@app.get("/health")
def health():
    return {"ok": True}

@app.post("/telemetry")
def telemetry(message: Telemetry):
    return {"accepted": True, "node_id": message.node_id, "sequence": message.sequence}
