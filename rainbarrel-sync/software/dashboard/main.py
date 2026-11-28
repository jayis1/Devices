from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="RainBarrel Sync prototype", version="0.1.0")
latest: dict[str, dict] = {}

class Telemetry(BaseModel):
    node_id: str = Field(min_length=1, max_length=32, pattern=r"^[a-zA-Z0-9_-]+$")
    kind: str = Field(pattern=r"^(tank_distance|rain_tips|demand_pulses)$")
    value: float = Field(ge=0, le=1000000)
    unit: str = Field(pattern=r"^(mm|tips|pulses)$")
    sequence: int = Field(ge=0)
    observed_at: datetime

@app.post("/api/v1/telemetry")
def ingest(event: Telemetry):
    if event.observed_at.tzinfo is None:
        raise HTTPException(422, "observed_at must include timezone")
    record = event.model_dump()
    record["received_at"] = datetime.now(timezone.utc)
    latest[event.node_id] = record
    return {"accepted": True}

@app.get("/api/v1/status")
def status():
    return {"prototype": True, "nodes": latest}
