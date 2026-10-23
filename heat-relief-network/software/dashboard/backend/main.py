from datetime import datetime, timezone
from typing import Any
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

app = FastAPI(title="Heat Relief Network API", version="0.1.0")
latest: dict[str, dict[str, Any]] = {}

class Telemetry(BaseModel):
    site_id: str = Field(min_length=1, max_length=64)
    node_id: str = Field(min_length=1, max_length=64)
    sequence: int = Field(ge=0)
    observed_at: datetime
    kind: str = Field(pattern="^(climate|supply|health)$")
    values: dict[str, float | int | bool]

@app.get("/health")
def health():
    return {"status": "ok"}

@app.post("/telemetry")
def ingest(sample: Telemetry):
    if sample.observed_at.tzinfo is None:
        raise HTTPException(422, "observed_at must include timezone")
    key = f"{sample.site_id}/{sample.node_id}"
    previous = latest.get(key)
    if previous and sample.sequence <= previous["sequence"]:
        raise HTTPException(409, "duplicate or stale sequence")
    latest[key] = sample.model_dump(mode="json")
    return {"accepted": True, "received_at": datetime.now(timezone.utc).isoformat()}

@app.get("/sites/{site_id}")
def site(site_id: str):
    readings = [v for k, v in latest.items() if k.startswith(site_id + "/")]
    if not readings:
        raise HTTPException(404, "site has no telemetry")
    return {"site_id": site_id, "readings": readings}
