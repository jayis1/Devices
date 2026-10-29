from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field
from typing import Any

app = FastAPI(title="RainGarden local telemetry", version="0.1.0")
latest: dict[str, dict[str, Any]] = {}

class Telemetry(BaseModel):
    version: int = Field(ge=1, le=1)
    node_id: str = Field(min_length=1, max_length=64)
    seq: int = Field(ge=0, le=4294967295)
    timestamp: str
    kind: str
    values: dict[str, float]

@app.post("/v1/telemetry")
def ingest(report: Telemetry):
    key = report.node_id
    previous = latest.get(key)
    if previous and report.seq <= previous["seq"]:
        raise HTTPException(409, "duplicate or out-of-order sequence")
    latest[key] = report.model_dump()
    return {"accepted": True}

@app.get("/v1/nodes")
def nodes():
    return {"nodes": list(latest.values())}
