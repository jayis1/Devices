from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Dict
app=FastAPI(title="GlowRoute local telemetry demo")
class Reading(BaseModel):
    v: int = Field(ge=1, le=1)
    node: int = Field(ge=1, le=255)
    seq: int = Field(ge=0, le=4294967295)
    lux: float = Field(ge=0, le=200000)
    uptime_s: int = Field(ge=0)
latest: Dict[int, Reading] = {}
@app.post("/telemetry")
def ingest(r: Reading):
    prev=latest.get(r.node)
    if prev and r.seq <= prev.seq: raise HTTPException(409,"duplicate or stale sequence")
    latest[r.node]=r
    return {"accepted":True}
@app.get("/telemetry")
def get_telemetry(): return [r.model_dump() for r in latest.values()]
