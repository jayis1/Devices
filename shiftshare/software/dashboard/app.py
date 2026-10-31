from fastapi import FastAPI
from pydantic import BaseModel, Field

app = FastAPI(title="ShiftShare prototype", version="0.1.0")

class Observation(BaseModel):
    v: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=4294967295)
    kind: str = Field(min_length=1, max_length=32)
    ts_utc: int | None = None
    values: dict[str, int | float | str | bool | None]

@app.get("/health")
def health():
    return {"status": "prototype", "cloud_enabled": False}

@app.post("/v1/observations")
def accept_observation(observation: Observation):
    # Authentication, durable storage, and MQTT transport are not enabled in this scaffold.
    return {"accepted": True, "node_id": observation.node_id, "seq": observation.seq}
