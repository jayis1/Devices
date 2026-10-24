from datetime import datetime
from math import isfinite
from typing import Literal
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="PantryLink demo", version="0.1.0")
records: dict[tuple[str, str, int], dict] = {}

class Telemetry(BaseModel):
    site_id: str = Field(min_length=1, max_length=64)
    node_id: str = Field(min_length=1, max_length=64)
    sequence: int = Field(ge=0, le=65535)
    kind: Literal["temperature", "mass", "door", "heartbeat"]
    value: float
    unit: str = Field(max_length=16)
    timestamp: datetime

@app.get("/health")
def health():
    return {"ok": True}

@app.post("/v1/telemetry", status_code=202)
def ingest(item: Telemetry):
    if not isfinite(item.value):
        raise HTTPException(status_code=422, detail="value must be finite")
    limits = {"temperature": (-40, 85), "mass": (0, 50000), "door": (0, 1), "heartbeat": (0, 1)}
    lo, hi = limits[item.kind]
    if not lo <= item.value <= hi:
        raise HTTPException(status_code=422, detail="value outside accepted range")
    key = (item.site_id, item.node_id, item.sequence)
    if key in records:
        raise HTTPException(status_code=409, detail="duplicate sequence")
    records[key] = item.model_dump(mode="json")
    return {"accepted": True}

@app.get("/v1/sites/{site_id}/inventory")
def inventory(site_id: str):
    return {"site_id": site_id, "entries": [], "source": "demo-no-persistent-stock-ledger"}
