from fastapi import FastAPI
from pydantic import BaseModel, Field
from time import time

app = FastAPI(title="RepairLoop local telemetry", version="0.1.0")
latest: dict[int, dict] = {}

class Reading(BaseModel):
    version: int = Field(ge=1, le=1)
    node_type: int = Field(ge=1, le=4)
    node_id: int = Field(ge=0, le=65535)
    sequence: int = Field(ge=0, le=65535)
    metric: str = Field(min_length=1, max_length=32)
    value: float
    unit: str = Field(max_length=12)

@app.get("/health")
def health():
    return {"ok": True}

@app.post("/v1/readings")
def ingest(r: Reading):
    latest[r.node_id] = {**r.model_dump(), "received_at": time()}
    return {"accepted": True, "node_id": r.node_id}

@app.get("/v1/nodes")
def nodes():
    now = time()
    return [{**v, "stale": now-v["received_at"] > 120} for v in latest.values()]
