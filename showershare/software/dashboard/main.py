from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="ShowerShare prototype", version="0.1.0")

class Event(BaseModel):
    v: int = Field(ge=1, le=1)
    node: str = Field(min_length=1, max_length=48, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=4294967295)
    ts: int = Field(ge=0)
    kind: str = Field(pattern=r"^(door|presence_band|surface_temp|flow_pulses|health)$")
    value: float = Field(ge=-1000000, le=1000000)
    unit: str = Field(max_length=12)

@app.get("/health")
def health():
    return {"ok": True}

@app.post("/v1/events")
def ingest(event: Event):
    # Prototype validator only; persistence, authentication and MQTT are not implemented.
    return {"accepted": True, "node": event.node, "seq": event.seq}
