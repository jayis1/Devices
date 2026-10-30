from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from datetime import datetime, timezone
from typing import Dict

app = FastAPI(title="ChargeShare prototype")
class Reading(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: int = Field(ge=0, le=4294967295)
    sequence: int = Field(ge=0, le=4294967295)
    uptime_ms: int = Field(ge=0)
    millivolts: int = Field(ge=0, le=65535)
    milliamps: int = Field(ge=0, le=65535)
    observed_at: datetime

latest: Dict[int, Reading] = {}
@app.post("/telemetry")
def ingest(r: Reading):
    old = latest.get(r.node_id)
    if old and r.sequence <= old.sequence:
        raise HTTPException(409, "duplicate or old sequence")
    latest[r.node_id] = r
    return {"accepted": True}
@app.get("/status")
def status():
    now = datetime.now(timezone.utc)
    return [{**r.model_dump(mode="json"), "stale": (now-r.observed_at).total_seconds() > 300}
            for r in latest.values()]
