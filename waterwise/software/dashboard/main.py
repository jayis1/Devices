from datetime import datetime, timezone
from math import isfinite
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="WaterWise local API", version="1.0")
latest = {}
class Reading(BaseModel):
    v: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=23, pattern=r"^[a-zA-Z0-9_-]+$")
    seq: int = Field(ge=0, le=4294967295)
    ts: int = Field(ge=0)
    kind: str
    values: dict[str, float]
    unit: dict[str, str]

@app.post("/telemetry")
def ingest(r: Reading):
    limits = {"flow": ("rate", "L/min", 0, 40), "pipe_temp": ("temp", "C", -30, 125)}
    if r.kind not in limits:
        raise HTTPException(422, "unsupported kind")
    key, unit, low, high = limits[r.kind]
    value = r.values.get(key)
    if value is None or not isfinite(value) or not low <= value <= high or r.unit.get(key) != unit:
        raise HTTPException(422, "invalid value or unit")
    if abs(datetime.now(timezone.utc).timestamp() - r.ts) > 86400:
        raise HTTPException(422, "timestamp outside one-day acceptance window")
    old = latest.get(r.node_id)
    if old and r.seq <= old["seq"]:
        raise HTTPException(409, "duplicate or out-of-order sequence")
    row = r.model_dump()
    latest[r.node_id] = row
    return {"accepted": True, "node_id": r.node_id, "seq": r.seq}

@app.get("/readings")
def readings():
    return list(latest.values())

@app.delete("/readings")
def clear():
    latest.clear()
    return {"deleted": True}
