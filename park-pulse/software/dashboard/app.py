from datetime import datetime, timezone
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
app = FastAPI(title="ParkPulse demo")
class Telemetry(BaseModel):
    version: int = Field(ge=1, le=1)
    sequence: int = Field(ge=0)
    type: int = Field(ge=1, le=4)
    value: int = Field(ge=0, le=100000000)
    unit: str
    observed_at: datetime
latest = {}
@app.get("/health")
def health(): return {"ok": True}
@app.post("/sites/{site_id}/nodes/{node_id}/telemetry")
def ingest(site_id: str, node_id: str, item: Telemetry):
    key=(site_id,node_id); old=latest.get(key)
    if old and item.sequence <= old.sequence: raise HTTPException(409,"duplicate or stale sequence")
    latest[key]=item; return {"accepted": True}
@app.get("/sites/{site_id}/latest")
def site_latest(site_id: str):
    return {node: item.model_dump(mode="json") for (site,node),item in latest.items() if site==site_id}
